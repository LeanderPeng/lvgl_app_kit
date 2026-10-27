add_library(lak_comm STATIC
    src/comm_ringbuffer.c
    src/comm_frame_queue.c
    src/comm_frame_channel.c
    src/comm_link.c)
target_include_directories(lak_comm PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_options(lak_comm PRIVATE -Wall -Wextra -Werror -Wpedantic)
