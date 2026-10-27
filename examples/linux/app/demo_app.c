#include "demo_app.h"
#include "application/demo_gui_worker.h"
#include "platform/demo_connection.h"
#include "protocol/demo_protocol.h"
#include "simulation/demo_device.h"
#include "presentation/pages/demo_pages.h"
#include "lak/linux_display.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned long iterations;
    const char *snapshot;
    const char *framebuffer;
    const char *input;
    bool fragment;
    bool tour;
} demo_options_t;

/* 组合根拥有模块实例；模块间只通过头文件接口和注入回调协作。 */
typedef struct {
    lv_display_t *display;
    lak_nav_t navigation;
    demo_connection_t connection;
    demo_device_t device;
    demo_model_t model;
    demo_service_t service;
    demo_gui_worker_t worker;
    demo_pages_context_t pages;
    unsigned tour_stage;
    uint32_t tour_time;
    int32_t first_temperature;
} demo_app_t;

static volatile sig_atomic_t stop_requested;

static void stop_handler(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static bool parse_options(int argc, char **argv, demo_options_t *options)
{
    *options = (demo_options_t){0};
    options->framebuffer = "/dev/fb0";
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc) {
            const char *value = argv[++i];
            char *end;
            errno = 0;
            options->iterations = strtoul(value, &end, 10);
            if (!*value || value[0] == '-' || errno || *end) return false;
        } else if (!strcmp(argv[i], "--snapshot") && i + 1 < argc) options->snapshot = argv[++i];
        else if (!strcmp(argv[i], "--fb") && i + 1 < argc) options->framebuffer = argv[++i];
        else if (!strcmp(argv[i], "--input") && i + 1 < argc) options->input = argv[++i];
        else if (!strcmp(argv[i], "--fragment")) options->fragment = true;
        else if (!strcmp(argv[i], "--tour")) options->tour = true;
        else return false;
    }
    return true;
}

static bool query_temperature(void *context)
{
    return demo_gui_worker_query(context);
}

static void model_changed(void *context)
{
    demo_app_t *app = context;
    /* 没有页面时模型也已更新；页面稍后在 before_show 读取完整状态。 */
    demo_pages_notify(&app->navigation);
}

static bool navigate_now(demo_app_t *app, lak_nav_action_t action, lak_page_id_t page)
{
    /* 仅由主循环调用，此处不在 LVGL 事件或页面生命周期回调中。 */
    return lak_nav_request(&app->navigation, action, page) == LAK_NAV_OK &&
           lak_nav_process(&app->navigation) == LAK_NAV_OK;
}

static bool tour_step(demo_app_t *app)
{
    if (app->model.failed || lv_tick_elaps(app->tour_time) > 3000) return false;
    switch (app->tour_stage) {
    case 0:
        if (!app->model.has_temperature) return true;
        printf("[tour] Model received before any page exists\n");
        app->first_temperature = app->model.temperature_tenths;
        if (!navigate_now(app, LAK_NAV_RESET, DEMO_PAGE_HOME)) return false;
        app->tour_stage = 1;
        app->tour_time = lv_tick_get();
        break;
    case 1:
        if (lv_tick_elaps(app->tour_time) < 100) return true;
        if (!navigate_now(app, LAK_NAV_SINGLE, DEMO_PAGE_SETTINGS) ||
            !demo_gui_worker_query(&app->worker)) return false;
        printf("[tour] Settings is active; requesting another temperature\n");
        app->tour_stage = 2;
        app->tour_time = lv_tick_get();
        break;
    case 2:
        if (app->model.request_pending) return true;
        if (app->model.temperature_tenths == app->first_temperature) return false;
        printf("[tour] Model updated while Home was hidden\n");
        app->tour_stage = 3;
        app->tour_time = lv_tick_get();
        break;
    case 3:
        if (lv_tick_elaps(app->tour_time) < 100) return true;
        if (!navigate_now(app, LAK_NAV_BACK, LAK_PAGE_NONE)) return false;
        printf("[tour] Home restored from model; temperature=%ld tenths\n",
            (long)app->model.temperature_tenths);
        app->tour_stage = 4;
        app->tour_time = lv_tick_get();
        break;
    case 4:
        if (lv_tick_elaps(app->tour_time) < 100) return true;
        app->tour_stage = 5;
        puts("[tour] Layered walkthrough complete");
        break;
    default:
        break;
    }
    return true;
}

