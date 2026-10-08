#include "lak/navigation.h"
#include <string.h>

static const lak_page_definition_t *find_definition(lak_nav_t *nav, lak_page_id_t id)
{
    for (size_t i = 0; i < nav->definition_count; ++i)
        if (nav->definitions[i].id == id) return &nav->definitions[i];
    return NULL;
}

lak_page_t *lak_nav_current(const lak_nav_t *nav)
{
    return nav && nav->initialized && nav->depth ? nav->stack[nav->depth - 1] : NULL;
}

static void destroy_page(lak_page_t *page)
{
    if (page->definition->destroy) page->definition->destroy(page);
    lv_obj_delete(page->screen);
    lv_free(page);
}

static void hide_current(lak_nav_t *nav)
{
    lak_page_t *page = lak_nav_current(nav);
    if (page && page->definition->before_hide) page->definition->before_hide(page);
}

static void show_page(lak_page_t *page)
{
    if (page->definition->before_show) page->definition->before_show(page);
    lv_screen_load(page->screen);
}

static lak_page_t *create_page(lak_nav_t *nav, const lak_page_definition_t *definition)
{
    lv_display_t *previous;
    lak_page_t *page = lv_malloc(sizeof(*page));
    if (!page) return NULL;
    memset(page, 0, sizeof(*page));
    page->id = definition->id;
    page->definition = definition;
    page->nav = nav;
    previous = lv_display_get_default();
    lv_display_set_default(nav->display);
    page->screen = lv_obj_create(NULL);
    lv_display_set_default(previous);
    if (!page->screen) {
        lv_free(page);
        return NULL;
    }
    if (!definition->create(page, definition->context)) {
        destroy_page(page);
        return NULL;
    }
    return page;
}

static void pop_to_index(lak_nav_t *nav, size_t index)
{
    size_t old_depth = nav->depth;
    if (index == nav->depth - 1) return;
    hide_current(nav);
    /* 先切离旧活动屏，再释放移除的页面，避免显示器保留悬空指针。 */
    nav->depth = index + 1;
    show_page(nav->stack[index]);
    while (old_depth > index + 1) destroy_page(nav->stack[--old_depth]);
}

static lak_nav_result_t execute(lak_nav_t *nav, lak_nav_command_t command)
{
    const lak_page_definition_t *definition;
    lak_page_t *page;
    lak_page_t *removed[LAK_NAV_MAX_DEPTH];
    size_t removed_count = 0;
    size_t target_depth;
    if (command.action == LAK_NAV_BACK || command.action == LAK_NAV_ROOT) {
        if (nav->depth < 2) return LAK_NAV_AT_ROOT;
        pop_to_index(nav, command.action == LAK_NAV_ROOT ? 0 : nav->depth - 2);
        return LAK_NAV_OK;
    }
    if (command.action == LAK_NAV_SINGLE || command.action == LAK_NAV_POP_TO) {
        for (size_t i = nav->depth; i > 0; --i) {
            if (nav->stack[i - 1]->id == command.id) {
                pop_to_index(nav, i - 1);
                return LAK_NAV_OK;
            }
        }
        if (command.action == LAK_NAV_POP_TO) return LAK_NAV_NOT_FOUND;
    }
    definition = find_definition(nav, command.id);
    if (!definition) return LAK_NAV_NOT_FOUND;
    target_depth = command.action == LAK_NAV_RESET ? 0 : nav->depth;
    if (command.action == LAK_NAV_REPLACE && target_depth) --target_depth;
    if (target_depth >= LAK_NAV_MAX_DEPTH) return LAK_NAV_FULL;

    /* 新页面成功创建后才改变旧栈；创建失败时保留原界面和历史。 */
    page = create_page(nav, definition);
    if (!page) return LAK_NAV_CREATE_FAILED;
    hide_current(nav);
    while (nav->depth > target_depth) removed[removed_count++] = nav->stack[--nav->depth];
    nav->stack[nav->depth++] = page;
    show_page(page);
    for (size_t i = 0; i < removed_count; ++i) destroy_page(removed[i]);
    return LAK_NAV_OK;
}

