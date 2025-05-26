// FreeRTOS
#include "FreeRTOS.h"
#include "queue.h"

// mpack
#include "mpack/mpack.h"

extern xQueueHandle bt_command_queue;
extern xQueueHandle usb_command_queue;

typedef enum : uint8_t {
    CMD_LIST_DEVICE = 0,
    CMD_SELECT_DEVICE,
    CMD_DITOO,
    MPACK = ((uint8_t)(-1)),
} command_type;

typedef struct {
    command_type type;
    uint8_t data[256];
    size_t length;
} command_t;

static inline uint8_t mpack_to_command(const mpack_node_t* node, command_t* cmd) {
    command_type exttype = mpack_node_exttype(*node);

    uint32_t len = mpack_node_data_len(*node);
    const char* data = mpack_node_data(*node);

    switch (exttype) {
        case CMD_LIST_DEVICE:
        case CMD_SELECT_DEVICE:
        case CMD_DITOO:
            cmd->type = exttype;
            memcpy(cmd->data, data, len);
            cmd->length = len;
            break;

        default:
            return 1;
    }

    return 0;
}

static inline size_t command_to_mpack(command_t cmd, char** buf) {
    *buf = malloc(cmd.length + 6);
    mpack_writer_t writer;
    mpack_writer_init(&writer, *buf, cmd.length + 6);

    mpack_write_ext(&writer, cmd.type, cmd.data, cmd.length);
    size_t count = mpack_writer_buffer_used(&writer);

    if (mpack_writer_destroy(&writer) != mpack_ok) {
        return -1;
    }

    return count;
}

static inline void mpack_in_command(const char* buf, size_t size, command_t* cmd) {
    cmd->type = MPACK;
    memcpy(cmd->data, buf, size);
    cmd->length = size;
}

static inline size_t rfcomm_packet_to_mpack(uint8_t* packet, uint16_t size, command_type type, command_t* cmd) {
    char* buf = malloc(size + 6);
    mpack_writer_t writer;
    mpack_writer_init(&writer, buf, size + 6);

    mpack_write_ext(&writer, type, packet, size);
    size_t count = mpack_writer_buffer_used(&writer);

    if (mpack_writer_destroy(&writer) != mpack_ok)
        return 1;

    cmd->type = MPACK;
    memcpy(cmd->data, buf, count);
    cmd->length = count;

    free(buf);

    return 0;
}