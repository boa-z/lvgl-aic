/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Interactive SDL2 host for the platform-only LVGL 9.6 smoke page.
 * This deliberately uses the same manual page as the target smoke app,
 * but does not emulate the ArtInChip framebuffer or touch hardware.
 */

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lvgl/lvgl.h>

#include "lv_aic_manual_test.h"

#define LVGL_AIC_SDL_WIDTH 800
#define LVGL_AIC_SDL_HEIGHT 480
#define LVGL_AIC_SDL_BYTES_PER_PIXEL 2U
#define LVGL_AIC_SDL_STRIDE (LVGL_AIC_SDL_WIDTH * LVGL_AIC_SDL_BYTES_PER_PIXEL)
#define LVGL_AIC_SDL_BUFFER_SIZE (LVGL_AIC_SDL_STRIDE * LVGL_AIC_SDL_HEIGHT)

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    lv_display_t *display;
    lv_indev_t *indev;
    int16_t mouse_x;
    int16_t mouse_y;
    bool mouse_pressed;
    bool running;
} lvgl_aic_sdl_ctx_t;

static LV_ATTRIBUTE_MEM_ALIGN uint8_t lvgl_aic_sdl_draw_buffer[LVGL_AIC_SDL_BUFFER_SIZE];
static lvgl_aic_sdl_ctx_t lvgl_aic_sdl_ctx;
static bool lvgl_aic_sdl_lv_initialized;

static void lvgl_aic_sdl_flush_cb(lv_display_t *display, const lv_area_t *area,
                                  uint8_t *px_map)
{
    (void)area;

    if ((lvgl_aic_sdl_ctx.texture != NULL) && (px_map != NULL) &&
        (SDL_UpdateTexture(lvgl_aic_sdl_ctx.texture, NULL, px_map,
                           LVGL_AIC_SDL_STRIDE) != 0)) {
        fprintf(stderr, "SDL texture update failed: %s\n", SDL_GetError());
    }

    if (lvgl_aic_sdl_ctx.renderer != NULL) {
        SDL_RenderClear(lvgl_aic_sdl_ctx.renderer);
        SDL_RenderCopy(lvgl_aic_sdl_ctx.renderer, lvgl_aic_sdl_ctx.texture,
                       NULL, NULL);
        SDL_RenderPresent(lvgl_aic_sdl_ctx.renderer);
    }

    lv_display_flush_ready(display);
}

static void lvgl_aic_sdl_mouse_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    if (data == NULL) {
        return;
    }

    data->point.x = lvgl_aic_sdl_ctx.mouse_x;
    data->point.y = lvgl_aic_sdl_ctx.mouse_y;
    data->state = lvgl_aic_sdl_ctx.mouse_pressed ? LV_INDEV_STATE_PRESSED
                                                  : LV_INDEV_STATE_RELEASED;
}

static void lvgl_aic_sdl_update_mouse_position(SDL_MouseMotionEvent *event)
{
    if ((event->x < 0) || (event->y < 0)) {
        return;
    }

    if (event->x >= LVGL_AIC_SDL_WIDTH) {
        lvgl_aic_sdl_ctx.mouse_x = LVGL_AIC_SDL_WIDTH - 1;
    } else {
        lvgl_aic_sdl_ctx.mouse_x = (int16_t)event->x;
    }

    if (event->y >= LVGL_AIC_SDL_HEIGHT) {
        lvgl_aic_sdl_ctx.mouse_y = LVGL_AIC_SDL_HEIGHT - 1;
    } else {
        lvgl_aic_sdl_ctx.mouse_y = (int16_t)event->y;
    }
}

