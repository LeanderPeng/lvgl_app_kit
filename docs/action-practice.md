# 第 1 课：设置目标温度的带参数 Action

本课使用 `examples/linux` 分层应用，不修改导航和通信框架，也不接入 62455 旧工程。目标是理解并接通：控件输入 → 视图 Action → 页面 → 注入接口 → 应用 worker → service。模拟设备确认用于观察请求是否真正完成；协议和队列内部留到后续课程展开。

## 1. 先看一次完整操作

进入首页的 `Pages`，在设置页拖动滑条到 27.5°C，然后松手：

```text
LV_EVENT_VALUE_CHANGED
  → slider_event：只更新 Selection 预览
LV_EVENT_RELEASED
  → slider_event：构造 { SET_TARGET_TEMPERATURE, 275 }
  → view->action(view->context, &action)
  → page_action(state, &action)
  → pages.set_target_temperature(command_context, 275)
  → app 中的 set_target_temperature(&app.worker, 275)
  → demo_gui_worker_set_target_temperature(worker, 275)
  → demo_service_set_target_temperature(service, 275)
  → demo_protocol_make_set_target(&frame, 275)
  → comm_frame_channel_push(tx, &frame, 0)
```

`Set 26 C` 按钮从 `button_clicked()` 开始，构造 `{ SET_TARGET_TEMPERATURE, 260 }`，之后经过相同链路。页面、worker 和 service 都不用识别输入来自滑条还是按钮。

这里的 Action 是应用示例定义的同步回调契约，不是导航框架中的通用命令对象。`demo_view_action_t`、`lak_nav_action_t`、`lak_event_t` 是三种不同的类型。

## 2. 按顺序读源码

以下路径均相对于仓库根目录。

| 顺序 | 源码与函数 | 阅读时要回答的问题 |
| --- | --- | --- |
| 1 | `examples/linux/presentation/views/demo_view.h`：`demo_view_action_t` | 动作类型和值分别在哪里？单位是什么？ |
| 2 | `examples/linux/presentation/views/demo_view.c`：`slider_event`、`button_clicked` | LVGL 事件在哪里变成用户意图？ |
| 3 | `examples/linux/presentation/pages/demo_pages.c`：`page_action` | 为什么页面只读取参数并调用注入接口？ |
| 4 | `examples/linux/presentation/pages/demo_pages.h`：`demo_pages_context_t` | 页面依赖具体 worker 类型了吗？ |
| 5 | `examples/linux/app/demo_app.c`：`set_target_temperature`、`app.pages` 赋值 | 哪个函数负责连接页面与应用？ |
| 6 | `examples/linux/application/demo_gui_worker.c`：`demo_gui_worker_set_target_temperature` | 范围、单请求限制和 pending 在哪里处理？ |
| 7 | `examples/linux/application/demo_service.c`：`demo_service_set_target_temperature` | 谁构造消息？成功返回代表什么？ |
| 8 | `examples/linux/application/demo_gui_worker.c`：`receive` | 何时才能更新已确认目标温度？ |

### Action 为什么从枚举变成结构体

原来的枚举只能表达“查询”“返回”等无参数动作。设置温度还需要携带数值：

```c
typedef struct {
    demo_view_action_kind_t kind;
    /* 仅设置目标温度时有效，单位为十分之一摄氏度。 */
    int32_t temperature_tenths;
} demo_view_action_t;
```

`260` 表示 26.0°C，`275` 表示 27.5°C。这是明确的接口单位，避免把控件值、浮点显示值和协议整数混在一起。其他动作忽略温度字段。将来有多种不同参数时，可以改为带类型标记的 union；本课只有一个参数，不引入额外结构。

### 为什么回调还要一个 context

```c
typedef void (*demo_view_action_fn)(void *context, const demo_view_action_t *action);
```

函数指针决定“调用哪段代码”，context 决定“操作哪个实例”。创建视图时，页面把 `page_action` 和自己的 `state` 一起交给视图；点击时，视图原样传回这两个信息。这样不需要全局的“当前页面状态”。

页面向应用提交命令时又使用另一组函数和 context：

```c
bool (*set_target_temperature)(void *context, int32_t temperature_tenths);
```

组装入口把函数设为应用适配函数，把 `command_context` 设为 `&app.worker`。页面头文件只知道这个契约，不需要包含 `demo_gui_worker.h`、协议或通道头文件。

视图回调的 `void` 表示它只上报意图，界面结果由模型驱动。应用命令的 `bool` 表示命令是否被本地接纳，不能解释为设备已经设置成功。

### 各层负责什么

| 层 | 本次职责 |
| --- | --- |
| view | 读取滑条值、构造 Action、更新选择预览、显示页面传来的文案和禁用状态 |
| page | 把 Action 转成应用命令，读取模型并格式化显示，提交导航请求 |
| app | 创建实例，连接函数与 context，安排初始化和退出清理 |
| application worker | 检查 16.0～30.0°C 范围和单请求限制，记录等待状态，处理确认和超时 |
| service | 构造带温度参数的内存消息，尝试投递 TX 通道 |
| protocol / device | 在本案例中实现 `SET:275\n` 和 `TARGET:275\n` 往返 |
| model | 保存实测温度、已确认目标、待确认目标和请求状态，跨页面存活 |

滑条范围由页面传入，业务范围仍在 worker 检查。以后增加按键、旋钮或外部调用，也不能绕过应用层校验。

## 3. 哪些数据被复制，哪些指针被借用

Action 在控件回调的栈上创建，仅在同步调用期间有效。页面不能把 `action` 指针保存到状态中，定时器和异步队列也不能保存这个地址。

本课逐层复制整数参数；worker 保存请求温度值；service 构造局部 `comm_frame_t`，pthread 队列复制帧内容。离开控件回调后，后续通信不再依赖原 Action 或页面对象。

