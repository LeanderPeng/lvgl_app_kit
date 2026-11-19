#include "demo_gui_worker.h"
#include "protocol/demo_protocol.h"
#include <stdio.h>

static void changed(demo_gui_worker_t *worker)
{
    if (worker->changed) worker->changed(worker->context);
}

static void receive(void *context, const comm_frame_t *frame)
{
    demo_gui_worker_t *worker = context;
    int32_t temperature;
    if (worker->model->request_pending && worker->model->pending_kind == DEMO_REQUEST_QUERY &&
        demo_protocol_read_temperature(frame, &temperature)) {
        demo_model_temperature_received(worker->model, temperature);
        printf("[worker] Model temperature=%ld tenths, revision=%u\n",
            (long)temperature, worker->model->revision);
    } else if (worker->model->request_pending && worker->model->pending_kind == DEMO_REQUEST_SET_TARGET &&
               demo_protocol_read_target(frame, &temperature) &&
               temperature == worker->model->requested_target_tenths) {
        demo_model_target_received(worker->model, temperature);
        printf("[worker] Target confirmed=%ld tenths, revision=%u\n",
            (long)temperature, worker->model->revision);
    } else {
        /* 不匹配的回复不完成当前请求；仍由原有超时机制结束等待。 */
        fprintf(stderr, "[worker] Unexpected or unmatched device message\n");
        return;
    }
    changed(worker);
}

bool demo_gui_worker_init(demo_gui_worker_t *worker, const comm_frame_channel_t *rx,
    demo_model_t *model, demo_service_t *service, demo_model_changed_fn notify, void *context)
{
    if (!worker || !model || !service) return false;
    worker->model = model;
    worker->service = service;
    worker->changed = notify;
    worker->context = context;
    return lak_gui_worker_init(&worker->receiver, rx, receive, worker, 5, 4);
}

bool demo_gui_worker_query(demo_gui_worker_t *worker)
{
    /* 单请求限制属于本示例业务，不影响框架对其他事务策略的支持。 */
    if (worker->model->request_pending) return false;
    if (!demo_service_query_temperature(worker->service)) {
        demo_model_request_failed(worker->model);
        changed(worker);
        return false;
    }
    demo_model_request_started(worker->model, DEMO_REQUEST_QUERY);
    worker->request_started = lv_tick_get();
    printf("[service] Temperature query queued\n");
    changed(worker);
    return true;
}

bool demo_gui_worker_set_target_temperature(demo_gui_worker_t *worker, int32_t temperature_tenths)
{
    /* 范围和单请求约束属于应用业务，不能只依赖控件的禁用状态。 */
    if (!demo_model_target_is_valid(temperature_tenths) || worker->model->request_pending)
        return false;
    if (!demo_service_set_target_temperature(worker->service, temperature_tenths)) {
        demo_model_request_failed(worker->model);
        changed(worker);
        return false;
    }
    worker->model->requested_target_tenths = temperature_tenths;
    demo_model_request_started(worker->model, DEMO_REQUEST_SET_TARGET);
    worker->request_started = lv_tick_get();
    printf("[service] Set target queued=%ld tenths; awaiting confirmation\n",
        (long)temperature_tenths);
    changed(worker);
    return true;
}

void demo_gui_worker_check_timeout(demo_gui_worker_t *worker)
{
    if (worker->model->request_pending && lv_tick_elaps(worker->request_started) >= 2000) {
        demo_model_request_failed(worker->model);
        fprintf(stderr, "[worker] Temperature request timed out\n");
        changed(worker);
    }
}

void demo_gui_worker_deinit(demo_gui_worker_t *worker)
{
    lak_gui_worker_deinit(&worker->receiver);
}
