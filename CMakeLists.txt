cmake_minimum_required(VERSION 3.16)
project(lvgl_app_kit VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LAK_BACKEND "SDL" CACHE STRING "Linux display backend: SDL, FBDEV, HEADLESS")
set_property(CACHE LAK_BACKEND PROPERTY STRINGS SDL FBDEV HEADLESS)
option(LAK_BUILD_DEMO "Build Linux GUI example" ON)
set(LAK_LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/config/lv_conf.h" CACHE FILEPATH "LVGL configuration")
set(LAK_SDL 0)
set(LAK_FBDEV 0)
if(LAK_BACKEND STREQUAL "SDL")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    set(LAK_SDL 1)
elseif(LAK_BACKEND STREQUAL "FBDEV")
    set(LAK_FBDEV 1)
elseif(NOT LAK_BACKEND STREQUAL "HEADLESS")
    message(FATAL_ERROR "LAK_BACKEND must be SDL, FBDEV or HEADLESS")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/lvgl/CMakeLists.txt")
    message(FATAL_ERROR "Run git submodule update --init --recursive first")
endif()
set(LV_BUILD_CONF_PATH "${LAK_LV_CONF_PATH}" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG OFF CACHE BOOL "" FORCE)
set(LV_FETCH_DEPENDENCIES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/lvgl EXCLUDE_FROM_ALL)
target_compile_definitions(lvgl PUBLIC LAK_USE_SDL=${LAK_SDL} LAK_USE_FBDEV=${LAK_FBDEV})
if(LAK_FBDEV)
    target_compile_definitions(lvgl PRIVATE _POSIX_C_SOURCE=200809L)
endif()
if(LAK_SDL)
    target_link_libraries(lvgl PUBLIC PkgConfig::SDL2)
endif()

add_library(lak_comm STATIC
    communication/src/comm_ringbuffer.c
    communication/src/comm_frame_queue.c
    communication/src/comm_frame_channel.c
    communication/src/comm_link.c)
target_include_directories(lak_comm PUBLIC communication/include)

find_package(Threads REQUIRED)
add_library(lak_linux STATIC
    ports/linux/comm_frame_queue_pthread.c
    ports/linux/comm_frame_channel_pthread.c
    ports/linux/comm_transport_fd.c)
target_include_directories(lak_linux PUBLIC ports/linux)
target_link_libraries(lak_linux PUBLIC lak_comm Threads::Threads)

add_library(lak_gui STATIC src/navigation.c src/comm_bridge.c)
target_include_directories(lak_gui PUBLIC include)
target_link_libraries(lak_gui PUBLIC lvgl::lvgl lak_comm m)

add_library(lak_display_linux STATIC ports/linux/linux_display.c)
target_include_directories(lak_display_linux PUBLIC include)
target_link_libraries(lak_display_linux PUBLIC lvgl::lvgl m)
target_compile_definitions(lak_display_linux PRIVATE _POSIX_C_SOURCE=200809L)

foreach(target lak_comm lak_linux lak_gui lak_display_linux)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -Wpedantic)
endforeach()

if(LAK_BUILD_DEMO)
    add_executable(lak_demo examples/linux/main.c)
    target_link_libraries(lak_demo PRIVATE lak_gui lak_linux lak_display_linux)
    target_compile_definitions(lak_demo PRIVATE _POSIX_C_SOURCE=200809L)
    target_compile_options(lak_demo PRIVATE -Wall -Wextra -Werror -Wpedantic)
endif()
