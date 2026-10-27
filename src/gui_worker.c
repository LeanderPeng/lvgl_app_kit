#include "lak/gui_worker.h"

#include <string.h>

static void receive_timer(lv_timer_t *timer)
{
    lak_gui_worker_t *worker = lv_timer_get_user_data(timer);
    lak_gui_worker_process(worker);
}

bool lak_gui_worker_init(lak_gui_worker_t *worker,
    const comm_frame_channel_t *rx, lak_gui_message_fn handler, void *context,
    uint32_t period_ms, size_t budget)
{
    if (!worker || !rx || !rx->context || !rx->push || !rx->pop ||
        !rx->close || !handler ||
        !period_ms || !budget) return false;
    /* 拒绝重复初始化，也防止回调重入时覆盖当前调度状态。 */
    if (worker->timer || worker->dispatching) return false;

    lv_timer_t *timer = lv_timer_create(receive_timer, period_ms, worker);
    if (!timer) return false;

    memset(worker, 0, sizeof(*worker));
    worker->rx = rx;
    worker->timer = timer;
    worker->handler = handler;
    worker->context = context;
    worker->budget = budget;
    worker->last_result = COMM_FRAME_CHANNEL_OK;
    return true;
}

void lak_gui_worker_deinit(lak_gui_worker_t *worker)
{
    /* 正在回调时保持对象有效，由应用在本轮消息处理结束后销毁。 */
    if (!worker || worker->dispatching) return;
    if (worker->timer) lv_timer_delete(worker->timer);
    memset(worker, 0, sizeof(*worker));
}

size_t lak_gui_worker_process(lak_gui_worker_t *worker)
{
    if (!worker || worker->dispatching) return 0;
    if (!worker->timer || !worker->rx || !worker->rx->context ||
        !worker->rx->push || !worker->rx->pop || !worker->rx->close ||
        !worker->handler || !worker->budget) {
        worker->last_result = COMM_FRAME_CHANNEL_INVALID_STATE;
        return 0;
    }

    size_t processed = 0;
    worker->dispatching = true;
    while (processed < worker->budget) {
        comm_frame_t frame;
        worker->last_result = comm_frame_channel_pop(worker->rx, &frame, 0);
        if (worker->last_result != COMM_FRAME_CHANNEL_OK) break;
        worker->handler(worker->context, &frame);
        ++processed;
    }
    worker->dispatching = false;
    return processed;
}
