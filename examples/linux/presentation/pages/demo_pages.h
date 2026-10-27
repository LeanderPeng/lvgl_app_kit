#ifndef DEMO_PAGES_H
#define DEMO_PAGES_H

#include "domain/demo_model.h"
#include "lak/navigation.h"

enum {
    DEMO_PAGE_HOME = 1,
    DEMO_PAGE_SETTINGS,
    DEMO_PAGE_DETAILS,
    DEMO_EVENT_MODEL_CHANGED = 100
};

typedef struct {
    const demo_model_t *model;
    bool (*query_temperature)(void *context);
    void *command_context;
} demo_pages_context_t;

/* 上下文和只读模型必须存活到导航销毁；应用命令通过接口注入。 */
bool demo_pages_register(lak_nav_t *nav, const demo_pages_context_t *context);
/* 只通知当前页面；历史页面在 before_show 中重新读取完整模型。 */
void demo_pages_notify(lak_nav_t *nav);

#endif
