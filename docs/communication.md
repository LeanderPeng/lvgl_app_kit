# 通信基础框架

建议先运行 [PING/PONG 最小通信案例](../examples/comm_echo/README.md)，按“协议注入、TX 入队、socket 收发、RX 出队”的顺序阅读实现。演示协议仅属于案例，通信核心仍不预设线路格式。

## 保留和移除的边界

从 communication_framework 移植 `RingBuffer`、`FrameQueue`、`FrameChannel` 及 pthread 后端。队列仍按值复制消息，关闭后禁止投递、允许排空，并唤醒等待线程。诊断统计和单调时钟等待保持上游实现。

重写 `comm_frame.h`，只包含项目自定义的内存 `id`、有效长度和字节数据，不带线路头、版本、类型或序号。不移植原有 Codec、Parser、MessageManager 和 appliance_manager，因为其实现依赖既定帧格式或事务语义。请求应答、序号、重试、心跳、断线重连和业务模型由项目决定。

## 数据路径

```text
GUI/业务任务 -> TX 通道 -> comm_link -> 项目 encode -> 非阻塞 transport.write
非阻塞 transport.read -> RingBuffer -> 项目 decode -> RX 通道
RX 通道 -> GUI 桥接定时器 -> 系统/浮层/当前页面事件
```

`comm_link_t` 由一个通信任务独占。TX/RX 使用 pthread 通道以跨线程；非阻塞核心队列与 RingBuffer 本身不提供并发保护。通信线程禁止操作 LVGL 对象。

## 协议注入契约

`comm_protocol_ops_t` 的三个函数必须全部提供：

| 接口 | 契约 |
| --- | --- |
| decode(context, ring, frame) | 提取最多一条逻辑消息，填写 id/payload_length/payload |
| encode(context, frame, bytes, capacity, size) | 成功返回 0，并产生 1..capacity 字节；失败返回非 0 |
| reset(context) | 清理协议自己的半帧/状态；初始化和显式 reset 时调用 |

decode 返回 `COMM_DECODE_READY` 时，必须已从 ring 消费至少一个字节，且 payload_length 不超过内存消息容量；这避免注入器不前进造成无限循环。返回 `MORE` 表示还需输入，允许消费确定的垃圾前缀，未完成帧须保留在 ring 或协议 context。可恢复的噪声、校验失败由项目 decode 自行丢弃并重新同步；不可恢复问题返回 `ERROR`。

框架每次调用 decode 前清零输出消息。不把内存结构体直接写入 fd，线路字节序和布局完全由 encode 决定。回调不得越界读写，不得重入同一个 link。

默认消息负载容量 `COMM_FRAME_MAX_PAYLOAD_SIZE=256`，流与 TX 编码缓冲 `COMM_LINK_BUFFER_SIZE=4096`。可通过所有相关目标共享的宏修改；注入器必须与容量匹配。若完整候选帧超过流缓冲，需增加容量或使用能够逐段消费的状态式解码器。

## 调度、背压及故障

`comm_link_poll(link, budget)` 每轮先尝试解析积压数据，最多处理 budget 条；再进行最多一次非阻塞读取和一次写入。新读取的字节在下一轮解析，调用者应周期调用，例如每 1–5 ms 一次，或配合 poll/epoll 唤醒。

RX 队列满时，link 保存已经解析的消息并暂停读取，下一轮先重试投递。TX 队列中取出的消息编码一次，短写时保持 buffer 和 offset，写完才取下一条。transport 返回 0 表示暂不可读写，负数表示不可恢复的断开/错误。队列等待统一为 0，不阻塞通信调度。

发生协议错误、通道关闭/错误、流缓冲耗尽、传输断开时，返回并锁存 `fault`，不会继续读写。项目停止旧连接、处理错误通知、排空或重新创建旧队列后，调用 `comm_link_reset()` 清除半帧和未完成发送，再建立新会话。reset 不会清空通道，也不会替项目重发可能已部分写出的旧消息。

send/入队成功只表示本地接受，不能证明设备已经执行。队列满需要调用方重试或明确处理拒绝；框架不静默覆盖旧消息。

## Linux fd 端口

调用者负责打开、配置和关闭 fd。`comm_transport_fd_init()` 设置 O_NONBLOCK，不取得 fd 所有权。socket 发送使用 MSG_NOSIGNAL；UART 应设 raw 模式、正确波特率、VMIN=1，避免零长度读取被误认为连接结束。该端口面向串口和流式 socket，不适用于数据报。

## GUI 桥接

使用 `lak_comm_bridge_init(&bridge, &nav, &rx, project_event_id, 10, 4)` 建立定时器。注册并打开根页面后再启动生产者；即便顺序尚未完成，桥接器在没有当前页时也不会消费队列。

页面回调收到 `lak_event_t`，其中 `data` 指向 `comm_frame_t` 的临时副本。桥接器只向当前页面分发。需要跨页面持久状态时，在系统过滤器中先更新项目模型，再交给页面刷新；历史页面恢复时从模型读取最新值，不依赖每条原始消息都曾直接到达页面。

停止顺序：请求通信任务退出 → close 通道唤醒等待者 → join 相关线程 → 停止桥接定时器 → destroy 队列。不能在仍有生产者/消费者访问时 destroy pthread 对象。
