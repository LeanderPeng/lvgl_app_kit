#include "demo_pages.h"
#include "presentation/views/demo_view.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    const demo_pages_context_t *context;
    lak_page_t *page;
    demo_view_t *view;
} demo_page_state_t;

static void refresh_page(lak_page_t *page)
{
    demo_page_state_t *state = page->user_data;
    if (!state || !state->view) return;
    const demo_model_t *model = state->context->model;
    char temperature[40];
    char revision[96];
    const char *status;
    if (model->has_temperature) {
        /* 先扩展到 64 位，再取绝对值，避免最小负数取反溢出。 */
        int64_t magnitude = model->temperature_tenths;
        if (magnitude < 0) magnitude = -magnitude;
        snprintf(temperature, sizeof(temperature), "%s%" PRId64 ".%" PRId64 " C",
                 model->temperature_tenths < 0 ? "-" : "", magnitude / 10, magnitude % 10);
    } else {
        snprintf(temperature, sizeof(temperature), "--.- C");
    }
    if (model->request_pending) status = "Waiting for the simulated device...";
    else if (model->failed) status = "Request failed. Query again to retry.";
    else if (model->has_temperature) status = "Temperature received; the model is up to date.";
    else status = "Ready to query the simulated device.";
    snprintf(revision, sizeof(revision), "Model revision: %" PRIu32 "   |   state survives page switches",
             model->revision);
    demo_view_update(state->view, temperature, status, revision, !model->request_pending);
}

static void page_action(void *context, demo_view_action_t action)
{
    demo_page_state_t *state = context;
    lak_nav_action_t navigation_action = LAK_NAV_SINGLE;
    lak_page_id_t destination = LAK_PAGE_NONE;
    if (action == DEMO_VIEW_QUERY) {
        /* 页面只提交用户意图；应用层负责发送请求和更新模型。 */
        if (!state->context->query_temperature(state->context->command_context))
            fprintf(stderr, "Temperature query was not accepted\n");
        refresh_page(state->page);
        return;
    }
    switch (action) {
    case DEMO_VIEW_OPEN_SETTINGS: destination = DEMO_PAGE_SETTINGS; break;
    case DEMO_VIEW_OPEN_DETAILS: destination = DEMO_PAGE_DETAILS; break;
    case DEMO_VIEW_OPEN_HOME: destination = DEMO_PAGE_HOME; break;
    case DEMO_VIEW_BACK: navigation_action = LAK_NAV_BACK; break;
    default: return;
    }
    /* 控件事件中只入队，页面切换由导航定时器在事件结束后完成。 */
    lak_nav_result_t result = lak_nav_request(state->page->nav, navigation_action, destination);
    if (result != LAK_NAV_OK) fprintf(stderr, "Navigation enqueue failed: %d\n", result);
}

static bool create_page(lak_page_t *page, void *context)
{
    demo_page_state_t *state = calloc(1, sizeof(*state));
    if (!state) return false;
    /* 提前交给生命周期管理，后续失败也能由 destroy 统一清理。 */
    page->user_data = state;
    state->context = context;
    state->page = page;
    demo_view_kind_t kind = page->id == DEMO_PAGE_HOME ? DEMO_VIEW_HOME :
                            page->id == DEMO_PAGE_SETTINGS ? DEMO_VIEW_SETTINGS : DEMO_VIEW_DETAILS;
    state->view = demo_view_create(page->screen, kind, page_action, state);
    return state->view != NULL;
}

static void destroy_page(lak_page_t *page)
{
    demo_page_state_t *state = page->user_data;
    if (!state) return;
    demo_view_destroy(state->view);
    free(state);
    page->user_data = NULL;
}

static void page_event(lak_page_t *page, lak_event_t *event)
{
    if (event->id == DEMO_EVENT_MODEL_CHANGED) {
        refresh_page(page);
        event->consumed = true;
    }
}

bool demo_pages_register(lak_nav_t *nav, const demo_pages_context_t *context)
{
    if (!nav || !context || !context->model || !context->query_temperature) return false;
    for (lak_page_id_t id = DEMO_PAGE_HOME; id <= DEMO_PAGE_DETAILS; ++id) {
        lak_page_definition_t definition = {0};
        definition.id = id;
        definition.create = create_page;
        definition.before_show = refresh_page;
        definition.destroy = destroy_page;
        definition.event = page_event;
        definition.context = (void *)context;
        if (lak_nav_register(nav, &definition) != LAK_NAV_OK) return false;
    }
    return true;
}

void demo_pages_notify(lak_nav_t *nav)
{
    if (!lak_nav_current(nav)) return;
    lak_event_t event = {0};
    event.id = DEMO_EVENT_MODEL_CHANGED;
    (void)lak_nav_dispatch(nav, &event, false);
}