int demo_app_run(int argc, char **argv)
{
    demo_options_t options;
    if (!parse_options(argc, argv, &options)) {
        fprintf(stderr, "Usage: %s [--frames N] [--snapshot image.ppm] [--fb /dev/fb0] "
            "[--input /dev/input/event0] [--fragment] [--tour]\n", argv[0]);
        return 2;
    }
    stop_requested = 0;
    signal(SIGINT, stop_handler);
    signal(SIGTERM, stop_handler);
    demo_app_t app = {0};
    int exit_code = 1;
    lv_init();
    app.display = lak_linux_display_create(800, 480, options.framebuffer, options.input);
    if (!app.display) goto cleanup;
    if (!demo_connection_init(&app.connection, &demo_device_protocol, options.fragment)) goto cleanup;
    demo_device_init(&app.device, &app.connection.device.rx, &app.connection.device.tx);
    demo_model_init(&app.model);
    demo_service_init(&app.service, &app.connection.host.tx);
    if (!lak_nav_init(&app.navigation, app.display)) goto cleanup;
    app.pages = (demo_pages_context_t){&app.model, query_temperature, &app.worker};
    if (!demo_pages_register(&app.navigation, &app.pages) ||
        !demo_gui_worker_init(&app.worker, &app.connection.host.rx, &app.model,
            &app.service, model_changed, &app)) goto cleanup;
    if (!demo_gui_worker_query(&app.worker)) goto cleanup;
    if (!options.tour && !navigate_now(&app, LAK_NAV_RESET, DEMO_PAGE_HOME)) goto cleanup;
    app.tour_time = lv_tick_get();
    printf("LVGL %d.%d.%d; layered application; local simulated device; I/O: %s\n",
        LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH,
        options.fragment ? "at most 2 bytes per call" : "normal");

    for (unsigned long iteration = 0; !stop_requested && lak_linux_display_running(); ++iteration) {
        if (!demo_connection_poll(&app.connection) || !demo_device_poll(&app.device)) goto cleanup;
        uint32_t delay = lv_timer_handler();
        if (app.worker.receiver.last_result != COMM_FRAME_CHANNEL_OK &&
            app.worker.receiver.last_result != COMM_FRAME_CHANNEL_TIMEOUT) goto cleanup;
        demo_gui_worker_check_timeout(&app.worker);
        if (options.tour) {
            if (!tour_step(&app)) goto cleanup;
            if (app.tour_stage == 5) break;
        }
        if (options.iterations && iteration + 1 >= options.iterations) break;
        lak_linux_sleep_ms(delay > 10 ? 10 : delay < 1 ? 1 : delay);
    }
    if (options.tour && app.tour_stage != 5) {
        fprintf(stderr, "[tour] Stopped before completing the walkthrough\n");
        goto cleanup;
    }
    /* 导航在本轮定时器之后完成时，截图前补一次实际刷新。 */
    lv_refr_now(app.display);
    if (options.snapshot && !lak_linux_display_snapshot(options.snapshot)) {
        fprintf(stderr, "Snapshot failed (requires HEADLESS backend)\n");
        goto cleanup;
    }
    exit_code = 0;

cleanup:
    if (exit_code) fprintf(stderr, "Application stopped with an error\n");
    demo_gui_worker_deinit(&app.worker);
    if (app.navigation.initialized && lak_nav_deinit(&app.navigation) != LAK_NAV_OK) exit_code = 1;
    if (app.connection.initialized && !demo_connection_deinit(&app.connection)) exit_code = 1;
    if (app.display) lak_linux_display_destroy(app.display);
    lv_deinit();
    return exit_code;
}
