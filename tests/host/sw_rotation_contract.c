/* SPDX-License-Identifier: Apache-2.0 */
#include "lvgl.h"
#include "lvgl_private.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

enum { W = 128, H = 128, SRC = 32 };
static uint32_t pixels[SRC * SRC], full[W * H];
static const uint32_t colors[] = {0xff336699, 0xffd04020, 0xff20b050, 0xff9050c0};
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p)
{ (void)a; (void)p; lv_display_flush_ready(d); }

static void render(lv_obj_t *canvas, lv_draw_image_dsc_t *d, const lv_area_t *area, bool tiled)
{
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);
    /* Deliberately nonzero layer origin, like a translated offscreen child. */
    for(int y = 0; y < H; y += tiled ? 13 : H) {
        for(int x = 0; x < W; x += tiled ? 17 : W) {
            lv_layer_t layer;
            lv_canvas_init_layer(canvas, &layer);
            layer.buf_area = (lv_area_t){90, 190, 90 + W - 1, 190 + H - 1};
            layer._clip_area = (lv_area_t){90 + x, 190 + y,
                90 + LV_MIN(x + (tiled ? 17 : W), W) - 1,
                190 + LV_MIN(y + (tiled ? 13 : H), H) - 1};
            lv_draw_image(&layer, d, area);
            lv_canvas_finish_layer(canvas, &layer);
        }
    }
}

