#include "bt.h"

#include <stdio.h>
#include <string.h>

#include "cmd.h"

// bluetooth stack
#include "btstack.h"
#include "btstack_run_loop.h"

// Pico
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

// mpack
#include "mpack/mpack.h"

#define HEARTBEAT_PERIOD_MS 1000

typedef enum {
    IDLE,
    W4_SCAN,
    W4_SCAN_RESULTS,
    W4_SCAN_COMPLETE,
    W4_RFCOMM_CHANNEL,
    WAIT_CMD,
    SEND
} state_t;

static char device_name[31];
static bd_addr_t server_addr;
static bd_addr_type_t server_addr_type;
static hci_con_handle_t connection_handle;
static uint8_t rfcomm_server_channel;

uint8_t data[] = {0x01, 0x04, 0x00, 0x74, 0x10, 0x88, 0x00, 0x02};
static command_t bt_cmd;

static state_t state = IDLE;
static uint16_t rfcomm_cid = 0;
static uint16_t rfcomm_mtu;

// Handler
static btstack_timer_source_t heartbeat;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_context_callback_registration_t handle_sdp_client_query_request;

// Handler methods
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void hci_packet_handler(uint8_t *packet, uint16_t size);
static void handle_start_sdp_client_query(void *context);
static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void rfcomm_packet_handler(uint8_t *packet, uint16_t size);
static void bt_queue_handler();
static void heart_beat_handler(btstack_timer_source_t *ts);

// Helper methods
static bool advertisement_report_contains_device_name(char *search_name, uint8_t *advertisement_report);
static bool is_server_addr_known(bd_addr_t addr);

//--------------------------------------------------------------------+
// Main
//--------------------------------------------------------------------+

void bt_client_task(void *param) {
    UNUSED(param);
    hard_assert(cyw43_arch_init() == PICO_OK);

    l2cap_init();
    rfcomm_init();

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // SDP init
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);

    handle_sdp_client_query_request.callback = &handle_start_sdp_client_query;

    btstack_run_loop_set_timer_handler(&heartbeat, heart_beat_handler);
    btstack_run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(&heartbeat);

    hci_power_control(HCI_POWER_ON);

    while (true) {
        btstack_run_loop_execute();
    }
}

static void start_scan() {
    printf("Starting scanning!\n");
    state = W4_SCAN_RESULTS;
    gap_set_scan_parameters(1, 0x0030, 0x0030);
    gap_start_scan();
}

static void stop_scan() {
    printf("Stop scanning!\n");
    state = W4_SCAN_COMPLETE;
    gap_stop_scan();
}

//--------------------------------------------------------------------+
// Bluetooth handler
//--------------------------------------------------------------------+

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            hci_packet_handler(packet, size);
            break;
        case RFCOMM_DATA_PACKET:
            rfcomm_packet_handler(packet, size);
            break;
        default:
            break;
    }
}

static void hci_packet_handler(uint8_t *packet, uint16_t size) {
    UNUSED(size);

    uint8_t event = hci_event_packet_get_type(packet);

    switch (event) {
        case BTSTACK_EVENT_STATE:
            // BTstack activated, get started
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
            state = W4_SCAN;
            break;

        case GAP_EVENT_ADVERTISING_REPORT:
            if (state != W4_SCAN_RESULTS) return;

            if (!advertisement_report_contains_device_name("DitooPro", packet)) return;
            gap_event_advertising_report_get_address(packet, server_addr);
            server_addr_type = gap_event_advertising_report_get_address_type(packet);
            if (is_server_addr_known(server_addr)) return;

            printf("Found DitooPro on %s!\n", bd_addr_to_str(server_addr));

            {
                size_t size = sizeof(device_name) + BD_ADDR_LEN + 15;
                char *buf = malloc(size);
                mpack_writer_t writer;
                mpack_writer_init(&writer, buf, size);

                mpack_build_map(&writer);
                mpack_write_cstr(&writer, device_name);
                mpack_write_bin(&writer, server_addr, BD_ADDR_LEN);
                mpack_complete_map(&writer);

                size_t count = mpack_writer_buffer_used(&writer);

                if (mpack_writer_destroy(&writer) != mpack_ok) {
                    printf("BT: An error occurred encoding the mpack data!\n");
                    return;
                }

                command_t usb_cmd;
                mpack_in_command(buf, count, &usb_cmd);

                xQueueSend(usb_command_queue, &usb_cmd, 0);

                free(buf);
            }
            break;

        case RFCOMM_EVENT_CHANNEL_OPENED:
            if (rfcomm_event_channel_opened_get_status(packet)) {
                printf("RFCOMM channel open failed, status 0x%02x\n", rfcomm_event_channel_opened_get_status(packet));
                return;
            };
            rfcomm_cid = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
            rfcomm_mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
            printf("RFCOMM channel open succeeded. New RFCOMM Channel ID 0x%02x, max frame size %u\n", rfcomm_cid, rfcomm_mtu);

            xQueueSend(usb_command_queue, &bt_cmd, 0);
            state = WAIT_CMD;
            break;

        case RFCOMM_EVENT_CAN_SEND_NOW:
            rfcomm_send(rfcomm_cid, bt_cmd.data, bt_cmd.length);
            state = WAIT_CMD;
            break;

        case RFCOMM_EVENT_CHANNEL_CLOSED:
            printf("RFCOMM channel closed\n");
            rfcomm_cid = 0;
            btstack_run_loop_remove_timer(&heartbeat);
            break;

        default:
            break;
    }
}

