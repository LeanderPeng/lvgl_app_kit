find_package(Threads REQUIRED)
add_library(lak_linux STATIC
    comm_frame_queue_pthread.c
    comm_frame_channel_pthread.c
    comm_transport_fd.c)
target_include_directories(lak_linux PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(lak_linux PUBLIC lak_comm Threads::Threads)

add_library(lak_display_linux STATIC linux_display.c)
target_include_directories(lak_display_linux PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
target_link_libraries(lak_display_linux PUBLIC lvgl::lvgl m)
target_compile_definitions(lak_display_linux PRIVATE _POSIX_C_SOURCE=200809L)

foreach(target lak_linux lak_display_linux)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -Wpedantic)
endforeach()
