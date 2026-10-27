#ifndef DEMO_DEVICE_H
#define DEMO_DEVICE_H

#include "comm_frame_channel.h"
#include <stdbool.h>

/* 模拟设备业务单独存在，可替换为真实设备而不改变应用和页面。 */
typedef struct {
    const comm_frame_channel_t *rx;
    const comm_frame_channel_t *tx;
    int32_t temperature_tenths;
} demo_device_t;

void demo_device_init(demo_device_t *device, const comm_frame_channel_t *rx,
    const comm_frame_channel_t *tx);
bool demo_device_poll(demo_device_t *device);

#endif
