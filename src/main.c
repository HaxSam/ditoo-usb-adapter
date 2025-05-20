#include <stdio.h>

// FreeRTOS
#include "FreeRTOS.h"
#include "task.h"

// Pico
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

// Priorities of our threads - higher numbers are higher priority
#define BLINK_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)

// Stack sizes of our threads in words (4 bytes)
#define BLINK_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

void blink_task(__unused void *params) {
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

    xTaskCreate(blink_task, "BlinkThread", BLINK_TASK_STACK_SIZE, NULL, BLINK_TASK_PRIORITY, NULL);

    vTaskStartScheduler();

    return 0;
}