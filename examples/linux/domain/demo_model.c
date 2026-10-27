#include "demo_model.h"
#include <string.h>

void demo_model_init(demo_model_t *model)
{
    memset(model, 0, sizeof(*model));
}

void demo_model_request_started(demo_model_t *model)
{
    model->request_pending = true;
    model->failed = false;
    ++model->revision;
}

void demo_model_temperature_received(demo_model_t *model, int32_t temperature)
{
    model->has_temperature = true;
    model->temperature_tenths = temperature;
    model->request_pending = false;
    model->failed = false;
    ++model->revision;
}

void demo_model_request_failed(demo_model_t *model)
{
    model->request_pending = false;
    model->failed = true;
    ++model->revision;
}
