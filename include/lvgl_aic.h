/**
 * @file lvgl_aic.h
 * @brief Public API for the ArtInChip LVGL platform port.
 */

#ifndef LVGL_AIC_H
#define LVGL_AIC_H

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include <lvgl/lvgl.h>
#endif

#if (LVGL_VERSION_MAJOR != 9) || (LVGL_VERSION_MINOR != 6)
#error "lvgl-aic currently supports LVGL 9.6.x only"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Return codes used by the platform integration API. */
typedef enum {
    LV_AIC_OK = 0,
    LV_AIC_ERR_INVALID_STATE = -1,
    LV_AIC_ERR_NO_BSP = -2,
    LV_AIC_ERR_DISPLAY = -3,
    LV_AIC_ERR_INPUT = -4,
    LV_AIC_ERR_NO_MEMORY = -5,
    LV_AIC_ERR_UNSUPPORTED = -6,
} lv_aic_result_t;

/** Application-owned input sampler for optional encoder and mouse devices. */
typedef void (*lv_aic_input_read_cb_t)(lv_indev_t * indev,
                                       lv_indev_data_t * data,
                                       void * user_data);

typedef struct {
    lv_aic_input_read_cb_t read_cb;
    void * user_data;
} lv_aic_input_provider_t;

/**
 * @brief Initialize ArtInChip display and input integration.
 *
 * The caller must have called lv_init() and must own the LVGL task loop.
 * The function does not create product UI objects or start protocol threads.
 *
 * @return LV_AIC_OK on success, otherwise a negative lv_aic_result_t.
 */
int lv_aic_init(void);

/** Register a sampler before lv_aic_init(); NULL clears the provider. */
int lv_aic_set_encoder_provider(const lv_aic_input_provider_t * provider);
int lv_aic_set_mouse_provider(const lv_aic_input_provider_t * provider);

/**
 * @brief Deinitialize the ArtInChip integration.
 *
 * Call this only after the LVGL task/flush callbacks have been stopped and
 * all image-decoder descriptors closed. Pending MPP readers refuse teardown;
 * close them and call again.
 */
void lv_aic_deinit(void);

/** @brief Return the LVGL display created by the port, or NULL. */
lv_display_t *lv_aic_get_display(void);

/** @brief Return the LVGL pointer input device, or NULL. */
lv_indev_t *lv_aic_get_pointer_indev(void);
lv_indev_t *lv_aic_get_encoder_indev(void);
lv_indev_t *lv_aic_get_mouse_indev(void);

#if AIC_LVGL_USE_MPP_DEC
/** Component-owned decoded-image cache; call only from the serialized LVGL owner.
 * Drop a source BEFORE replacing/freeing its encoded bytes or descriptor, or
 * rewriting its file. NULL drops all entries. Active readers stay alive until
 * close. This also drops LVGL header metadata; lv_image_cache_drop alone does
 * not invalidate this cache. The limit bounds retained buffers + metadata,
 * with at most 16 entries. In-flight uncached decoding is outside that budget.
 */
typedef struct {
    uint32_t hits, misses, evictions;
    uint32_t entries, bytes, limit_bytes;
} lv_aic_mpp_cache_stats_t;
void lv_aic_mpp_cache_set_limit(uint32_t bytes);
void lv_aic_mpp_cache_drop(const void *src);
const lv_aic_mpp_cache_stats_t *lv_aic_mpp_cache_stats(void);

/** One retained cache entry, for diagnostics (path is NULL for memory sources). */
typedef struct {
    const char *path;
    uint32_t width, height, bytes, refs;
    lv_color_format_t color_format;
    bool premultiply;
} lv_aic_mpp_cache_entry_t;
/** Visit retained entries newest first; owner thread only, do not mutate the
 * cache from @p fn. */
void lv_aic_mpp_cache_foreach(void (*fn)(const lv_aic_mpp_cache_entry_t *entry, void *user),
                              void *user);

/** @brief Return the MPP image decoder owned by the port, or NULL. */
lv_image_decoder_t *lv_aic_get_mpp_decoder(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* LVGL_AIC_H */
