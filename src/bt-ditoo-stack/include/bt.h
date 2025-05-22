#pragma once

#define BT_STACK_SIZE (3 * configMINIMAL_STACK_SIZE / 2)

void bt_client_task(void* param);