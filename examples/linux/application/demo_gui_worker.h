#ifndef DEMO_GUI_WORKER_H
#define DEMO_GUI_WORKER_H

#include "lak/gui_worker.h"
#include "domain/demo_model.h"
#include "demo_service.h"

typedef void (*demo_model_changed_fn)(void *context);
typedef struct {
    lak_gui_worker_t receiver;
    demo_model_t *model;
    demo_service_t *service;
    demo_model_changed_fn changed;
    void *context;
    uint32_t request_started;
} demo_gui_worker_t;

bool demo_gui_worker_init(demo_gui_worker_t *worker, const comm_frame_channel_t *rx,
    demo_model_t *model, demo_service_t *service, demo_model_changed_fn changed, void *context);
bool demo_gui_worker_query(demo_gui_worker_t *worker);
/* 仅限 GUI 线程；true 表示本地接纳，目标值仍需等待设备确认。 */
bool demo_gui_worker_set_target_temperature(demo_gui_worker_t *worker, int32_t temperature_tenths);
void demo_gui_worker_check_timeout(demo_gui_worker_t *worker);
void demo_gui_worker_deinit(demo_gui_worker_t *worker);

#endif
