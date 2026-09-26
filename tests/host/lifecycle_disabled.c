/* Disabled feature guards must not call an unavailable backend. */
#include <assert.h>
#include "lv_aic_display.h"
#include "lv_aic_indev.h"
static int releases;
int lv_aic_display_init(lv_display_t **out) { *out=(lv_display_t *)(uintptr_t)1; return LV_AIC_OK; }
void lv_aic_display_deinit(lv_display_t *display) { assert(display != NULL); releases++; }
int lv_aic_indev_init(lv_display_t *display, lv_indev_t **out) { (void)display; (void)out; assert(0); return -1; }
void lv_aic_indev_deinit(lv_indev_t *indev) { (void)indev; assert(0); }
int lv_aic_mpp_decoder_init(lv_image_decoder_t **out) { (void)out; assert(0); return -1; }
void lv_aic_mpp_decoder_deinit(lv_image_decoder_t *decoder) { (void)decoder; assert(0); }
int main(void) {
    for (int i=0; i<3; i++) {
        assert(lv_aic_init()==LV_AIC_OK);
        assert(lv_aic_init()==LV_AIC_ERR_INVALID_STATE);
        assert(lv_aic_get_pointer_indev()==NULL);
        lv_aic_deinit();
        lv_aic_deinit();
        assert(lv_aic_get_display()==NULL);
    }
    assert(releases==3);
    return 0;
}
