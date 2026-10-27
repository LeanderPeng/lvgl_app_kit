#ifndef DEMO_SERVICE_H
#define DEMO_SERVICE_H

#include "comm_frame_channel.h"
#include <stdbool.h>

typedef struct { const comm_frame_channel_t *tx; } demo_service_t;
void demo_service_init(demo_service_t *service, const comm_frame_channel_t *tx);
/* 成功仅表示请求已进入本地队列，设备回复由工作器另行处理。 */
bool demo_service_query_temperature(demo_service_t *service);

#endif
