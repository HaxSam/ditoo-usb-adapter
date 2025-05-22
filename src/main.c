#include <stdio.h>

// FreeRTOS
#include "FreeRTOS.h"
#include "bsp/board_api.h"
#include "task.h"

// Pico
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

// Ditoo stack
#include "bt.h"
#include "dev.h"

// Priorities of our threads - higher numbers are higher priority
#define BLINK_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)

// Stack sizes of our threads in words (4 bytes)
#define BLINK_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

void blink_task(__unused void* params) {
    hard_assert(cyw43_arch_init() == PICO_OK);
    bool on = false;

    while (true) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
        on = !on;
        vTaskDelay(250);
    }
}

int main(void) {
    stdio_init_all();

    TaskHandle_t bt_handle, usb_handle;

    xTaskCreate(bt_client_task, "bt", BT_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, &bt_handle);
    xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, &usb_handle);
    xTaskCreate(cdc_task, "cdc", CDC_STACK_SIZE, NULL, configMAX_PRIORITIES - 3, NULL);

    // vTaskCoreAffinitySet(bt_handle, 2);
    // vTaskCoreAffinitySet(usb_handle, 1);

    // xTaskCreate(usb_run, "BlinkThread", BLINK_TASK_STACK_SIZE, NULL, BLINK_TASK_PRIORITY, NULL);

    vTaskStartScheduler();

    return 0;
}