| 对象或地址 | 持有者与有效期 |
| --- | --- |
| `button_binding_t` | 嵌入 view，和控件回调一起存活；不能使用创建按钮函数的局部 binding |
| slider 的 user_data | 借用 view，必须在释放 view 前先删除控件 |
| view 的 context | 借用页面 state，必须在销毁 view 后才能释放 state |
| state 的 context | 借用 `app.pages`，必须覆盖导航及所有页面的生存期 |
| command_context | 借用 `app.worker`，由组装入口保证有效 |
| model | 属于 app，页面隐藏或销毁不会删除模型 |

当前释放顺序是：停止 GUI 接收工作器 → 销毁导航中的页面 → 删除 view 控件 → 释放 view → 释放页面 state → 销毁通信资源和显示 → 应用入口返回。工作器停止接收后，其结构体内存仍存在，页面销毁函数不会再提交命令。

导航仍使用 `lak_nav_request()` 延后执行。不要在控件回调里立即处理导航，把当前控件所属页面同步销毁。

## 4. 一次操作有三个时刻

1. **选择**：拖动时仅修改 view 的 `Selection`，不会发送每一个中间值。
2. **接纳**：松手后 worker 检查范围和 pending，service 成功入队；记录 `requested_target_tenths`，但不修改已确认目标。界面禁用设置输入，显示等待状态。
3. **确认**：收到匹配的 `TARGET` 回复后才更新 `target_temperature_tenths` 和 `has_target_temperature`，解除 pending，通知当前页面刷新。

`temperature_tenths` 是实测温度，与目标温度分开。设为 27.5°C 不会把实测温度直接改成 27.5°C。

查询和设置共用原有的单请求限制，模型中的 `pending_kind` 区分期望回复，页面也据此决定显示哪一个选择值。等待设置时，`TEMP` 回复不能当作设置确认；`TARGET` 数值也必须匹配本次请求值。不匹配的消息会记录日志，继续等待原有的两秒超时。超时或 TX 投递失败保留此前已确认目标；设备可能已经执行但回复丢失时，本地状态只表示确认情况。

本案例没有事务序号，按“请求类型 + 值”匹配无法区分超时前后两次相同设置的迟到回复。真实产品的事务、重试、重连和状态查询策略属于产品协议与应用层，本课不把它扩展成完整设备控制协议。

## 5. 小练习

先写出你的答案，再对照源码和末尾参考答案。

1. 滑条从 25.0 拖到 27.5，期间发生五次 VALUE_CHANGED，最后一次 RELEASED。应该发送几次设置？参数是多少？
2. 将来接入旋钮或数字键盘，哪些层需要改？worker 和 service 需要知道输入设备吗？
3. 设置页上报 275 后立刻返回首页，设备才回复。哪些对象已经销毁，275 还保存在哪里？再次打开设置页从哪里恢复？
4. TX 通道满时，能否先把 `target_temperature_tenths` 设为 275 再提示失败？为什么？
5. `action`、`view->context`、`command_context` 三个指针，分别指向谁？哪个只活到当前同步调用返回？

可选动手：把预设改为 28.0°C，同时修改按钮文案和 binding 的温度值。只需要改 view，之后观察 worker 日志与确认显示是否均为 280。完成后恢复 26.0°C，避免混淆下文结果。

## 6. 运行验证

在 Linux 仓库根目录执行：

```sh
cmake -S . -B build-sdl -DLAK_BACKEND=SDL -DLAK_BUILD_DEMO=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-sdl --target lak_demo -j
./build-sdl/lak_demo --fragment
```

进入 `Pages`，先拖动到 27.5°C 并松手，再按 `Set 26 C`。等待回复后，分别应看到 `Confirmed: 27.5 C` 和 `Confirmed: 26.0 C`。再次进入首页查询，实测温度继续按模拟设备原有逻辑变化，不会被目标值覆盖。

观察日志：

```text
[service] Set target queued=275 tenths; awaiting confirmation
[worker] Target confirmed=275 tenths, revision=...
[service] Set target queued=260 tenths; awaiting confirmation
[worker] Target confirmed=260 tenths, revision=...
```

服务器没有桌面时可先验证既有主链路回归：

```sh
cmake -S . -B build-headless -DLAK_BACKEND=HEADLESS -DCMAKE_BUILD_TYPE=Debug
cmake --build build-headless --target lak_demo -j
./build-headless/lak_demo --tour --fragment --frames 1000
```

`--tour` 仍演示温度查询和跨页面模型更新，不会自动触发新增设置控件。此次真实控件事件、目标设置及异常分支由临时验证程序运行；结果见 `docs/validation.md`。临时程序验证后删除，不作为产品接口。

## 7. 复盘与参考答案

1. 只发送一次，参数 275；VALUE_CHANGED 只预览，RELEASED 才提交。
2. 输入端构造同一个 Action 即可；新交互若不需要新业务语义，页面以下不用区分来源。
3. Back 导航处理后，设置页的控件、view 和 state 被销毁；worker/model 和已入队消息仍有效。回复更新 app 拥有的模型；重新创建的设置页在 `before_show` 读取模型。
4. 不能。入队失败连本地接纳都没完成；已确认目标只能由有效设备确认更新。
5. action 指向局部动作结构；view 的 context 指向页面 state；command_context 指向 app.worker。只有 action 指针限于本次同步回调；另外两者由持有对象的生命周期约束。

本节应能独立说明：动作在哪里产生、参数怎样越过分层、哪里校验业务、true 的含义、谁拥有上下文，以及页面销毁后为什么不会丢掉已经提交的请求。下一节再展开 `lak_event_t` 如何经过系统过滤器、浮层和页面，以及哪里消费事件。
