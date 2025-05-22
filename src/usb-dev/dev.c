#include "dev.h"

#include <stdio.h>

// tinyusb
#include "bsp/board_api.h"
#include "include/tusb_config.h"
#include "tusb.h"

// Pico
#include "pico/stdlib.h"

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