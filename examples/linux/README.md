# Linux 分层应用示例

这个示例把页面、视图、业务状态、消息处理、服务、协议、连接和设备模拟拆成独立模块，演示一条完整的温度查询链路。它既是可运行的 Linux GUI，也是后续产品工程的结构参考。

案例通过本地 `socketpair()` 连接应用端和模拟设备端，实际经过 TX/RX 通道、`comm_link`、协议编解码和 fd 传输。两端在同一个主线程中轮流推进，没有额外创建通信线程。

## 运行

以下命令在仓库根目录执行。首先确认已经运行 `git submodule update --init --recursive` 拉取固定版本的 LVGL。

### SDL 窗口

Linux 需要 CMake、C99/C++17 编译器、pthread、pkg-config 和 SDL2 开发包。

```sh
cmake -S . -B build-sdl -DLAK_BACKEND=SDL -DLAK_BUILD_DEMO=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-sdl --target lak_demo -j
./build-sdl/lak_demo
```

启动时会自动查询一次温度，随后可以在界面中继续查询、进入其他页面并返回。关闭窗口或按 Ctrl+C 退出。案例通信只使用进程内创建的本地 socket，不连接真实设备。

### HEADLESS 自动教学演示

无桌面的 Linux 服务器可以使用内存显示后端：

```sh
cmake -S . -B build-headless -DLAK_BACKEND=HEADLESS -DLAK_BUILD_DEMO=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-headless --target lak_demo -j
./build-headless/lak_demo --tour --frames 600 --snapshot build-headless/layered-demo.ppm
```

`--tour` 自动执行以下过程，便于结合日志阅读模块调用关系：

1. 在尚未打开页面时查询温度，接收回复并更新模型。
2. 打开首页，从模型读取温度并显示。
3. 进入设置页，再次查询并更新模型。
4. 返回首页，显示模型中的最新温度。

这一过程说明应用消息处理不依赖某个页面存在，也不需要让每个页面都解析设备消息。模拟设备前两次分别回复 `25.1 °C` 和 `25.2 °C`，最后返回首页时显示后者。

演示完成后打印 `[tour] Layered walkthrough complete` 并自动退出；若 `--frames` 太小导致演示尚未完成，程序打印未完成提示并返回失败。这里的循环上限用于限制运行时间，实际演示推进仍取决于消息和定时器状态。

如需观察分段收发，增加 `--fragment`：

```sh
./build-headless/lak_demo --tour --fragment --frames 1000 --snapshot build-headless/layered-fragment.ppm
```

分段模式将每次底层读写限制为最多 2 字节，用同一套协议和业务模块验证流式收发的实际行为。

### Framebuffer

```sh
cmake -S . -B build-fbdev -DLAK_BACKEND=FBDEV -DLAK_BUILD_DEMO=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-fbdev --target lak_demo -j
./build-fbdev/lak_demo --fb /dev/fb0 --input /dev/input/event0
```

显示和输入节点由使用者选择。`--input` 可以省略；设备驱动、权限、面板、背光等板级初始化由运行环境负责。

### 参数

| 参数 | 含义 |
| --- | --- |
| `--frames N` | 主循环迭代次数上限，`0` 表示不设此上限；不是实际显示刷新帧数 |
| `--snapshot path.ppm` | 退出前保存内存画面，仅 HEADLESS 支持 |
| `--fb path` | FBDEV 显示设备节点 |
| `--input path` | 可选 evdev 输入设备节点 |
| `--fragment` | 本地连接每次读写最多 2 字节 |
| `--tour` | 自动执行跨页面的温度查询演示，完成后退出 |

构建目录会保留 CMake 缓存。切换配置时重新执行对应的配置命令；若同时保留多种显示后端，使用不同构建目录。

## 模块职责

