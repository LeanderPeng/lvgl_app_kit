#include "lak/navigation.h"
#include "lak/comm_bridge.h"
#include "lak/linux_display.h"
#include "comm_frame_channel_pthread.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PAGE_HOME = 1, PAGE_SETTINGS, PAGE_DETAILS, EVENT_MESSAGE = 100 };
static volatile sig_atomic_t stop_requested;
static lak_nav_t navigation;

static void stop_handler(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y,
                        const lv_font_t *font, uint32_t color)
{
    lv_obj_t *object = lv_label_create(parent);
    lv_label_set_text(object, text);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_style_text_font(object, font, 0);
    lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
    return object;
}

static void navigate(lv_event_t *event)
{
    uintptr_t destination = (uintptr_t)lv_event_get_user_data(event);
    lak_nav_result_t result = lak_nav_request(&navigation,
        destination ? LAK_NAV_SINGLE : LAK_NAV_BACK, (lak_page_id_t)destination);
    if (result != LAK_NAV_OK) fprintf(stderr, "Navigation enqueue failed: %d\n", result);
}

static void button(lv_obj_t *parent, const char *text, int x, int y,
                     int width, uint32_t destination, bool primary)
{
    lv_obj_t *object = lv_button_create(parent);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, 48);
    lv_obj_set_style_radius(object, 10, 0);
    lv_obj_set_style_shadow_width(object, 0, 0);
    lv_obj_set_style_bg_color(object, lv_color_hex(primary ? 0x5EE2B0 : 0x263848), 0);
    lv_obj_add_event_cb(object, navigate, LV_EVENT_CLICKED, (void *)(uintptr_t)destination);
    lv_obj_t *caption = lv_label_create(object);
    lv_label_set_text(caption, text);
    lv_obj_set_style_text_color(caption, lv_color_hex(primary ? 0x102D26 : 0xE6EDF4), 0);
    lv_obj_center(caption);
}

static bool create_page(lak_page_t *page, void *context)
{
    (void)context;
    lv_obj_t *screen = page->screen;
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101B27), 0);
    lv_obj_set_scrollable(screen, false);
    label(screen, "LVGL / APP KIT", 32, 25, &lv_font_montserrat_14, 0x5EE2B0);
    const char *title = page->id == PAGE_HOME ? "A foundation for your next UI." :
                        page->id == PAGE_SETTINGS ? "Page lifecycle" : "Protocol belongs to your project.";
    label(screen, title, 32, 64, &lv_font_montserrat_28, 0xF0F4F8);
    label(screen, "Linux   /   LVGL 9.6.0   /   C99", 32, 108, &lv_font_montserrat_14, 0xA3B4C4);

    lv_obj_t *card = lv_obj_create(screen);
    lv_obj_set_pos(card, 32, 154);
    lv_obj_set_size(card, 736, 212);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x192A39), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_pad_all(card, 22, 0);
    lv_obj_set_scrollable(card, false);
    if (page->id == PAGE_HOME) {
        label(card, "Navigation, ready.", 0, 0, &lv_font_montserrat_20, 0xF0F4F8);
        label(card, "Registered pages, bounded history and deferred transitions.", 0, 39,
              &lv_font_montserrat_14, 0xA3B4C4);
        label(card, "Communication, yours to define.", 0, 87, &lv_font_montserrat_20, 0xF0F4F8);
        label(card, "Queues and transport are ready. No wire protocol is installed.", 0, 127,
              &lv_font_montserrat_14, 0xA3B4C4);
        button(screen, "Explore pages  >", 32, 397, 230, PAGE_SETTINGS, true);
        button(screen, "Protocol boundary", 280, 397, 240, PAGE_DETAILS, false);
    } else {
        const char *body = page->id == PAGE_SETTINGS ?
            "create  >  before_show  >  before_hide  >  destroy\n\n"
            "SINGLE / PUSH / REPLACE / RESET / BACK / POP_TO / ROOT\n\n"
            "Returning to a page keeps its existing widgets and local state." :
            "Project codec  <->  stream buffer  <->  UART / TCP\n\n"
            "Communication task  ->  RX queue  ->  GUI timer  ->  page\n\n"
            "No sync bytes, CRC, command IDs or request format are assumed.";
        label(card, body, 0, 0, &lv_font_montserrat_14, 0xC3D0DC);
        button(screen, "<  Back", 32, 397, 150, 0, false);
        button(screen, page->id == PAGE_SETTINGS ? "Protocol boundary  >" : "Home",
               202, 397, 290, page->id == PAGE_SETTINGS ? PAGE_DETAILS : PAGE_HOME, true);
    }
    return true;
}

