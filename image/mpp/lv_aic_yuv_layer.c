/* SPDX-License-Identifier: Apache-2.0 */
#define AIC_LVGL_USE_PRIVATE_API 1
#include "lv_aic_yuv_layer.h"
#include "lv_aic_yuv_layer_private.h"

struct lv_aic_yuv_layer {
    lv_layer_t *layer;
    lv_aic_yuv_frame_t frame;
    lv_aic_yuv_release_cb_t release;
    void *context;
    struct lv_aic_yuv_layer *next;
    uint32_t readers;
    bool retired;
};

static lv_aic_yuv_layer_t *bindings;

static lv_aic_yuv_layer_t *find_binding(const lv_layer_t *layer)
{
    for (lv_aic_yuv_layer_t *binding = bindings; binding; binding = binding->next)
        if (binding->layer == layer) return binding;
    return NULL;
}

static bool planar_layer_format(lv_aic_yuv_format_t format)
{
    return format == LV_COLOR_FORMAT_I420 || format == LV_COLOR_FORMAT_I422 ||
           format == LV_COLOR_FORMAT_I444 || format == LV_COLOR_FORMAT_I400;
}

static void free_binding(lv_aic_yuv_layer_t *binding)
{
    lv_aic_yuv_layer_t **link = &bindings;
    while (*link && *link != binding) link = &(*link)->next;
    if (*link) *link = binding->next;
    binding->release(binding->context);
    lv_free(binding);
}

bool lv_aic_yuv_layer_attach(lv_layer_t *layer, const lv_aic_yuv_frame_t *frame,
                             lv_aic_yuv_retain_cb_t retain_cb,
                             lv_aic_yuv_release_cb_t release_cb,
                             void *context)
{
    if (!layer || !frame || !retain_cb || !release_cb || find_binding(layer) ||
        !planar_layer_format(frame->format) || layer->color_format != frame->format ||
        !lv_aic_yuv_validate(frame)) return false;

    lv_aic_yuv_layer_t *binding = lv_malloc_zeroed(sizeof(*binding));
    if (!binding || !retain_cb(context)) {
        if (binding) lv_free(binding);
        return false;
    }
    binding->layer = layer;
    binding->frame = *frame;
    binding->release = release_cb;
    binding->context = context;
    binding->next = bindings;
    bindings = binding;
    return true;
}

bool lv_aic_yuv_layer_detach(lv_layer_t *layer)
{
    lv_aic_yuv_layer_t *binding = find_binding(layer);
    if (!binding || binding->retired) return false;
    binding->retired = true;
    if (!binding->readers) free_binding(binding);
    return true;
}

lv_aic_yuv_layer_t *lv_aic_yuv_layer_acquire(const lv_layer_t *layer,
                                             const lv_aic_yuv_frame_t **frame)
{
    lv_aic_yuv_layer_t *binding = find_binding(layer);
    if (!frame || !binding || binding->retired || binding->readers == UINT32_MAX)
        return NULL;
    binding->readers++;
    *frame = &binding->frame;
    return binding;
}

void lv_aic_yuv_layer_release_lease(lv_aic_yuv_layer_t *binding)
{
    if (!binding || !binding->readers) return;
    binding->readers--;
    if (binding->retired && !binding->readers) free_binding(binding);
}
