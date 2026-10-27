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
    if (!demo_protocol_read_temperature(frame, &temperature)) {
        demo_model_request_failed(worker->model);
        fprintf(stderr, "[worker] Unexpected device message\n");
    } else {
        demo_model_temperature_received(worker->model, temperature);
        printf("[worker] Model temperature=%ld tenths, revision=%u\n",
            (long)temperature, worker->model->revision);
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
    demo_model_request_started(worker->model);
    worker->request_started = lv_tick_get();
    printf("[service] Temperature query queued\n");
    changed(worker);
    return true;
}

void demo_gui_worker_check_timeout(demo_gui_worker_t *worker)
{
    if (worker->model->request_pending && lv_tick_elaps(worker->request_started) >= 2000) {
        demo_model_request_failed(worker->model);
        fprintf(stderr, "[worker] Temperature query timed out\n");
        changed(worker);
    }
}

void demo_gui_worker_deinit(demo_gui_worker_t *worker)
{
    lak_gui_worker_deinit(&worker->receiver);
}
