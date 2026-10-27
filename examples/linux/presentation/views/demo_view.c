#include "demo_view.h"
#include <stdlib.h>

typedef struct {
    struct demo_view *view;
    demo_view_action_t action;
} button_binding_t;

struct demo_view {
    lv_obj_t *root;
    lv_obj_t *temperature;
    lv_obj_t *status;
    lv_obj_t *revision;
    lv_obj_t *query;
    demo_view_action_fn action;
    void *context;
    button_binding_t buttons[3];
    size_t button_count;
};

static lv_obj_t *create_label(lv_obj_t *parent, const char *text, int x, int y,
                              const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    if (!label) return NULL;
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

static void button_clicked(lv_event_t *event)
{
    const button_binding_t *binding = lv_event_get_user_data(event);
    demo_view_t *view = binding->view;
    if (view->action) view->action(view->context, binding->action);
}

static lv_obj_t *create_button(demo_view_t *view, const char *text, int x,
                               int width, demo_view_action_t action, bool primary)
{
    if (view->button_count >= sizeof(view->buttons) / sizeof(view->buttons[0])) return NULL;
    lv_obj_t *button = lv_button_create(view->root);
    if (!button) return NULL;
    lv_obj_set_pos(button, x, 397);
    lv_obj_set_size(button, width, 48);
    lv_obj_set_style_radius(button, 10, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(primary ? 0x5EE2B0 : 0x263848), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_50, LV_STATE_DISABLED);
    button_binding_t *binding = &view->buttons[view->button_count++];
    binding->view = view;
    binding->action = action;
    lv_obj_add_event_cb(button, button_clicked, LV_EVENT_CLICKED, binding);
    lv_obj_t *caption = create_label(button, text, 0, 0, &lv_font_montserrat_14,
                                     primary ? 0x102D26 : 0xE6EDF4);
    if (!caption) return NULL;
    lv_obj_center(caption);
    return button;
}

demo_view_t *demo_view_create(lv_obj_t *screen, demo_view_kind_t kind,
                              demo_view_action_fn action, void *context)
{
    if (!screen || kind < DEMO_VIEW_HOME || kind > DEMO_VIEW_DETAILS) return NULL;
    demo_view_t *view = calloc(1, sizeof(*view));
    if (!view) return NULL;
    view->action = action;
    view->context = context;
    view->root = lv_obj_create(screen);
    if (!view->root) goto failed;
    lv_obj_set_size(view->root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(view->root, 0, 0);
    lv_obj_set_style_border_width(view->root, 0, 0);
    lv_obj_set_style_radius(view->root, 0, 0);
    lv_obj_set_style_bg_color(view->root, lv_color_hex(0x101B27), 0);
    lv_obj_set_scrollable(view->root, false);
    lv_obj_set_scrollable(screen, false);

    const char *title = kind == DEMO_VIEW_HOME ? "A layered application, running." :
                        kind == DEMO_VIEW_SETTINGS ? "Pages have a clear responsibility." :
                        "One message. Clear boundaries.";
    if (!create_label(view->root, "LVGL / APP KIT", 32, 25, &lv_font_montserrat_14, 0x5EE2B0) ||
        !create_label(view->root, title, 32, 64, &lv_font_montserrat_28, 0xF0F4F8) ||
        !create_label(view->root, "Linux   /   LVGL 9   /   simulated device", 32, 108,
                      &lv_font_montserrat_14, 0xA3B4C4)) goto failed;

    lv_obj_t *card = lv_obj_create(view->root);
    if (!card) goto failed;
    lv_obj_set_pos(card, 32, 154);
    lv_obj_set_size(card, 736, 212);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x192A39), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_pad_all(card, 22, 0);
    lv_obj_set_scrollable(card, false);

    if (kind == DEMO_VIEW_HOME) {
        if (!create_label(card, "DEVICE TEMPERATURE", 0, 0,
                           &lv_font_montserrat_14, 0xA3B4C4)) goto failed;
        view->temperature = create_label(card, "--.- C", 0, 32,
                                          &lv_font_montserrat_28, 0x5EE2B0);
        if (!view->temperature) goto failed;
        view->status = create_label(card, "", 0, 92, &lv_font_montserrat_14, 0xE6EDF4);
        view->revision = create_label(card, "", 0, 130, &lv_font_montserrat_14, 0xA3B4C4);
        view->query = create_button(view, "Query temperature", 32, 220, DEMO_VIEW_QUERY, true);
        if (!view->query ||
            !create_button(view, "Pages", 270, 160, DEMO_VIEW_OPEN_SETTINGS, false) ||
            !create_button(view, "Layers", 448, 160, DEMO_VIEW_OPEN_DETAILS, false)) goto failed;
    } else {
        const char *body = kind == DEMO_VIEW_SETTINGS ?
            "Pages own lifecycle and user intent.\n"
            "Views own widgets, layout and styles.\n\n"
            "Returning to a page reads the latest model." :
            "Device -> transport -> codec -> worker -> model\n\n"
            "Pages render state and submit application commands.\n"
            "The model survives while a page is hidden.";
        if (!create_label(card, body, 0, 0, &lv_font_montserrat_14, 0xC3D0DC)) goto failed;
        view->status = create_label(card, "", 0, 124, &lv_font_montserrat_14, 0xE6EDF4);
        view->revision = create_label(card, "", 0, 148, &lv_font_montserrat_14, 0xA3B4C4);
        if (!create_button(view, "<  Back", 32, 150, DEMO_VIEW_BACK, false) ||
            !create_button(view, "Home", 200, 150, DEMO_VIEW_OPEN_HOME, true)) goto failed;
        if (kind == DEMO_VIEW_SETTINGS &&
            !create_button(view, "Layers", 368, 160, DEMO_VIEW_OPEN_DETAILS, false)) goto failed;
    }
    if (!view->status || !view->revision) goto failed;
    return view;

failed:
    demo_view_destroy(view);
    return NULL;
}

void demo_view_update(demo_view_t *view, const char *temperature,
                      const char *status, const char *revision, bool query_enabled)
{
    if (!view) return;
    if (view->temperature) lv_label_set_text(view->temperature, temperature);
    lv_label_set_text(view->status, status);
    lv_label_set_text(view->revision, revision);
    if (view->query) {
        if (query_enabled) lv_obj_remove_state(view->query, LV_STATE_DISABLED);
        else lv_obj_add_state(view->query, LV_STATE_DISABLED);
    }
}

void demo_view_destroy(demo_view_t *view)
{
    if (!view) return;
    /* 先销毁持有按钮回调上下文的控件，再释放上下文本身。 */
    if (view->root) lv_obj_delete(view->root);
    free(view);
}
