/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_input.h"

typedef struct {
    lv_aic_input_provider_t provider;
} lv_aic_input_ctx_t;

static void lv_aic_input_read(lv_indev_t * indev, lv_indev_data_t * data)
{
    lv_aic_input_ctx_t * ctx = (lv_aic_input_ctx_t *)lv_indev_get_driver_data(indev);
    if(ctx == NULL || ctx->provider.read_cb == NULL || data == NULL) return;
    ctx->provider.read_cb(indev, data, ctx->provider.user_data);
}

int lv_aic_input_init(lv_display_t * display, lv_indev_type_t type,
                      const lv_aic_input_provider_t * provider,
                      lv_indev_t ** out)
{
    if(display == NULL || provider == NULL || provider->read_cb == NULL || out == NULL)
        return LV_AIC_ERR_INVALID_STATE;
    *out = NULL;
    lv_aic_input_ctx_t * ctx = lv_malloc_zeroed(sizeof(*ctx));
    if(ctx == NULL) return LV_AIC_ERR_NO_MEMORY;
    ctx->provider = *provider;
    lv_indev_t * indev = lv_indev_create();
    if(indev == NULL) {
        lv_free(ctx);
        return LV_AIC_ERR_NO_MEMORY;
    }
    lv_indev_set_type(indev, type);
    lv_indev_set_read_cb(indev, lv_aic_input_read);
    lv_indev_set_driver_data(indev, ctx);
    lv_indev_set_display(indev, display);
    *out = indev;
    return LV_AIC_OK;
}

void lv_aic_input_deinit(lv_indev_t * indev)
{
    if(indev == NULL) return;
    lv_aic_input_ctx_t * ctx = (lv_aic_input_ctx_t *)lv_indev_get_driver_data(indev);
    lv_indev_set_driver_data(indev, NULL);
    lv_indev_set_read_cb(indev, NULL);
    lv_indev_delete(indev);
    lv_free(ctx);
}
