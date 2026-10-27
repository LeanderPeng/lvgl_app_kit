#ifndef DEMO_MODEL_H
#define DEMO_MODEL_H

#include <stdbool.h>
#include <stdint.h>

/* 模型独立于页面存活；仅由 GUI 线程中的应用工作器修改。 */
typedef struct {
    bool has_temperature;
    int32_t temperature_tenths;
    uint32_t revision;
    bool request_pending;
    bool failed;
} demo_model_t;

void demo_model_init(demo_model_t *model);
void demo_model_request_started(demo_model_t *model);
void demo_model_temperature_received(demo_model_t *model, int32_t temperature);
void demo_model_request_failed(demo_model_t *model);

#endif