int main(void)
{
    lv_init();
    lv_display_t *disp = lv_display_create(W, H);
    assert(disp);
    lv_display_set_flush_cb(disp, flush);
    lv_obj_t *canvas = lv_canvas_create(lv_screen_active());
    lv_draw_buf_t *buf = lv_draw_buf_create(W, H, LV_COLOR_FORMAT_ARGB8888, 0);
    assert(buf && buf->header.stride == W * 4);
    lv_canvas_set_draw_buf(canvas, buf);
    for(unsigned y = 0; y < SRC; y++) for(unsigned x = 0; x < SRC; x++)
        pixels[y * SRC + x] = colors[(x >= 16) + 2 * (y >= 16)];
    lv_image_dsc_t src = {.header = {.magic = LV_IMAGE_HEADER_MAGIC, .w = SRC, .h = SRC,
        .stride = SRC * 4, .cf = LV_COLOR_FORMAT_ARGB8888}, .data = (uint8_t *)pixels,
        .data_size = sizeof pixels};
    const lv_point_t pivots[] = {{16,16}, {0,0}, {16384,16384}, {-16384,16384},
                               {32768,-32768}};
    const int angles[] = {0, -1, -451, 3601, 1, 9, 450, 451, 899, 900, 1800, 2700, 3591, 3599};
    const int scales[][2] = {{256,256}, {128,384}, {512,512}};
    unsigned scenes = 0, checked = 0;
    for(unsigned p = 0; p < sizeof pivots / sizeof pivots[0]; p++)
    for(unsigned a = 0; a < sizeof angles / sizeof angles[0]; a++)
    for(unsigned z = 0; z < sizeof scales / sizeof scales[0]; z++) {
        double theta = angles[a] * 3.14159265358979323846 / 1800.0;
        double c = cos(theta), s = sin(theta);
        double sx = scales[z][0] / 256.0, sy = scales[z][1] / 256.0;
        double cx = (16 - pivots[p].x) * sx, cy = (16 - pivots[p].y) * sy;
        int ox = (int)floor(154 - (c * cx - s * cy + pivots[p].x));
        int oy = (int)floor(254 - (s * cx + c * cy + pivots[p].y));
        lv_area_t area = {ox, oy, ox + SRC - 1, oy + SRC - 1};
        /* Independent forward geometry oracle, including both scale orders. */
        for(unsigned order = 0; order < 2; order++) {
            lv_point_t corners[] = {{0,0}, {31,0}, {0,31}, {31,31}};
            lv_point_array_transform(corners, 4, angles[a], scales[z][0], scales[z][1], &pivots[p], order);
            for(unsigned k = 0; k < 4; k++) {
                double x = (k & 1 ? 31 : 0) - pivots[p].x;
                double y = (k & 2 ? 31 : 0) - pivots[p].y;
                double ex = order ? c * x * sx - s * y * sy : (c * x - s * y) * sx;
                double ey = order ? s * x * sx + c * y * sy : (s * x + c * y) * sy;
                /* Q15 degree-table interpolation + integer floor: <= 4 px at 32768 pivot/2x. */
                if(fabs(corners[k].x - (ex + pivots[p].x)) > 4 ||
                   fabs(corners[k].y - (ey + pivots[p].y)) > 4) {
                    fprintf(stderr, "geometry p=%u a=%d z=%u order=%u k=%u got=%d,%d expected=%.3f,%.3f\n",
                            p, angles[a], z, order, k, corners[k].x, corners[k].y,
                            ex + pivots[p].x, ey + pivots[p].y);
                    return 1;
                }
            }
        }
        for(unsigned aa = 0; aa < 2; aa++) {
            lv_draw_image_dsc_t d;
            lv_draw_image_dsc_init(&d);
            d.src = &src; d.rotation = angles[a]; d.pivot = pivots[p];
            d.scale_x = scales[z][0]; d.scale_y = scales[z][1]; d.antialias = aa;
            render(canvas, &d, &area, false);
            memcpy(full, buf->data, sizeof full);
            unsigned interior = 0;
            for(int y = 0; y < H; y++) for(int x = 0; x < W; x++) {
                double dx = 90 + x - ox - pivots[p].x;
                double dy = 190 + y - oy - pivots[p].y;
                double ix = (c * dx + s * dy) / sx + pivots[p].x;
                double iy = (-s * dx + c * dy) / sy + pivots[p].y;
                /* Exclude table/nearest/bilinear boundary uncertainty, not the interior. */
                bool xin = (ix >= 4 && ix <= 11) || (ix >= 20 && ix <= 27);
                bool yin = (iy >= 4 && iy <= 11) || (iy >= 20 && iy <= 27);
                if(ix < -4 || iy < -4 || ix > SRC + 4 || iy > SRC + 4)
                    assert(full[y * W + x] == 0);
                if(xin && yin) {
                    uint32_t expected = colors[(ix >= 16) + 2 * (iy >= 16)];
                    if(full[y * W + x] != expected) {
                        fprintf(stderr, "pixel p=%u a=%d z=%u aa=%u xy=%d,%d src=%.2f,%.2f got=%08x expected=%08x\n",
                                p, angles[a], z, aa, x, y, ix, iy, full[y * W + x], expected);
                        return 1;
                    }
                    interior++;
                }
            }
            assert(interior >= 100);
            checked += interior;
            render(canvas, &d, &area, true);
            assert(!memcmp(full, buf->data, sizeof full));
            scenes++;
        }
    }
    double max_geometry_error = 0;
    for(int angle = 0; angle < 3600; angle++) {
        double c = cos(angle * 3.14159265358979323846 / 1800.0);
        double s = sin(angle * 3.14159265358979323846 / 1800.0);
        for(unsigned p = 0; p < sizeof pivots / sizeof pivots[0]; p++)
        for(unsigned z = 0; z < sizeof scales / sizeof scales[0]; z++)
        for(unsigned order = 0; order < 2; order++) {
            lv_point_t point = {0,31};
            double x = -pivots[p].x, y = 31 - pivots[p].y;
            double sx = scales[z][0] / 256.0, sy = scales[z][1] / 256.0;
            double ex = (order ? c*x*sx-s*y*sy : (c*x-s*y)*sx) + pivots[p].x;
            double ey = (order ? s*x*sx+c*y*sy : (s*x+c*y)*sy) + pivots[p].y;
            lv_point_transform(&point, angle, scales[z][0], scales[z][1], &pivots[p], order);
            double error = fmax(fabs(point.x-ex), fabs(point.y-ey));
            if(error > max_geometry_error) max_geometry_error = error;
            if(error > 4) {
                fprintf(stderr, "sweep angle=%d p=%u z=%u order=%u error=%.3f\n", angle,p,z,order,error);
                return 1;
            }
        }
    }
    printf("PASS 108000 full-circle geometry probes; maximum error %.3f pixels\n", max_geometry_error);
    lv_image_cache_drop(&src);
    lv_obj_delete(canvas); lv_draw_buf_destroy(buf); lv_display_delete(disp); lv_deinit();
    printf("PASS %u rotated scenes, %u independent interior pixels, exact partial rendering\n", scenes, checked);
    return 0;
}
