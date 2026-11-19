#include "demo_device.h"
#include "protocol/demo_protocol.h"

void demo_device_init(demo_device_t *device, const comm_frame_channel_t *rx,
    const comm_frame_channel_t *tx)
{
    device->rx = rx;
    device->tx = tx;
    device->temperature_tenths = 250;
    device->target_temperature_tenths = 250;
}

bool demo_device_poll(demo_device_t *device)
{
    comm_frame_t request;
    comm_frame_channel_result_t received = comm_frame_channel_pop(device->rx, &request, 0);
    if (received == COMM_FRAME_CHANNEL_TIMEOUT) return true;
    if (received != COMM_FRAME_CHANNEL_OK) return false;
    comm_frame_t response;
    if (request.id == DEMO_MESSAGE_QUERY && !request.payload_length) {
        if (device->temperature_tenths >= 300) device->temperature_tenths = 250;
        ++device->temperature_tenths;
        if (!demo_protocol_make_temperature(&response, device->temperature_tenths)) return false;
    } else {
        int32_t target;
        if (!demo_protocol_read_set_target(&request, &target)) return false;
        device->target_temperature_tenths = target;
        if (!demo_protocol_make_target(&response, target)) return false;
    }
    return comm_frame_channel_push(device->tx, &response, 0) == COMM_FRAME_CHANNEL_OK;
}
