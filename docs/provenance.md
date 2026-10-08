# 来源和移植差异

## LVGL

- 上游：https://github.com/lvgl/lvgl
- 版本：v9.6.0
- 提交：`80ca777e37a2b176770726a02e07a6fb79ef0b39`
- 引入形式：`external/lvgl` Git 子模块；保持上游代码与许可证原样。
- 本工程适配：独立 lv_conf.h、构建目标、Linux 显示入口，全部位于子模块外。

## Communication Framework

- 上游：https://github.com/LeanderPeng/communication_framework
- 来源提交：`3e5003196e2c88991fced0002e17a63805820d97`
- 快照形式：只移植所需基础文件，避免把参考协议和产品示例带入通用框架。
- 复用模块：`comm_ringbuffer`、`comm_frame_queue`、`comm_frame_channel`、`comm_frame_queue_pthread`、`comm_frame_channel_pthread`。
- 适配差异：公共头文件增加 C++ 链接边界；`comm_frame.h` 重写为无线路语义的内存容器。队列和通道算法保持上游实现。
- 新增模块：`comm_link` 协议/传输注入与背压调度、`comm_transport_fd` 非阻塞 fd 端口、`lak_comm_bridge` GUI 线程交接。
- 未移植模块：原 Codec、Parser、MessageManager、appliance_manager 及测试，避免固化原帧格式与业务事务规则。

上游后续更新不会自动覆盖移植快照。更新时应逐项比较基础模块，并重新验证当前的无协议消息容器和 pthread 交接。

## 页面导航

参考 SAT63331 PageManager 的页面注册、历史栈、单实例返回、根页重置、beforeLoad/beforeHide/beforeFreed 生命周期以及分层事件分发思想，使用 C99 独立实现。

未复制产品页面、资源、业务消息、RTOS/ETL 依赖。新实现增加明确错误返回、创建失败回滚、容量检查、命令延迟执行和事件逐层消费检查。首版不移植异步转场动画，以保证页面所有权与销毁顺序清晰。
