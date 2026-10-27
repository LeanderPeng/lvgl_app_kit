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