| 文件或目录 | 负责什么 |
| --- | --- |
| `main.c` | 调用应用入口 |
| `app/demo_app.*` | 参数、信号、初始化、回调连接、主循环、退出清理 |
| `domain/demo_model.*` | 应用持久状态及更新，无 LVGL 和通信依赖 |
| `application/demo_gui_worker.*` | 解释消息、更新模型、处理查询意图、通知界面 |
| `application/demo_service.*` | 构造查询消息并投递 TX 通道 |
| `protocol/demo_protocol.*` | 案例编解码、消息标识和字段转换 |
| `platform/demo_connection.*` | 本地连接两端、队列、link、fd 和收发调度，使用组装入口注入的协议接口 |
| `simulation/demo_device.*` | 消费设备端命令、维护模拟温度、生成回复并投递设备端 TX |
| `presentation/pages/demo_pages.*` | 页面生命周期、导航、读取模型、响应交互 |
| `presentation/views/demo_view.*` | 创建控件、布局、文本和样式 |

页面只表达用户意图；组装入口将这个意图连接到应用 worker。应用 worker 更新模型后通过注入的回调通知表现层，无需直接依赖具体页面。

连接模块不包含产品协议头文件，也不识别 `QUERY` 命令。主循环依次推进连接收发、模拟设备处理和 LVGL 定时器；模拟设备与界面应用各自消费所属端点的 RX 通道。

## 案例协议

本案例的线路格式为：

```text
应用 → 模拟设备：QUERY\n
模拟设备 → 应用：TEMP:<温度整数>\n
```

温度整数的单位为十分之一摄氏度。例如 `TEMP:253\n` 表示 `25.3 °C`，其中 `\n` 表示线路上的单个换行字节。

这些命令、分隔符和字段含义全部位于案例的协议模块。它们不是 `lak_comm` 的默认协议，也不会要求未来的产品沿用。产品可以替换为自己的二进制格式、校验方式、请求应答和业务命令。

`comm_frame_t` 是内存消息容器，不能直接当作线路结构体发送。协议模块负责它与线路字节之间的转换；应用 worker 通过项目协议接口取得有意义的字段，再更新模型。

案例同一时间只允许一个查询等待回复，超过 2 秒未收到回复会将请求标记为失败。请求限制和超时位于应用层；框架本身不规定事务编号、重试或超时策略。

## 对照日志阅读调用链

```text
用户查询
  → page 的操作回调
  → application worker
  → service
  → TX 通道
  → link 调用编码器
  → 本地 socket
  → simulation 模拟设备消费设备端 RX 并生成回复
  → 设备端 TX 和 link
  → 本地 socket
  → link 调用解码器
  → RX 通道
  → lak_gui_worker
  → application worker 更新 model
  → 通知 page
  → page 读取 model 并更新 view
```

先读应用入口，确认各对象由谁创建、回调交给谁；再沿一次查询逐层跟踪。暂时不用同时展开队列内部、导航内部和 LVGL 内部。

`lak_gui_worker` 是框架的通用接收调度器，`demo_gui_worker` 是案例的应用消息处理器。前者不知道温度是什么，后者不读写 fd，也不负责画控件。

回调收到的消息仅在回调期间有效。要跨页面保存的温度被复制到模型；不要把消息指针存入页面等待以后使用。

## 应用于后续项目

在独立产品仓库中保留相同的职责划分，替换 `domain`、`application`、`protocol`、`presentation` 和产品连接配置。框架继续作为依赖提供通用模块。

使用 GUI Guider 等工具时，可将生成的控件与布局接入 views；页面生命周期和业务处理留在独立的手写模块。不要把协议解析塞进控件回调，也不要把设备状态只保存在某个页面的局部变量中。

若实际项目需要通信线程，将连接轮询交给该线程，通过线程安全通道与 GUI 线程交接。线程退出并完成等待之后，才能销毁队列和连接资源。模型与 LVGL 仍由 GUI 线程统一访问。

进一步阅读：[工程分层与模块边界](../../docs/architecture.md)、[通信契约](../../docs/communication.md)、[导航生命周期](../../docs/navigation.md)、[纯通信最小案例](../comm_echo/README.md)。
