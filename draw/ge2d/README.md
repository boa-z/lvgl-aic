# GE2D draw unit

The GE2D backend is intentionally not implemented in Phase 1.

A later implementation must use an independent `lv_draw_aic_ge2d_unit_t`, not
`lv_draw_sw_unit_t`, and must route unsupported tasks back to the LVGL software
renderer.
