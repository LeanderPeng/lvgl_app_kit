# LVGL App Kit

面向 Linux 的通用 LVGL 9 GUI 框架。提供页面导航、通信基础设施和 GUI 消息交接；具体产品的页面、业务模型、协议及设备初始化留在使用方仓库。

当前版本 `0.1.0`，固定使用 [LVGL v9.6.0](https://github.com/lvgl/lvgl/releases/tag/v9.6.0)，提交 `80ca777e37a2b176770726a02e07a6fb79ef0b39`。通过 Git submodule 锁定版本，不跟踪浮动分支。

## 已提供的能力

- C99 页面注册、有限历史栈、单实例复用、多实例压栈、替换、重置、返回、返回指定页和根页。
- 页面创建、显示前、隐藏前、销毁生命周期；创建失败保留当前页面。
- 延后执行导航请求，系统层 → 浮层 → 页面事件分发，事件消费后停止。
- 从 [communication_framework](https://github.com/LeanderPeng/communication_framework) 移植环形缓冲、消息队列、通道及 Linux pthread 后端。
- 项目注入编解码和传输接口，支持分段输入、发送短写、接收背压和明确的错误状态。
- Linux 非阻塞 fd 传输适配，可对接已配置的 UART、TCP 或本地 socket。
- GUI 定时器消费 RX 通道，所有 LVGL 调用统一在 GUI 线程执行。
- 独立于页面的 GUI 消息工作器；产品先更新模型，页面显示时读取最新状态。
- 应用组装、消息处理、服务、模型、协议、页面控制、视图及平台适配各自建模和构建。
- SDL、Linux framebuffer/evdev、HEADLESS 三种显示后端及三个通用示例页面。

**没有预设线路协议。** 框架没有 AA55、CRC、字节序、版本号、命令表、应答格式或重试策略。`comm_frame_t` 仅是内存消息容器，不能直接在线路上传输。未注入协议时，`comm_link_init()` 拒绝启动。

想先理解通信调用链，可从 [PING/PONG 最小通信案例](examples/comm_echo/README.md) 入手：通过本地 socket 完成一次往返，并支持限制每次读写 2 字节的分段模式。案例协议独立放在 `examples/comm_echo`，不会成为框架默认协议。

学习完整工程时，先读[架构与模块边界](docs/architecture.md)，再运行[分层 GUI 应用](examples/linux/README.md)。它通过本地模拟设备查询温度，完整演示命令发送、编解码、模型更新和页面刷新。

## 快速运行

Linux 需要 CMake ≥ 3.16、C99/C++17 编译器、pthread。SDL 模式另需 pkg-config 和 SDL2 开发包。

```sh
git clone --recurse-submodules https://github.com/LeanderPeng/lvgl_app_kit.git
cd lvgl_app_kit
cmake -S . -B build -DLAK_BACKEND=SDL -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/lak_demo
```

已克隆但未拉取依赖时执行 `git submodule update --init --recursive`。示例可以查询模拟温度、进入页面和返回；关闭窗口或按 Ctrl+C 退出。通信使用进程内创建的本地 socket 对，不连接真实串口或远程设备。

无图形桌面的服务器可使用真实 LVGL 渲染到内存：

```sh
cmake -S . -B build-headless -DLAK_BACKEND=HEADLESS
cmake --build build-headless -j
./build-headless/lak_demo --frames 30 --snapshot build-headless/home.ppm
```

自动演示无页面接收、后台更新模型和返回页面恢复状态：

```sh
./build-headless/lak_demo --tour --fragment --snapshot build-headless/tour.ppm
```

`--frames N` 限制主循环迭代次数，`0` 表示持续运行；它不是实际显示刷新次数。PPM 截图仅支持 HEADLESS。

使用 SDL 虚拟视频驱动做启动检查：

```sh
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software ./build/lak_demo --frames 30
```

Linux framebuffer 设备：

```sh
cmake -S . -B build-fbdev -DLAK_BACKEND=FBDEV
cmake --build build-fbdev -j
./build-fbdev/lak_demo --fb /dev/fb0 --input /dev/input/event0
```

设备节点必须由调用者选择并具备访问权限；不传 `--input` 时不打开输入设备。框架不包含厂商面板、电源、背光或 DMA 初始化，需由实际项目负责。

## 作为外部项目依赖

验证旧项目和创建新产品时，将本仓库作为独立依赖；不要把产品实现放进框架的 `src/` 或 `communication/`。

```cmake
set(LAK_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(LAK_BUILD_COMM_EXAMPLE OFF CACHE BOOL "" FORCE)
set(LAK_BACKEND HEADLESS CACHE STRING "" FORCE)
add_subdirectory(third_party/lvgl_app_kit)
add_executable(my_product main.c pages.c protocol.c)
target_link_libraries(my_product PRIVATE lak_navigation lak_gui_worker lak_linux)
```

`lak_navigation` 提供页面导航，`lak_gui_worker` 提供与页面无关的 GUI 消息调度，`lak_comm` 提供操作系统无关的通信核心，`lak_linux` 提供 pthread 队列和 fd 传输；需要框架的显示端口时再链接 `lak_display_linux`。`lak_gui` 聚合目标和原 `comm_bridge` API 继续可用。也可自行创建 LVGL 显示器、输入设备及 tick 回调，再把显示器交给 `lak_nav_init()`。

项目配置可通过 `-DLAK_LV_CONF_PATH=/absolute/path/lv_conf.h` 注入；所有目标必须使用同一份配置。交叉编译传入项目自己的 `CMAKE_TOOLCHAIN_FILE`，本框架不绑定任何芯片或产品工具链。

## 目录与接入文档

```text
include/lak/           导航、GUI 工作器、兼容桥接及 Linux 显示接口
src/                   分别构建导航、GUI 工作器与兼容桥接
communication/         不预设协议的通信核心
ports/linux/           pthread、fd、SDL/FBDEV/HEADLESS 端口
config/lv_conf.h        默认 LVGL 配置
examples/linux/        完整分层应用，包含示例专属模型和协议
examples/comm_echo/    独立协议注入与本地 socket 收发案例
external/lvgl/         固定版本的官方子模块
docs/                  接入契约、来源和验证记录
```

- [导航接口和生命周期](docs/navigation.md)
- [整体架构与模块边界](docs/architecture.md)
- [分层应用的运行与阅读顺序](examples/linux/README.md)
- [通信协议注入与线程交接](docs/communication.md)
- [外部项目移植步骤](docs/integration.md)
- [代码来源与差异](docs/provenance.md)
- [验证记录及边界](docs/validation.md)

新增及修改代码注释使用中文；文本统一使用 UTF-8 和 LF。第三方子模块保留上游内容。
