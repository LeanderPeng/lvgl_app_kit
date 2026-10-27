#ifndef LAK_GUI_WORKER_H
#define LAK_GUI_WORKER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"
#include "comm_frame_channel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* frame 仅在本次回调期间有效，需要保留的数据应由应用复制。 */
typedef void (*lak_gui_message_fn)(void *context, const comm_frame_t *frame);

/*
 * 在 GUI 线程中消费接收消息，不依赖页面、导航或产品模型。
 * 首次使用前必须零初始化；所有接口均只能由 GUI 线程调用。
 * 字段由工作器维护，应用只可读取，不可在运行期间直接修改。
 */
typedef struct {
    const comm_frame_channel_t *rx;
    lv_timer_t *timer;
    lak_gui_message_fn handler;
    void *context;
    size_t budget;
    comm_frame_channel_result_t last_result;
    bool dispatching;
} lak_gui_worker_t;

/*
 * 在 lv_init 后调用，创建周期性接收定时器；无需等待任何页面创建。
 * rx、其后端和 context 指向的应用对象必须在工作器使用期间保持有效。
 * handler 必须有效，context 可以为空；period_ms 和 budget 必须大于零。
 * rx 必须是完整绑定的通道，工作器自身不发送消息或关闭通道。
 * 已初始化的工作器必须先反初始化，再重新初始化。
 * 参数无效、重复初始化或定时器创建失败时返回 false。
 */
bool lak_gui_worker_init(lak_gui_worker_t *worker,
    const comm_frame_channel_t *rx, lak_gui_message_fn handler, void *context,
    uint32_t period_ms, size_t budget);

/*
 * 删除定时器并清空工作器，不销毁或关闭接收通道。
 * 不得在本工作器的 handler 内调用；遇到此类调用会直接返回，保持运行。
 * 同一工作器可以重复反初始化，但首次使用前仍必须零初始化。
 */
void lak_gui_worker_deinit(lak_gui_worker_t *worker);

/*
 * 立即以零等待方式处理最多 budget 条消息，返回已调用 handler 的次数。
 * 定时器也使用本接口；页面不存在或正在切换均不影响消息消费。
 * 每次 pop 的结果保存在 last_result，遇到非 OK 结果即结束本轮。
 * 初始化后尚未接收时 last_result 为 OK，不表示已接收到消息。
 * 不得从本工作器的 handler 内重入；重入直接返回零，不改变 last_result。
 * 未初始化时返回零，并将 last_result 设为 INVALID_STATE。
 */
size_t lak_gui_worker_process(lak_gui_worker_t *worker);

#ifdef __cplusplus
}
#endif

#endif
