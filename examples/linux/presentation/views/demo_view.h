#ifndef DEMO_VIEW_H
#define DEMO_VIEW_H

#include <lvgl.h>

typedef enum {
    DEMO_VIEW_HOME,
    DEMO_VIEW_SETTINGS,
    DEMO_VIEW_DETAILS
} demo_view_kind_t;

typedef enum {
    DEMO_VIEW_QUERY,
    DEMO_VIEW_OPEN_SETTINGS,
    DEMO_VIEW_OPEN_DETAILS,
    DEMO_VIEW_BACK,
    DEMO_VIEW_OPEN_HOME
} demo_view_action_t;

typedef struct demo_view demo_view_t;
typedef void (*demo_view_action_fn)(void *context, demo_view_action_t action);

/* 视图只创建控件和上报操作，不解释模型、不发通信消息，也不执行导航。 */
demo_view_t *demo_view_create(lv_obj_t *screen, demo_view_kind_t kind,
                              demo_view_action_fn action, void *context);
/* 文案由页面转换；LVGL 会复制文案，调用后不保留这些字符串指针。 */
void demo_view_update(demo_view_t *view, const char *temperature,
                      const char *status, const char *revision, bool query_enabled);
/* 只销毁视图拥有的子控件；screen 仍由导航框架管理。 */
void demo_view_destroy(demo_view_t *view);

#endif
