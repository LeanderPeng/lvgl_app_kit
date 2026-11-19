#include "demo_service.h"
#include "protocol/demo_protocol.h"

void demo_service_init(demo_service_t *service, const comm_frame_channel_t *tx)
{
    service->tx = tx;
}

bool demo_service_query_temperature(demo_service_t *service)
{
    comm_frame_t frame;
    demo_protocol_make_query(&frame);
    return comm_frame_channel_push(service->tx, &frame, 0) == COMM_FRAME_CHANNEL_OK;
}

bool demo_service_set_target_temperature(demo_service_t *service, int32_t temperature_tenths)
{
    comm_frame_t frame;
    if (!demo_protocol_make_set_target(&frame, temperature_tenths)) return false;
    /* 队列复制帧内容，不保留局部变量 frame 的地址。 */
    return comm_frame_channel_push(service->tx, &frame, 0) == COMM_FRAME_CHANNEL_OK;
}