static void navigation_timer(lv_timer_t *timer)
{
    lak_nav_process(lv_timer_get_user_data(timer));
}

bool lak_nav_init(lak_nav_t *nav, lv_display_t *display)
{
    lv_display_t *previous;
    if (!nav || !display) return false;
    memset(nav, 0, sizeof(*nav));
    nav->display = display;
    previous = lv_display_get_default();
    lv_display_set_default(display);
    nav->parking_screen = lv_obj_create(NULL);
    lv_display_set_default(previous);
    if (!nav->parking_screen) return false;
    nav->timer = lv_timer_create(navigation_timer, 5, nav);
    if (!nav->timer) {
        lv_obj_delete(nav->parking_screen);
        return false;
    }
    nav->initialized = true;
    return true;
}

lak_nav_result_t lak_nav_register(lak_nav_t *nav, const lak_page_definition_t *definition)
{
    if (!nav || !nav->initialized || !definition || !definition->id || !definition->create)
        return LAK_NAV_INVALID;
    if (nav->busy) return LAK_NAV_BUSY;
    if (find_definition(nav, definition->id)) return LAK_NAV_INVALID;
    if (nav->definition_count == LAK_NAV_MAX_PAGES) return LAK_NAV_FULL;
    nav->definitions[nav->definition_count++] = *definition;
    return LAK_NAV_OK;
}

lak_nav_result_t lak_nav_request(lak_nav_t *nav, lak_nav_action_t action, lak_page_id_t id)
{
    size_t tail;
    if (!nav || !nav->initialized || action < LAK_NAV_SINGLE || action > LAK_NAV_ROOT)
        return LAK_NAV_INVALID;
    if (action != LAK_NAV_BACK && action != LAK_NAV_ROOT && !find_definition(nav, id))
        return LAK_NAV_NOT_FOUND;
    if (nav->command_count == LAK_NAV_MAX_COMMANDS) return LAK_NAV_FULL;
    tail = (nav->command_head + nav->command_count) % LAK_NAV_MAX_COMMANDS;
    nav->commands[tail] = (lak_nav_command_t){action, id};
    ++nav->command_count;
    return LAK_NAV_OK;
}

lak_nav_result_t lak_nav_process(lak_nav_t *nav)
{
    lak_nav_command_t command;
    if (!nav || !nav->initialized) return LAK_NAV_INVALID;
    if (nav->busy) return LAK_NAV_BUSY;
    if (!nav->command_count) return LAK_NAV_OK;
    nav->busy = true;
    command = nav->commands[nav->command_head];
    nav->command_head = (nav->command_head + 1) % LAK_NAV_MAX_COMMANDS;
    --nav->command_count;
    nav->last_result = execute(nav, command);
    if (nav->done) nav->done(nav->done_context, nav->last_result);
    nav->busy = false;
    return nav->last_result;
}

bool lak_nav_dispatch(lak_nav_t *nav, lak_event_t *event, bool through_history)
{
    if (!nav || !nav->initialized || !event || nav->busy) return false;
    nav->busy = true;
    if (!event->consumed && nav->system_filter) nav->system_filter(nav->filter_context, event);
    if (!event->consumed && nav->overlay_filter) nav->overlay_filter(nav->filter_context, event);
    for (size_t i = nav->depth; i > 0 && !event->consumed; --i) {
        lak_page_t *page = nav->stack[i - 1];
        if (page->definition->event) page->definition->event(page, event);
        if (!through_history) break;
    }
    nav->busy = false;
    return event->consumed;
}

lak_nav_result_t lak_nav_deinit(lak_nav_t *nav)
{
    if (!nav || !nav->initialized) return LAK_NAV_INVALID;
    if (nav->busy) return LAK_NAV_BUSY;
    nav->busy = true;
    lv_timer_delete(nav->timer);
    hide_current(nav);
    /* 空白屏交还显示器持有，随显示器删除；其余导航资源在这里释放。 */
    lv_screen_load(nav->parking_screen);
    while (nav->depth) destroy_page(nav->stack[--nav->depth]);
    nav->initialized = false;
    nav->busy = false;
    return LAK_NAV_OK;
}
