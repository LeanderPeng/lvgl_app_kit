#ifndef DEMO_DEVICE_PROTOCOL_H
#define DEMO_DEVICE_PROTOCOL_H

#include "comm_link.h"
#include <stdbool.h>

/* 命令与线路格式仅属于本示例，不是框架默认协议。 */
enum {
    DEMO_MESSAGE_QUERY = 1,
    DEMO_MESSAGE_TEMPERATURE,
    DEMO_MESSAGE_SET_TARGET,
    DEMO_MESSAGE_TARGET
};
extern const comm_protocol_ops_t demo_device_protocol;
void demo_protocol_make_query(comm_frame_t *frame);
bool demo_protocol_make_temperature(comm_frame_t *frame, int32_t temperature);
bool demo_protocol_read_temperature(const comm_frame_t *frame, int32_t *temperature);
bool demo_protocol_make_set_target(comm_frame_t *frame, int32_t temperature);
bool demo_protocol_read_set_target(const comm_frame_t *frame, int32_t *temperature);
bool demo_protocol_make_target(comm_frame_t *frame, int32_t temperature);
bool demo_protocol_read_target(const comm_frame_t *frame, int32_t *temperature);

#endif