static int lvgl_aic_sdl_save_screenshot(const char *path)
{
    SDL_Surface *surface;
    int result;

    if (path == NULL) {
        return 0;
    }

    surface = SDL_CreateRGBSurfaceWithFormat(0, LVGL_AIC_SDL_WIDTH,
                                            LVGL_AIC_SDL_HEIGHT, 32,
                                            SDL_PIXELFORMAT_ARGB8888);
    if (surface == NULL) {
        fprintf(stderr, "SDL screenshot surface failed: %s\n", SDL_GetError());
        return -1;
    }

    if (SDL_RenderReadPixels(lvgl_aic_sdl_ctx.renderer, NULL,
                             SDL_PIXELFORMAT_ARGB8888, surface->pixels,
                             surface->pitch) != 0) {
        fprintf(stderr, "SDL screenshot read failed: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return -1;
    }

    result = SDL_SaveBMP(surface, path);
    if (result != 0) {
        fprintf(stderr, "SDL screenshot save failed: %s\n", SDL_GetError());
    }
    SDL_FreeSurface(surface);
    return result;
}

static int lvgl_aic_sdl_init(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    lvgl_aic_sdl_ctx.window = SDL_CreateWindow(
        "lvgl-aic SDL smoke | LVGL 9.6 | 800x480",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        LVGL_AIC_SDL_WIDTH, LVGL_AIC_SDL_HEIGHT, SDL_WINDOW_SHOWN);
    if (lvgl_aic_sdl_ctx.window == NULL) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return -1;
    }

    lvgl_aic_sdl_ctx.renderer = SDL_CreateRenderer(
        lvgl_aic_sdl_ctx.window, -1, SDL_RENDERER_ACCELERATED);
    if (lvgl_aic_sdl_ctx.renderer == NULL) {
        lvgl_aic_sdl_ctx.renderer = SDL_CreateRenderer(
            lvgl_aic_sdl_ctx.window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (lvgl_aic_sdl_ctx.renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return -1;
    }

    lvgl_aic_sdl_ctx.texture = SDL_CreateTexture(
        lvgl_aic_sdl_ctx.renderer, SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING, LVGL_AIC_SDL_WIDTH, LVGL_AIC_SDL_HEIGHT);
    if (lvgl_aic_sdl_ctx.texture == NULL) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return -1;
    }

    lv_init();
    lvgl_aic_sdl_lv_initialized = true;
    lvgl_aic_sdl_ctx.display = lv_display_create(LVGL_AIC_SDL_WIDTH,
                                                  LVGL_AIC_SDL_HEIGHT);
    if (lvgl_aic_sdl_ctx.display == NULL) {
        fprintf(stderr, "lv_display_create failed\n");
        return -1;
    }

    lv_display_set_color_format(lvgl_aic_sdl_ctx.display,
                                LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(lvgl_aic_sdl_ctx.display, lvgl_aic_sdl_flush_cb);
    lv_display_set_buffers(lvgl_aic_sdl_ctx.display,
                           lvgl_aic_sdl_draw_buffer, NULL,
                           sizeof(lvgl_aic_sdl_draw_buffer),
                           LV_DISPLAY_RENDER_MODE_FULL);

    if (lv_aic_manual_test_create() != LV_AIC_OK) {
        fprintf(stderr, "lv_aic_manual_test_create failed\n");
        return -1;
    }

    lvgl_aic_sdl_ctx.indev = lv_indev_create();
    if (lvgl_aic_sdl_ctx.indev == NULL) {
        fprintf(stderr, "lv_indev_create failed\n");
        return -1;
    }
    lv_indev_set_type(lvgl_aic_sdl_ctx.indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(lvgl_aic_sdl_ctx.indev, lvgl_aic_sdl_mouse_read_cb);
    lv_indev_set_display(lvgl_aic_sdl_ctx.indev, lvgl_aic_sdl_ctx.display);

    lvgl_aic_sdl_ctx.mouse_x = 0;
    lvgl_aic_sdl_ctx.mouse_y = 0;
    lvgl_aic_sdl_ctx.mouse_pressed = false;
    lvgl_aic_sdl_ctx.running = true;
    return 0;
}

static void lvgl_aic_sdl_deinit(void)
{
    if (lvgl_aic_sdl_ctx.indev != NULL) {
        lv_indev_delete(lvgl_aic_sdl_ctx.indev);
        lvgl_aic_sdl_ctx.indev = NULL;
    }

    lv_aic_manual_test_deinit();
    if (lvgl_aic_sdl_ctx.display != NULL) {
        lv_display_delete(lvgl_aic_sdl_ctx.display);
        lvgl_aic_sdl_ctx.display = NULL;
    }
    if (lvgl_aic_sdl_lv_initialized) {
        lv_deinit();
        lvgl_aic_sdl_lv_initialized = false;
    }

    if (lvgl_aic_sdl_ctx.texture != NULL) {
        SDL_DestroyTexture(lvgl_aic_sdl_ctx.texture);
        lvgl_aic_sdl_ctx.texture = NULL;
    }
    if (lvgl_aic_sdl_ctx.renderer != NULL) {
        SDL_DestroyRenderer(lvgl_aic_sdl_ctx.renderer);
        lvgl_aic_sdl_ctx.renderer = NULL;
    }
    if (lvgl_aic_sdl_ctx.window != NULL) {
        SDL_DestroyWindow(lvgl_aic_sdl_ctx.window);
        lvgl_aic_sdl_ctx.window = NULL;
    }
    SDL_Quit();
}

static void lvgl_aic_sdl_usage(const char *program)
{
    printf("Usage: %s [--frames N] [--screenshot PATH] [--self-test]\n", program);
    printf("  --frames N          exit after N event-loop iterations\n");
    printf("  --screenshot PATH   save a BMP after the first rendered frame\n");
    printf("  --self-test         inject a mouse click on the smoke button\n");
}

int main(int argc, char **argv)
{
    const char *screenshot_path = NULL;
    bool self_test = false;
    unsigned long frame_limit = 0;
    unsigned long frame_count = 0;
    uint32_t last_tick = 0;
    int result = 0;

    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--help") == 0) ||
            (strcmp(argv[i], "-h") == 0)) {
            lvgl_aic_sdl_usage(argv[0]);
            return 0;
        }
        if ((strcmp(argv[i], "--frames") == 0) && ((i + 1) < argc)) {
            frame_limit = strtoul(argv[++i], NULL, 10);
            continue;
        }
        if ((strcmp(argv[i], "--screenshot") == 0) && ((i + 1) < argc)) {
            screenshot_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--self-test") == 0) {
            self_test = true;
            continue;
        }
        fprintf(stderr, "Unknown argument: %s\n", argv[i]);
        lvgl_aic_sdl_usage(argv[0]);
        return 2;
    }

    if (lvgl_aic_sdl_init() != 0) {
        lvgl_aic_sdl_deinit();
        return 1;
    }

    last_tick = SDL_GetTicks();
    lv_refr_now(lvgl_aic_sdl_ctx.display);
    if (screenshot_path != NULL) {
        result = lvgl_aic_sdl_save_screenshot(screenshot_path);
        if (result != 0) {
            lvgl_aic_sdl_deinit();
            return 1;
        }
    }

    while (lvgl_aic_sdl_ctx.running) {
        SDL_Event event;
        uint32_t now;
        uint32_t delay_ms;

        if (self_test && (frame_count == 1)) {
            /* Button bounds are x=24..203, y=80..135. */
            lvgl_aic_sdl_ctx.mouse_x = 114;
            lvgl_aic_sdl_ctx.mouse_y = 108;
            lvgl_aic_sdl_ctx.mouse_pressed = true;
        } else if (self_test && (frame_count == 3)) {
            lvgl_aic_sdl_ctx.mouse_pressed = false;
        }

        while (SDL_PollEvent(&event) != 0) {
            switch (event.type) {
            case SDL_QUIT:
                lvgl_aic_sdl_ctx.running = false;
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    lvgl_aic_sdl_ctx.running = false;
                }
                break;
            case SDL_MOUSEMOTION:
                lvgl_aic_sdl_update_mouse_position(&event.motion);
                lvgl_aic_sdl_ctx.mouse_pressed =
                    (event.motion.state & SDL_BUTTON_LMASK) != 0;
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    lvgl_aic_sdl_ctx.mouse_x = (int16_t)event.button.x;
                    lvgl_aic_sdl_ctx.mouse_y = (int16_t)event.button.y;
                    lvgl_aic_sdl_ctx.mouse_pressed = true;
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    lvgl_aic_sdl_ctx.mouse_x = (int16_t)event.button.x;
                    lvgl_aic_sdl_ctx.mouse_y = (int16_t)event.button.y;
                    lvgl_aic_sdl_ctx.mouse_pressed = false;
                }
                break;
            default:
                break;
            }
        }

        now = SDL_GetTicks();
        lv_tick_inc(now - last_tick);
        last_tick = now;

        delay_ms = lv_timer_handler();
        if (delay_ms == LV_NO_TIMER_READY) {
            delay_ms = 10;
        } else if (delay_ms == 0) {
            delay_ms = 1;
        } else if (delay_ms > 100) {
            delay_ms = 100;
        }
        SDL_Delay(delay_ms);

        frame_count++;
        if (self_test && (frame_count >= 8)) {
            const char *status = lv_aic_manual_test_status_text();
            if ((status == NULL) ||
                (strcmp(status, "button event received") != 0)) {
                fprintf(stderr, "SDL mouse self-test failed: status=%s\n",
                        (status == NULL) ? "<null>" : status);
                result = 1;
            }
            lvgl_aic_sdl_ctx.running = false;
        }
        if ((frame_limit != 0) && (frame_count >= frame_limit)) {
            lvgl_aic_sdl_ctx.running = false;
        }
    }

    if (self_test && (result == 0)) {
        const char *status = lv_aic_manual_test_status_text();
        if ((status == NULL) ||
            (strcmp(status, "button event received") != 0)) {
            fprintf(stderr, "SDL mouse self-test did not complete\n");
            result = 1;
        }
    }

    lvgl_aic_sdl_deinit();
    return result;
}
