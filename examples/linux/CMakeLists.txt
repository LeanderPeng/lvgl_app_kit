# 每层单独构建，以依赖关系明确模块职责，禁止下层反向链接应用入口。
add_library(demo_model STATIC domain/demo_model.c)
add_library(demo_protocol STATIC protocol/demo_protocol.c)
target_link_libraries(demo_protocol PUBLIC lak_comm)

add_library(demo_service STATIC application/demo_service.c)
target_link_libraries(demo_service PUBLIC demo_protocol)

add_library(demo_worker STATIC application/demo_gui_worker.c)
target_link_libraries(demo_worker PUBLIC lak_gui_worker demo_model demo_service)

add_library(demo_views STATIC presentation/views/demo_view.c)
target_link_libraries(demo_views PUBLIC lvgl::lvgl)

add_library(demo_pages STATIC presentation/pages/demo_pages.c)
target_link_libraries(demo_pages PUBLIC lak_navigation demo_model demo_views)

add_library(demo_connection STATIC platform/demo_connection.c)
target_link_libraries(demo_connection PUBLIC lak_linux)
target_compile_definitions(demo_connection PRIVATE _POSIX_C_SOURCE=200809L)

add_library(demo_device STATIC simulation/demo_device.c)
target_link_libraries(demo_device PUBLIC demo_protocol)

add_library(demo_app STATIC app/demo_app.c)
target_link_libraries(demo_app PRIVATE demo_connection demo_device demo_worker demo_pages lak_display_linux)

foreach(target demo_model demo_protocol demo_service demo_worker demo_views demo_pages demo_connection demo_device demo_app)
    target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -Wpedantic)
endforeach()

add_executable(lak_demo main.c)
target_link_libraries(lak_demo PRIVATE demo_app)
target_compile_options(lak_demo PRIVATE -Wall -Wextra -Werror -Wpedantic)
set_target_properties(lak_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
