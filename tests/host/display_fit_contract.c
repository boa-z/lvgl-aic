/* SPDX-License-Identifier: Apache-2.0 */
/* Virtual-resolution geometry: the letterbox fit of a design resolution into
 * the panel and the inverse panel->LVGL point map used by touch. */
#include "lv_aic_display_fit.h"

#include <assert.h>
#include <stddef.h>

int main(void)
{
    lv_aic_display_fit_t fit;

    /* SDK 1024x600 demos on the D50T 800x480 panel: width-bound, centered. */
    assert(lv_aic_display_fit(1024, 600, 800, 480, &fit));
    assert(fit.x == 0 && fit.w == 800);
    assert(fit.h == 469 && fit.y == 5);

    /* Identity and pillarbox (height-bound) cases. */
    assert(lv_aic_display_fit(800, 480, 800, 480, &fit));
    assert(fit.x == 0 && fit.y == 0 && fit.w == 800 && fit.h == 480);
    assert(lv_aic_display_fit(480, 272, 800, 480, &fit)); /* SDK 480x272 set */
    assert(fit.x == 0 && fit.w == 800 && fit.h == 453 && fit.y == 13);
    assert(lv_aic_display_fit(400, 400, 800, 480, &fit));
    assert(fit.w == 480 && fit.h == 480 && fit.x == 160 && fit.y == 0);
    assert(!lv_aic_display_fit(0, 600, 800, 480, &fit));
    assert(!lv_aic_display_fit(1024, 600, 800, 480, NULL));

    /* Inverse map: corners, center, and the letterbox bars clamp to edges. */
    assert(lv_aic_display_fit(1024, 600, 800, 480, &fit));
    {
        int32_t x = 0, y = 5;
        lv_aic_display_fit_map(&fit, 1024, 600, &x, &y);
        assert(x == 0 && y == 0);
        x = 799; y = 5 + 469 - 1;
        lv_aic_display_fit_map(&fit, 1024, 600, &x, &y);
        assert(x == 1023 && y == 599);
        x = 400; y = 5 + 234; /* the fit is 469 rows: its middle row is 239 */
        lv_aic_display_fit_map(&fit, 1024, 600, &x, &y);
        assert(x == 512 && y == 300);
        x = 10; y = 0; /* top bar */
        lv_aic_display_fit_map(&fit, 1024, 600, &x, &y);
        assert(y == 0);
        x = 10; y = 479; /* bottom bar */
        lv_aic_display_fit_map(&fit, 1024, 600, &x, &y);
        assert(y == 599);
    }
    /* Every panel pixel inside the fit maps into range and monotonically. */
    {
        int32_t last = -1;
        for (int32_t p = 0; p < 800; p++) {
            int32_t x = p, y = 240;
            lv_aic_display_fit_map(&fit, 1024, 600, &x, &y);
            assert(x >= 0 && x < 1024 && x >= last);
            last = x;
        }
    }
    return 0;
}
