#include <assert.h>
#include <stdio.h>

#include <lvgl.h>
#include "lvgl_aic.h"

static int encoder_reads;
static int mouse_reads;

static void encoder_read(lv_indev_t * indev, lv_indev_data_t * data, void * user_data)
{
    (void)indev;
    (void)user_data;
    encoder_reads++;
    data->enc_diff = 2;
    data->state = LV_INDEV_STATE_PRESSED;
}

static void mouse_read(lv_indev_t * indev, lv_indev_data_t * data, void * user_data)
{
    (void)indev;
    (void)user_data;
    mouse_reads++;
    data->point.x = 17;
    data->point.y = 23;
    data->state = LV_INDEV_STATE_RELEASED;
}

int lv_aic_display_init(lv_display_t ** display)
{
    *display = lv_display_create(64, 64);
    return *display != NULL ? LV_AIC_OK : LV_AIC_ERR_NO_MEMORY;
}

void lv_aic_display_deinit(lv_display_t * display)
{
    if (display != NULL) lv_display_delete(display);
}

int main(void)
{
    lv_aic_input_provider_t encoder = { .read_cb = encoder_read, .user_data = NULL };
    lv_aic_input_provider_t mouse = { .read_cb = mouse_read, .user_data = NULL };
    lv_indev_data_t data;

    lv_init();
    assert(lv_aic_set_encoder_provider(&encoder) == LV_AIC_OK);
    assert(lv_aic_set_mouse_provider(&mouse) == LV_AIC_OK);
    assert(lv_aic_init() == LV_AIC_OK);

    lv_indev_t * encoder_indev = lv_aic_get_encoder_indev();
    lv_indev_t * mouse_indev = lv_aic_get_mouse_indev();
    assert(encoder_indev != NULL);
    assert(mouse_indev != NULL);
    assert(lv_indev_get_type(encoder_indev) == LV_INDEV_TYPE_ENCODER);
    assert(lv_indev_get_type(mouse_indev) == LV_INDEV_TYPE_POINTER);

    lv_indev_get_read_cb(encoder_indev)(encoder_indev, &data);
    assert(encoder_reads == 1);
    assert(data.enc_diff == 2);
    assert(data.state == LV_INDEV_STATE_PRESSED);

    lv_indev_get_read_cb(mouse_indev)(mouse_indev, &data);
    assert(mouse_reads == 1);
    assert(data.point.x == 17 && data.point.y == 23);
    assert(data.state == LV_INDEV_STATE_RELEASED);

    assert(lv_aic_set_encoder_provider(&encoder) == LV_AIC_ERR_INVALID_STATE);
    assert(lv_aic_set_mouse_provider(&mouse) == LV_AIC_ERR_INVALID_STATE);
    lv_aic_deinit();
    assert(lv_aic_get_encoder_indev() == NULL);
    assert(lv_aic_get_mouse_indev() == NULL);
    assert(lv_aic_set_encoder_provider(NULL) == LV_AIC_OK);
    assert(lv_aic_set_mouse_provider(NULL) == LV_AIC_OK);
    puts("PASS application-owned encoder and mouse providers");
    return 0;
}

