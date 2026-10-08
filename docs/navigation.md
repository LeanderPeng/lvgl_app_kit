# 页面导航

## 接入顺序

1. `lv_init()`，建立显示器、输入设备和单调 tick；启动 GUI 主循环。
2. GUI 线程调用 `lak_nav_init(&nav, display)`，一个显示器只使用一个导航器。
3. 用 `lak_nav_register()` 注册页面定义；定义会复制到固定表中，`context` 只保存指针。
4. `lak_nav_request(&nav, LAK_NAV_RESET, HOME_ID)` 打开首个页面。
5. 首次初始化阶段可直接调用 `lak_nav_process()`；运行期交给导航定时器处理。
6. 建立通信桥接，再启动通信生产者。

所有 API 均只允许 GUI 线程调用；初始化对象不能重复 init，先 deinit 再重新初始化。注册项和页面 context 必须在全部相关页面销毁前有效。

## 页面定义

```c
static bool home_create(lak_page_t *page, void *context)
{
    (void)context;
    lv_obj_t *label = lv_label_create(page->screen);
    lv_label_set_text(label, "Home");
    lv_obj_center(label);
    return true;
}

lak_page_definition_t home = {0};
home.id = 1;
home.create = home_create;
lak_nav_register(&nav, &home);
```

页面根 `screen` 由框架创建并释放，页面函数仅构建其子控件。不要替换、删除根屏幕，不要直接调用 `lv_screen_load()` 绕过导航。页面私有资源放在 `user_data`，由 `destroy` 释放；页面定时器在 `before_hide` 暂停、`before_show` 恢复，并在 `destroy` 删除。页面根仍存在时执行 `destroy`。

`create` 返回 false 时会调用 `destroy` 清理部分资源，因此销毁回调必须容忍未完成初始化。LVGL 自身的内存断言不属于可以返回 false 的可恢复错误。

## 操作语义

| 操作 | 语义 |
| --- | --- |
| SINGLE | 从栈顶寻找同 ID 页面，存在则返回并移除上层页面，否则新建 |
| PUSH | 新建实例并压栈，允许同 ID 多次出现 |
| REPLACE | 创建新页面后替换栈顶；空栈时创建根页 |
| RESET | 创建新页面后清空全部旧历史，作为新根页 |
| BACK | 返回一层，根页返回 AT_ROOT |
| POP_TO | 返回最近的指定 ID 实例；不存在返回 NOT_FOUND |
| ROOT | 返回栈底原实例；已在根页返回 AT_ROOT |

默认注册容量 16、历史深度 8、待执行命令 8。可通过一致的编译宏覆盖 `LAK_NAV_MAX_PAGES`、`LAK_NAV_MAX_DEPTH`、`LAK_NAV_MAX_COMMANDS`；不能只修改调用方的结构布局而不重编框架。

`lak_nav_request()` 返回的是入队结果。实际执行后写入 `nav.last_result`，并同步通知可选的 `nav.done(nav.done_context, result)`。每个定时器周期最多执行一个命令，避免链式回调饿死 LVGL。

导航和事件回调内可以继续排队，但不能同步递归执行导航、注销或销毁导航器；这些重入返回 BUSY。不要在普通 LVGL 对象事件回调内手动调用 `lak_nav_process()`，框架无法识别所有外部回调上下文。

## 生命周期与所有权

新页成功创建 → 旧当前页 `before_hide` → 更新当前页 → 新页 `before_show` → 切屏 → 销毁被移除页。仅离开当前页时调用 `before_hide`，已隐藏的历史页不会重复调用。

创建失败、未注册 ID 或栈容量不足都不会销毁当前页。首版采用即时切屏，不提供异步过渡动画；历史页保留完整 LVGL 对象，以换取返回时的状态保留。动画支持需要进一步设计转场期间对象的释放时机。

## 事件

调用 `lak_nav_dispatch()`，顺序为 `system_filter` → `overlay_filter` → 当前页。打开 `through_history` 时从栈顶向下继续分发；每一步都检查 `event.consumed`，消费后立即停止。

过滤器是业务回调接口，不会自动创建模态窗口。项目可使用 LVGL 的系统层和顶层创建全局控件。事件的 `data` 是借用数据，只在本次分发期间有效；需要延后处理时必须复制。

## 退出

停止通信任务及其他生产者 → 删除 GUI 桥接器 → GUI 线程调用 `lak_nav_deinit()` → 销毁输入设备和显示器 → `lv_deinit()`。不要在对象事件回调中直接执行退出清理，置退出标记并由主循环处理。

deinit 加载一张空白屏幕，释放所有历史页和导航定时器；空白屏幕归显示器持有，随显示器销毁。默认 Linux 显示端口用于单显示器进程，生命周期由应用统一管理。
