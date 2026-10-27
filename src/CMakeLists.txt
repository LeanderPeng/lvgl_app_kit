# 导航只依赖 LVGL，不把通信依赖带给页面控制层。
add_library(lak_navigation STATIC navigation.c)
target_include_directories(lak_navigation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(lak_navigation PUBLIC lvgl::lvgl m)

# 通用 GUI 消息调度不依赖页面导航，也不认识产品模型。
add_library(lak_gui_worker STATIC gui_worker.c)
target_include_directories(lak_gui_worker PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(lak_gui_worker PUBLIC lvgl::lvgl lak_comm)

# 保留已有聚合目标和直接转发页面的桥接 API，供旧接入方式使用。
add_library(lak_gui STATIC comm_bridge.c)
target_include_directories(lak_gui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(lak_gui PUBLIC lak_navigation lak_gui_worker)

foreach(target lak_navigation lak_gui_worker lak_gui)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -Wpedantic)
endforeach()
