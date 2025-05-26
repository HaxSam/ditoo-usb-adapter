#include <stdio.h>

// FreeRTOS
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

// Pico
#include "pico/stdlib.h"

// Ditoo stack
#include "bt.h"
#include "cmd.h"
#include "dev.h"

xQueueHandle bt_command_queue;

int main(void) {
    stdio_init_all();

    TaskHandle_t bt_handle, usb_handle;
    bt_command_queue = xQueueCreate(10, sizeof(bt_command_t));

    if (bt_command_queue == NULL)
        printf("bt queue coudnt be created\n");

    xTaskCreate(bt_client_task, "bt", BT_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, &bt_handle);
    xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, &usb_handle);
    xTaskCreate(cdc_task, "cdc", CDC_STACK_SIZE, NULL, configMAX_PRIORITIES - 3, NULL);

    vTaskCoreAffinitySet(bt_handle, 1);
    vTaskCoreAffinitySet(usb_handle, 2);

    vTaskStartScheduler();

    return 0;
}