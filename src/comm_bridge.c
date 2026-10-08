#include "lak/comm_bridge.h"
#include <string.h>

static void receive_timer(lv_timer_t *timer)
{
    lak_comm_bridge_t *bridge = lv_timer_get_user_data(timer);
    /* 没有活动页时保留队列中的消息，等待界面注册完成。 */
    if (!lak_nav_current(bridge->nav) || bridge->nav->busy) return;
    for (size_t i = 0; i < bridge->budget; ++i) {
        comm_frame_t frame;
        bridge->last_result = comm_frame_channel_pop(bridge->rx, &frame, 0);
        if (bridge->last_result != COMM_FRAME_CHANNEL_OK) break;
        lak_event_t event = {bridge->event_id, &frame, sizeof(frame), false};
        lak_nav_dispatch(bridge->nav, &event, false);
    }
}

bool lak_comm_bridge_init(lak_comm_bridge_t *bridge, lak_nav_t *nav,
    const comm_frame_channel_t *rx, uint32_t event_id, uint32_t period_ms, size_t budget)
{
    if (!bridge || !nav || !nav->initialized || !rx || !rx->context || !rx->pop ||
        !period_ms || !budget) return false;
    memset(bridge, 0, sizeof(*bridge));
    bridge->nav = nav;
    bridge->rx = rx;
    bridge->event_id = event_id;
    bridge->budget = budget;
    bridge->timer = lv_timer_create(receive_timer, period_ms, bridge);
    return bridge->timer != NULL;
}

void lak_comm_bridge_deinit(lak_comm_bridge_t *bridge)
{
    if (!bridge || !bridge->timer) return;
    lv_timer_delete(bridge->timer);
    bridge->timer = NULL;
}
