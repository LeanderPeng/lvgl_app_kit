#ifndef DEMO_MODEL_H
#define DEMO_MODEL_H

#include <stdbool.h>
#include <stdint.h>

/* 本示例的目标温度范围，单位为十分之一摄氏度。 */
enum { DEMO_TARGET_MIN_TENTHS = 160, DEMO_TARGET_MAX_TENTHS = 300 };
typedef enum { DEMO_REQUEST_NONE, DEMO_REQUEST_QUERY, DEMO_REQUEST_SET_TARGET } demo_request_kind_t;

/* 模型独立于页面存活；仅由 GUI 线程中的应用工作器修改。 */
typedef struct {
    bool has_temperature;
    int32_t temperature_tenths;
    bool has_target_temperature;
    int32_t target_temperature_tenths;
    int32_t requested_target_tenths;
    uint32_t revision;
    bool request_pending;
    demo_request_kind_t pending_kind;
    bool failed;
} demo_model_t;

void demo_model_init(demo_model_t *model);
void demo_model_request_started(demo_model_t *model, demo_request_kind_t kind);
void demo_model_temperature_received(demo_model_t *model, int32_t temperature);
bool demo_model_target_is_valid(int32_t temperature);
void demo_model_target_received(demo_model_t *model, int32_t temperature);
void demo_model_request_failed(demo_model_t *model);

#endif
