/* SPDX-License-Identifier: Apache-2.0 */
#include "lv_aic_display_fit.h"

bool lv_aic_display_fit(int32_t vw, int32_t vh, int32_t pw, int32_t ph,
                        lv_aic_display_fit_t *fit)
{
    if (fit == 0 || vw <= 0 || vh <= 0 || pw <= 0 || ph <= 0) {
        return false;
    }
    /* Width-bound when the panel is relatively taller than the design. */
    if ((int64_t)pw * vh <= (int64_t)ph * vw) {
        fit->w = pw;
        fit->h = (int32_t)(((int64_t)vh * pw + vw / 2) / vw);
    } else {
        fit->h = ph;
        fit->w = (int32_t)(((int64_t)vw * ph + vh / 2) / vh);
    }
    if (fit->w < 1) fit->w = 1;
    if (fit->h < 1) fit->h = 1;
    if (fit->w > pw) fit->w = pw;
    if (fit->h > ph) fit->h = ph;
    fit->x = (pw - fit->w) / 2;
    fit->y = (ph - fit->h) / 2;
    return true;
}

static int32_t map_axis(int32_t p, int32_t origin, int32_t scaled, int32_t virt)
{
    int64_t v;
    if (p <= origin) {
        return 0;
    }
    if (p >= origin + scaled) {
        return virt - 1;
    }
    /* Center of panel pixel p, back-projected: ((p - origin) + 0.5) * virt / scaled. */
    v = ((int64_t)(2 * (p - origin) + 1) * virt) / (2 * (int64_t)scaled);
    return v >= virt ? virt - 1 : (int32_t)v;
}

void lv_aic_display_fit_map(const lv_aic_display_fit_t *fit, int32_t vw, int32_t vh,
                            int32_t *x, int32_t *y)
{
    if (fit == 0 || x == 0 || y == 0 || fit->w <= 0 || fit->h <= 0) {
        return;
    }
    *x = map_axis(*x, fit->x, fit->w, vw);
    *y = map_axis(*y, fit->y, fit->h, vh);
}
