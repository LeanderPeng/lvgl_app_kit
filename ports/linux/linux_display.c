#include "lak/linux_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

static bool running;

#if LV_USE_SDL
#include <SDL2/SDL.h>

/* 退出由应用主循环统一清理，阻止 SDL 驱动在定时器内部提前销毁 LVGL。 */
static int event_filter(void *context, SDL_Event *event)
{
    (void)context;
    if (event->type == SDL_QUIT ||
        (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_CLOSE)) {
        running = false;
        return 0;
    }
    return 1;
}
#elif !LV_USE_LINUX_FBDEV
static uint32_t *pixels;
static uint32_t *draw_buffer;
static int32_t pixel_width;
static int32_t pixel_height;

static void headless_flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{
    size_t width = (size_t)lv_area_get_width(area);
    size_t stride = lv_draw_buf_width_to_stride((uint32_t)width, LV_COLOR_FORMAT_XRGB8888);
    for (int32_t y = area->y1; y <= area->y2; ++y) {
        memcpy(pixels + (size_t)y * (size_t)pixel_width + area->x1,
               data + (size_t)(y - area->y1) * stride, width * sizeof(uint32_t));
    }
    lv_display_flush_ready(display);
}
#endif

uint32_t lak_linux_tick_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint32_t)((uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u);
}

void lak_linux_sleep_ms(uint32_t ms)
{
    struct timespec delay = {(time_t)(ms / 1000u), (long)(ms % 1000u) * 1000000L};
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
}

lv_display_t *lak_linux_display_create(int32_t width, int32_t height,
    const char *framebuffer, const char *input)
{
    lv_display_t *display;
    if (width <= 0 || height <= 0 || running) return NULL;
    (void)framebuffer;
    (void)input;
    lv_tick_set_cb(lak_linux_tick_ms);
#if LV_USE_SDL
    display = lv_sdl_window_create(width, height);
    if (!display) return NULL;
    SDL_SetEventFilter(event_filter, NULL);
    lv_sdl_window_set_title(display, "LVGL App Kit");
    lv_indev_t *mouse = lv_sdl_mouse_create();
    if (!mouse) {
        lv_display_delete(display);
        return NULL;
    }
    lv_indev_set_display(mouse, display);
#elif LV_USE_LINUX_FBDEV
    display = lv_linux_fbdev_create();
    if (!display) return NULL;
    if (!framebuffer || lv_linux_fbdev_set_file(display, framebuffer) != LV_RESULT_OK) {
        lv_display_delete(display);
        return NULL;
    }
    if (input) {
        lv_indev_t *touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, input);
        if (!touch) {
            lv_display_delete(display);
            return NULL;
        }
        lv_indev_set_display(touch, display);
    }
#else
    if ((size_t)width > SIZE_MAX / sizeof(uint32_t) / (size_t)height ||
        (size_t)width > UINT32_MAX / sizeof(uint32_t) / 32u) return NULL;
    pixels = calloc((size_t)width * (size_t)height, sizeof(uint32_t));
    draw_buffer = calloc((size_t)width * 32u, sizeof(uint32_t));
    if (!pixels || !draw_buffer) {
        free(pixels);
        free(draw_buffer);
        pixels = draw_buffer = NULL;
        return NULL;
    }
    display = lv_display_create(width, height);
    if (!display) {
        free(pixels);
        free(draw_buffer);
        pixels = draw_buffer = NULL;
        return NULL;
    }
    pixel_width = width;
    pixel_height = height;
    lv_display_set_color_format(display, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display, draw_buffer, NULL,
        (uint32_t)width * 32u * sizeof(uint32_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, headless_flush);
#endif
    running = true;
    return display;
}

bool lak_linux_display_running(void)
{
    return running;
}

void lak_linux_display_destroy(lv_display_t *display)
{
    lv_indev_t *input = lv_indev_get_next(NULL);
    while (input) {
        lv_indev_t *next = lv_indev_get_next(input);
        if (lv_indev_get_display(input) == display) lv_indev_delete(input);
        input = next;
    }
    if (display) lv_display_delete(display);
#if LV_USE_SDL
    SDL_SetEventFilter(NULL, NULL);
    lv_sdl_quit();
#elif !LV_USE_LINUX_FBDEV
    free(draw_buffer);
    free(pixels);
    pixels = draw_buffer = NULL;
#endif
    running = false;
}

bool lak_linux_display_snapshot(const char *path)
{
#if !LV_USE_SDL && !LV_USE_LINUX_FBDEV
    if (!path || !pixels) return false;
    FILE *file = fopen(path, "wb");
    if (!file) return false;
    bool ok = fprintf(file, "P6\n%d %d\n255\n", pixel_width, pixel_height) > 0;
    for (size_t i = 0; ok && i < (size_t)pixel_width * (size_t)pixel_height; ++i) {
        uint8_t rgb[3] = {(uint8_t)(pixels[i] >> 16), (uint8_t)(pixels[i] >> 8), (uint8_t)pixels[i]};
        ok = fwrite(rgb, 1, sizeof(rgb), file) == sizeof(rgb);
    }
    return fclose(file) == 0 && ok;
#else
    (void)path;
    return false;
#endif
}
