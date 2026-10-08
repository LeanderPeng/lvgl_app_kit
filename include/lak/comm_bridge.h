#ifndef LAK_COMM_BRIDGE_H
#define LAK_COMM_BRIDGE_H

#include "lak/navigation.h"
#include "comm_frame_channel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lak_nav_t *nav;
    const comm_frame_channel_t *rx;
    lv_timer_t *timer;
    uint32_t event_id;
    size_t budget;
    comm_frame_channel_result_t last_result;
} lak_comm_bridge_t;

/* 在注册并打开首个页面后启动，避免启动消息在订阅者就绪前被消费。 */
bool lak_comm_bridge_init(lak_comm_bridge_t *bridge, lak_nav_t *nav,
    const comm_frame_channel_t *rx, uint32_t event_id, uint32_t period_ms, size_t budget);
void lak_comm_bridge_deinit(lak_comm_bridge_t *bridge);

#ifdef __cplusplus
}
#endif
#endif
