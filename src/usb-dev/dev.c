#include "dev.h"

#include <stdio.h>

#include "cmd.h"
#include "usb_descriptors.h"

// mpack
#include "mpack/mpack.h"

// tinyusb
#include "bsp/board_api.h"
#include "include/tusb_config.h"
#include "tusb.h"

// Pico
#include "pico/stdlib.h"

#define URL "example.tinyusb.org/webusb-serial/index.html"

const tusb_desc_webusb_url_t desc_url = {
    .bLength = 3 + sizeof(URL) - 1,
    .bDescriptorType = 3,  // WEBUSB URL type
    .bScheme = 1,          // 0: http, 1: https
    .url = URL,
};

static bool web_serial_connected = false;

//--------------------------------------------------------------------+
// Main
//--------------------------------------------------------------------+

void usb_device_task(__unused void* param) {
    board_init();

    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO,
    };

    tusb_init(BOARD_TUD_RHPORT, &dev_init);

    if (board_init_after_tusb)
        board_init_after_tusb();

    while (true) {
        tud_task();

        tud_vendor_write_flush();
        tud_cdc_write_flush();
    }
}

void echo_all(const uint8_t buf[], uint32_t count) {
    // echo to web serial
    if (web_serial_connected) {
        tud_vendor_write(buf, count);
        tud_vendor_write_flush();
    }

    // echo to cdc
    if (tud_cdc_connected()) {
        for (uint32_t i = 0; i < count; i++) {
            tud_cdc_write_char(buf[i]);
            if (buf[i] == '\r') {
                tud_cdc_write_char('\n');
            }
        }
        tud_cdc_write_flush();
    }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

void tud_mount_cb(void) {}
void tud_umount_cb() {}

void tud_suspend_cb(bool remote_wakeup_en) {}
void tud_resume_cb(void) {}

//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+

static size_t read_cdc(mpack_tree_t* tree, char* buf, size_t count) {
    if (!tud_cdc_available()) return 0;

    uint32_t step = tud_cdc_read(buf, count);

    tud_cdc_write(buf, step);
    // printf("%.*s\n", step, buf);

    tud_cdc_write_flush();

    return step;
}

#define MAX_NODES 32
#define MAX_SIZE (MAX_NODES * 1024)

void cdc_task(__unused void* param) {
    mpack_tree_t tree;
    mpack_tree_init_stream(&tree, read_cdc, NULL, MAX_SIZE, MAX_NODES);

    while (true) {
        if (!mpack_tree_try_parse(&tree)) {
            if (mpack_tree_error(&tree) != mpack_ok) break;
            continue;
        }

        if (mpack_tree_error(&tree) != mpack_ok)
            break;

        mpack_node_t node = mpack_tree_root(&tree);

        bt_command_t bt_cmd;
        command_type exttype = mpack_node_exttype(node);

        uint32_t len = mpack_node_data_len(node);
        const char* data = mpack_node_data(node);

        switch (exttype) {
            case CMD_BT_SEND_DATA:
                bt_cmd.type = exttype;
                memcpy(bt_cmd.data, data, len);
                bt_cmd.length = len;
                break;

            default:
                break;
        }

        xQueueSend(bt_command_queue, &bt_cmd, 1000);

        mpack_tree_destroy(&tree);
        mpack_tree_init_stream(&tree, read_cdc, NULL, MAX_SIZE, MAX_NODES);

        for (int i = 0; i < len; ++i)
            printf("%02x ", data[i]);
        printf("\n");
    }
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {}
void tud_cdc_rx_cb(uint8_t itf) {}

//--------------------------------------------------------------------+
// USB Vendor
//--------------------------------------------------------------------+

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
    // nothing to with DATA & ACK stage
    if (stage != CONTROL_STAGE_SETUP) return true;

    switch (request->bmRequestType_bit.type) {
        case TUSB_REQ_TYPE_VENDOR:
            switch (request->bRequest) {
                case VENDOR_REQUEST_WEBUSB:
                    // match vendor request in BOS descriptor
                    // Get landing page url
                    return tud_control_xfer(rhport, request, (void*)(uintptr_t)&desc_url, desc_url.bLength);

                case VENDOR_REQUEST_MICROSOFT:
                    if (request->wIndex == 7) {
                        // Get Microsoft OS 2.0 compatible descriptor
                        uint16_t total_len;
                        memcpy(&total_len, desc_ms_os_20 + 8, 2);

                        return tud_control_xfer(rhport, request, (void*)(uintptr_t)desc_ms_os_20, total_len);
                    } else {
                        return false;
                    }

                default:
                    break;
            }
            break;

        case TUSB_REQ_TYPE_CLASS:
            if (request->bRequest == 0x22) {
                // Webserial simulate the CDC_REQUEST_SET_CONTROL_LINE_STATE (0x22) to connect and disconnect.
                web_serial_connected = (request->wValue != 0);

                if (web_serial_connected) {
                    printf("Web serial connected\n");

                    tud_vendor_write_str("\r\nWebUSB interface connected\r\n");
                    tud_vendor_write_flush();
                } else {
                    printf("Web serial disconnected\n");
                }

                // response with status OK
                return tud_control_status(rhport, request);
            }
            break;

        default:
            break;
    }

    // stall unknown request
    return false;
}

void tud_vendor_rx_cb(uint8_t itf, uint8_t const* buffer, uint16_t bufsize) {
    (void)itf;

    echo_all(buffer, bufsize);

    // if using RX buffered is enabled, we need to flush the buffer to make room for new data
    tud_vendor_read_flush();
}