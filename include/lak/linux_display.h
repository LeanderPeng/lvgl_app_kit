#ifndef LAK_LINUX_DISPLAY_H
#define LAK_LINUX_DISPLAY_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 在 lv_init 之后调用，backend 由 CMake 决定；FBDEV 使用传入设备路径。 */
lv_display_t *lak_linux_display_create(int32_t width, int32_t height,
    const char *framebuffer, const char *input);
bool lak_linux_display_running(void);
/* 在导航、桥接器销毁后调用；单显示器示例端口不支持重复初始化。 */
void lak_linux_display_destroy(lv_display_t *display);
uint32_t lak_linux_tick_ms(void);
void lak_linux_sleep_ms(uint32_t ms);
/* HEADLESS 输出 PPM，用于检查实际渲染；其他后端返回 false。 */
bool lak_linux_display_snapshot(const char *path);

#ifdef __cplusplus
}
#endif
#endif
