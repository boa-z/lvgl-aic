#include <lvgl/lvgl.h>
#include "lv_aic_manual_test.h"

int main(void)
{
    lv_display_t *display;
    int result;

    lv_init();
    display = lv_display_create(800, 480);
    if (display == NULL) {
        return 1;
    }

    result = lv_aic_manual_test_create();
    if (result != LV_AIC_OK) {
        lv_display_delete(display);
        return 2;
    }

    for (int i = 0; i < 5; ++i) {
        (void)lv_timer_handler();
    }

    lv_aic_manual_test_deinit();
    lv_display_delete(display);
    lv_deinit();
    return 0;
}
