# 最小通信案例：PING / PONG

这个案例演示如何把项目自己的协议注入框架，走完一次双向收发。不需要串口、设备或图形桌面，程序通过 Linux `socketpair` 建立两个真实的本地流式 socket。

## 构建与运行

在仓库根目录执行：

```sh
cmake -S . -B build-comm -DLAK_BACKEND=HEADLESS \
    -DLAK_BUILD_DEMO=OFF -DLAK_BUILD_COMM_EXAMPLE=ON
cmake --build build-comm --target lak_comm_echo -j
./build-comm/lak_comm_echo
./build-comm/lak_comm_echo --fragment
```

配置阶段仍需初始化仓库的 LVGL 子模块，但只构建 `lak_comm_echo` 目标时不会编译或链接 LVGL、GUI 和显示端口。该可执行文件仅链接通信核心及 Linux 端口。

普通运行输出：

```text
Local socket pair; example-only line protocol; I/O: normal
[host] TX queued: PING
[device] RX decoded: PING
[device] TX queued: PONG
[host] RX decoded: PONG
Round trip complete.
```

`--fragment` 将两端每次读取、写入都限制为最多 2 字节。线路上的 `PING\n` 会分成 `PI`、`NG`、`\n` 写出；接收端必须等到换行符到达才能输出完整消息。它仍应完成同样的 PING/PONG 往返。

成功退出码为 0；初始化、收发、解码、内容核对或资源回收失败为 1；参数错误为 2。主循环使用单调时钟设置 2 秒总超时，避免失败时永久等待；这只是案例的退出条件，不是框架的请求超时/重试策略。

## 阅读顺序

1. `demo_protocol.c`：`encode_line()` 把消息编码为线路字节，`decode_line()` 从 RingBuffer 中提取完整消息。
2. `main.c` 的 `endpoint_init()`：建立 RX/TX 队列和通道，绑定非阻塞 fd，再通过 `comm_link_init()` 注入协议与传输接口。
3. `send_text()`：构造 `comm_frame_t`，按值投递到 TX 通道；此时只表示本地入队。
4. 主循环的 `comm_link_poll()`：推进解析、编码和真实 socket 读写。
5. `receive_text()`：应用从 RX 通道取出解码后的消息；设备端应用收到 PING 后才投递 PONG。
6. `endpoint_destroy()`：循环结束后关闭和销毁队列，最后由调用者关闭 fd。

```text
host 应用投递 PING
  -> host TX -> encode -> socket -> device RingBuffer -> decode -> device RX
  -> device 应用收到 PING 并投递 PONG
  -> device TX -> encode -> socket -> host RingBuffer -> decode -> host RX
  -> host 应用收到 PONG，正常退出
```

## 案例协议的边界

为了便于阅读，这里单独定义了一个按换行符分隔的演示协议：

- 线路格式：`payload + '\n'`，不发送内存结构体和结尾 NUL。
- 内存消息 `id=0`，没有线路命令号、CRC、请求序号或自动重试。
- payload 最大为 `COMM_FRAME_MAX_PAYLOAD_SIZE` 字节，不能包含换行符；空行可以解码为空消息。
- 半帧保留在 RingBuffer；多个完整行可以逐条取出；超长行返回协议错误。
- PING/PONG 是 `main.c` 中的应用逻辑，协议文件只负责行的编码和提取。

这套协议只编译进案例目标，**不会链接到 lak_comm/lak_linux，也不是框架的默认协议**。正式项目应提供自己的 `comm_protocol_ops_t`，并按需要实现校验、转义、重新同步和事务管理。

## 与真实项目的对应

本案例在单一主线程依次调度两端，重点是展示调用顺序，没有额外创建收发线程。虽然使用 pthread 队列，但此处不演示并发调度，也不代表实际串口联调。

接入项目时，通信线程独占自己的 `comm_link_t`；业务/GUI 通过 TX/RX 通道交接消息。需要驱动界面时，把主机 RX 交给 `lak_comm_bridge_init()`，不要同时由 `receive_text()` 和 GUI 桥接器消费同一个 RX 队列。协议、业务页面和设备初始化继续放在使用方仓库。

通过 `-DLAK_BUILD_COMM_EXAMPLE=OFF` 可关闭案例构建。该选项首次配置时默认跟随 `LAK_BUILD_DEMO`，后续以 CMake 缓存中的显式值为准。
