#include "dev.h"

#include <stdio.h>

#include "usb_descriptors.h"

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

void cdc_task(__unused void* param) {
    while (true) {
        while (tud_cdc_available()) {
            uint8_t buf[64];

            uint32_t count = tud_cdc_read(buf, sizeof(buf));

            tud_cdc_write(buf, count);
            printf("%s\n", buf);
        }

        tud_cdc_write_flush();
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