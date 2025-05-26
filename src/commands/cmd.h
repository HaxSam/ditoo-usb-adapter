#include "FreeRTOS.h"
#include "queue.h"

typedef enum {
    CMD_BT_SEND_DATA = 0,
} command_type;

typedef struct {
    command_type type;
    uint8_t data[256];
    size_t length;
} bt_command_t;

extern xQueueHandle bt_command_queue;