static void handle_start_sdp_client_query(void *context) {
    UNUSED(context);
    state = W4_RFCOMM_CHANNEL;
    sdp_client_query_rfcomm_channel_and_name_for_uuid(&handle_query_rfcomm_event, server_addr, BLUETOOTH_SERVICE_CLASS_SERIAL_PORT);
}

static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    switch (hci_event_packet_get_type(packet)) {
        case SDP_EVENT_QUERY_RFCOMM_SERVICE:
            rfcomm_server_channel = sdp_event_query_rfcomm_service_get_rfcomm_channel(packet);
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            if (sdp_event_query_complete_get_status(packet)) {
                printf("SDP query failed, status 0x%02x\n", sdp_event_query_complete_get_status(packet));
                break;
            }
            if (rfcomm_server_channel == 0) {
                printf("No SPP service found\n");
                break;
            }
            printf("SDP query done, channel 0x%02x.\n", rfcomm_server_channel);
            printf("Connecting to device with addr %s.\n", bd_addr_to_str(server_addr));
            rfcomm_create_channel(packet_handler, server_addr, rfcomm_server_channel, NULL);
            break;
        default:
            break;
    }
}

static void rfcomm_packet_handler(uint8_t *packet, uint16_t size) {
    printf("BT: Data recived (size: %d): '", size);
    for (int i = 0; i < size; ++i)
        printf("%02x ", packet[i]);
    printf("'\n");

    command_t usb_cmd;

    if (rfcomm_packet_to_mpack(packet, size, CMD_DITOO, &usb_cmd)) {
        printf("BT: Writing mpack faild\n");
        return;
    }

    xQueueSend(usb_command_queue, &usb_cmd, 0);
}

static void bt_queue_handler() {
    if (xQueueReceive(bt_command_queue, &bt_cmd, 0) != errQUEUE_EMPTY) {
        switch (bt_cmd.type) {
            case CMD_LIST_DEVICE:
                if (state == W4_SCAN) start_scan();
                break;
            case CMD_SELECT_DEVICE:
                if (bt_cmd.length != BD_ADDR_LEN) break;
                if (W4_SCAN_RESULTS) stop_scan();
                memcpy(server_addr, bt_cmd.data, BD_ADDR_LEN);
                if (rfcomm_cid) rfcomm_disconnect(rfcomm_cid);
                state = W4_SCAN_COMPLETE;
                (void)sdp_client_register_query_callback(&handle_sdp_client_query_request);

            case CMD_DITOO:
                if (state == WAIT_CMD) state = SEND;
                break;
            default:
                break;
        }
    }
}

static void heart_beat_handler(btstack_timer_source_t *ts) {
    bt_queue_handler();

    if (rfcomm_cid && state == SEND) {
        printf("BT: CMD recived\n");
        rfcomm_request_can_send_now_event(rfcomm_cid);
    }

    btstack_run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(ts);
}

//--------------------------------------------------------------------+
// Helper methods
//--------------------------------------------------------------------+

static bool advertisement_report_contains_device_name(char *search_name, uint8_t *advertisement_report) {
    // get advertisement from report event
    const uint8_t *adv_data = gap_event_advertising_report_get_data(advertisement_report);
    uint8_t adv_len = gap_event_advertising_report_get_data_length(advertisement_report);

    device_name[0] = '\0';

    // iterate over advertisement data
    ad_context_t context;
    for (ad_iterator_init(&context, adv_len, adv_data); ad_iterator_has_more(&context); ad_iterator_next(&context)) {
        uint8_t data_type = ad_iterator_get_data_type(&context);
        uint8_t data_size = ad_iterator_get_data_len(&context);
        const uint8_t *data = ad_iterator_get_data(&context);

        switch (data_type) {
            case BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME:
            case BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME:
                memcpy(device_name, data, MIN(data_size, sizeof(device_name) - 1));
                device_name[MIN(data_size, sizeof(device_name) - 1)] = '\0';
                break;

            default:
                break;
        }
    }

    return strncmp(device_name, search_name, strlen(search_name)) == 0;
}

static bool is_server_addr_known(bd_addr_t addr) {
    static uint8_t place = 0;
    static bd_addr_t known_servers[64];

    for (int i = 0; i < place; ++i)
        if (memcmp(known_servers[i], addr, BD_ADDR_LEN) == 0) return true;

    memcpy(known_servers[0], addr, BD_ADDR_LEN);
    ++place;

    return false;
}