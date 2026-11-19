#include "demo_model.h"
#include <string.h>

void demo_model_init(demo_model_t *model)
{
    memset(model, 0, sizeof(*model));
}

void demo_model_request_started(demo_model_t *model, demo_request_kind_t kind)
{
    model->request_pending = true;
    model->pending_kind = kind;
    model->failed = false;
    ++model->revision;
}

void demo_model_temperature_received(demo_model_t *model, int32_t temperature)
{
    model->has_temperature = true;
    model->temperature_tenths = temperature;
    model->request_pending = false;
    model->pending_kind = DEMO_REQUEST_NONE;
    model->failed = false;
    ++model->revision;
}

void demo_model_request_failed(demo_model_t *model)
{
    model->request_pending = false;
    model->pending_kind = DEMO_REQUEST_NONE;
    model->failed = true;
    ++model->revision;
}

bool demo_model_target_is_valid(int32_t temperature)
{
    return temperature >= DEMO_TARGET_MIN_TENTHS && temperature <= DEMO_TARGET_MAX_TENTHS;
}

void demo_model_target_received(demo_model_t *model, int32_t temperature)
{
    model->has_target_temperature = true;
    model->target_temperature_tenths = temperature;
    model->request_pending = false;
    model->pending_kind = DEMO_REQUEST_NONE;
    model->failed = false;
    ++model->revision;
}