static void page_event(lak_page_t *page, lak_event_t *event)
{
    if (event->id == EVENT_MESSAGE && event->size == sizeof(comm_frame_t)) {
        const comm_frame_t *frame = event->data;
        printf("GUI page=%u message=%u bytes=%zu\n", page->id, frame->id, frame->payload_length);
        event->consumed = true;
    }
}

int main(int argc, char **argv)
{
    unsigned long iterations = 0;
    const char *snapshot = NULL;
    const char *fb = "/dev/fb0";
    const char *input = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc) iterations = strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--snapshot") && i + 1 < argc) snapshot = argv[++i];
        else if (!strcmp(argv[i], "--fb") && i + 1 < argc) fb = argv[++i];
        else if (!strcmp(argv[i], "--input") && i + 1 < argc) input = argv[++i];
        else {
            fprintf(stderr, "Usage: %s [--frames N] [--snapshot image.ppm] [--fb /dev/fb0] [--input /dev/input/event0]\n", argv[0]);
            return 2;
        }
    }
    signal(SIGINT, stop_handler);
    signal(SIGTERM, stop_handler);
    lv_init();
    lv_display_t *display = lak_linux_display_create(800, 480, fb, input);
    if (!display) {
        fprintf(stderr, "Display initialization failed\n");
        lv_deinit();
        return 1;
    }
    int exit_code = 1;
    comm_frame_queue_pthread_t rx_queue;
    comm_frame_t rx_storage[8];
    comm_frame_channel_t rx;
    lak_comm_bridge_t bridge = {0};
    bool queue_ready = false;
    if (!lak_nav_init(&navigation, display)) goto done;
    for (lak_page_id_t id = PAGE_HOME; id <= PAGE_DETAILS; ++id) {
        lak_page_definition_t definition = {0};
        definition.id = id;
        definition.create = create_page;
        definition.event = page_event;
        if (lak_nav_register(&navigation, &definition) != LAK_NAV_OK) goto done;
    }
    if (lak_nav_request(&navigation, LAK_NAV_RESET, PAGE_HOME) != LAK_NAV_OK ||
        lak_nav_process(&navigation) != LAK_NAV_OK) goto done;
    if (comm_frame_queue_pthread_init(&rx_queue, rx_storage, 8) != COMM_FRAME_QUEUE_PTHREAD_OK) goto done;
    queue_ready = true;
    if (comm_frame_channel_pthread_bind(&rx, &rx_queue) != COMM_FRAME_CHANNEL_OK ||
        !lak_comm_bridge_init(&bridge, &navigation, &rx, EVENT_MESSAGE, 10, 4)) goto done;
    printf("LVGL %d.%d.%d; navigation ready; protocol unbound\n",
           LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    for (unsigned long frame = 0; !stop_requested && lak_linux_display_running(); ++frame) {
        uint32_t delay = lv_timer_handler();
        if (iterations && frame + 1 >= iterations) break;
        lak_linux_sleep_ms(delay > 10 ? 10 : delay < 1 ? 1 : delay);
    }
    exit_code = 0;
    if (snapshot && !lak_linux_display_snapshot(snapshot)) {
        fprintf(stderr, "Snapshot failed (requires HEADLESS backend)\n");
        exit_code = 1;
    }
done:
    lak_comm_bridge_deinit(&bridge);
    if (queue_ready) {
        comm_frame_queue_pthread_close(&rx_queue);
        comm_frame_queue_pthread_destroy(&rx_queue);
    }
    if (navigation.initialized) lak_nav_deinit(&navigation);
    lak_linux_display_destroy(display);
    lv_deinit();
    return exit_code;
}
