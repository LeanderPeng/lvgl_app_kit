#ifndef LAK_NAVIGATION_H
#define LAK_NAVIGATION_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LAK_NAV_MAX_PAGES
#define LAK_NAV_MAX_PAGES 16
#endif
#ifndef LAK_NAV_MAX_DEPTH
#define LAK_NAV_MAX_DEPTH 8
#endif
#ifndef LAK_NAV_MAX_COMMANDS
#define LAK_NAV_MAX_COMMANDS 8
#endif
#if LAK_NAV_MAX_PAGES < 1 || LAK_NAV_MAX_DEPTH < 1 || LAK_NAV_MAX_COMMANDS < 1
#error "Navigation capacities must be positive"
#endif
#define LAK_PAGE_NONE 0u

typedef uint32_t lak_page_id_t;
typedef struct lak_nav lak_nav_t;
typedef struct lak_page lak_page_t;

typedef struct {
    uint32_t id;
    const void *data;
    size_t size;
    bool consumed;
} lak_event_t;

/* create 只填充已有 screen；失败时也会调用 destroy 清理部分创建的私有资源。 */
typedef struct {
    lak_page_id_t id;
    bool (*create)(lak_page_t *page, void *context);
    void (*before_show)(lak_page_t *page);
    void (*before_hide)(lak_page_t *page);
    void (*destroy)(lak_page_t *page);
    void (*event)(lak_page_t *page, lak_event_t *event);
    void *context;
} lak_page_definition_t;

struct lak_page {
    lak_page_id_t id;
    lv_obj_t *screen;
    void *user_data;
    lak_nav_t *nav;
    const lak_page_definition_t *definition;
};

typedef enum {
    LAK_NAV_SINGLE,
    LAK_NAV_PUSH,
    LAK_NAV_REPLACE,
    LAK_NAV_RESET,
    LAK_NAV_BACK,
    LAK_NAV_POP_TO,
    LAK_NAV_ROOT
} lak_nav_action_t;

typedef enum {
    LAK_NAV_OK,
    LAK_NAV_INVALID,
    LAK_NAV_NOT_FOUND,
    LAK_NAV_FULL,
    LAK_NAV_CREATE_FAILED,
    LAK_NAV_AT_ROOT,
    LAK_NAV_BUSY
} lak_nav_result_t;

typedef void (*lak_event_filter_fn)(void *context, lak_event_t *event);
typedef void (*lak_nav_done_fn)(void *context, lak_nav_result_t result);

typedef struct {
    lak_nav_action_t action;
    lak_page_id_t id;
} lak_nav_command_t;

/* 固定容量，所有 API 仅限 GUI 线程。一个显示器只能绑定一个导航管理器。 */
struct lak_nav {
    lv_display_t *display;
    lv_obj_t *parking_screen;
    lv_timer_t *timer;
    lak_page_definition_t definitions[LAK_NAV_MAX_PAGES];
    size_t definition_count;
    lak_page_t *stack[LAK_NAV_MAX_DEPTH];
    size_t depth;
    lak_nav_command_t commands[LAK_NAV_MAX_COMMANDS];
    size_t command_head;
    size_t command_count;
    lak_nav_result_t last_result;
    lak_event_filter_fn system_filter;
    lak_event_filter_fn overlay_filter;
    void *filter_context;
    lak_nav_done_fn done;
    void *done_context;
    bool busy;
    bool initialized;
};

bool lak_nav_init(lak_nav_t *nav, lv_display_t *display);
lak_nav_result_t lak_nav_register(lak_nav_t *nav, const lak_page_definition_t *definition);
/* 返回值仅表示入队结果；完成结果由 done 回调和 last_result 返回。 */
lak_nav_result_t lak_nav_request(lak_nav_t *nav, lak_nav_action_t action, lak_page_id_t id);
/* 一般由内部定时器调用；不能在 LVGL 对象事件或页面生命周期回调中手动调用。 */
lak_nav_result_t lak_nav_process(lak_nav_t *nav);
/* 依次发送到系统层、浮层、当前页或历史页，消费后立即停止。data 只在调用期有效。 */
bool lak_nav_dispatch(lak_nav_t *nav, lak_event_t *event, bool through_history);
lak_page_t *lak_nav_current(const lak_nav_t *nav);
/* 必须在对象事件回调之外调用，且早于 lv_display_delete。 */
lak_nav_result_t lak_nav_deinit(lak_nav_t *nav);

#ifdef __cplusplus
}
#endif
#endif
