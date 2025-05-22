#pragma once

#define USBD_STACK_SIZE (3 * configMINIMAL_STACK_SIZE / 2) * (CFG_TUSB_DEBUG ? 2 : 1)
#define CDC_STACK_SIZE configMINIMAL_STACK_SIZE

void usb_device_task(void* param);
void cdc_task(void* param);