# Compatibility

## Supported baseline

| Item | Fixed value |
|---|---|
| LVGL tag | `v9.6.0` |
| LVGL commit | `80ca777e37a2b176770726a02e07a6fb79ef0b39` |
| Luban-Lite reference | `c5807f9e7d18292f920dafaa018b8174635085c4` |
| Primary SoC | ArtInChip D13x / D133ECS |
| Kernel | RT-Thread |
| Renderer in Phase 1 | LVGL software renderer |

The compile-time guard in `compat/lvgl_aic_compat.h` intentionally rejects
LVGL versions other than 9.6.x.

## Public/private API policy

Display and touch code use public LVGL APIs. The GE2D implementation lives in
`draw/ge2d/` and, like the MPP decoder, isolates the private headers it needs
behind `compat/lvgl_aic_private.h`.

MPP decoder depends on LVGL 9.6 image decoder private API.
`image/mpp/lv_aic_mpp_decoder.c` and the three
`draw/ge2d/lv_draw_aic_ge2d*.c` files are the only translation units that
enable `AIC_LVGL_USE_PRIVATE_API` and include
`compat/lvgl_aic_private.h`; format and stream helpers stay public-only.

The GE2D unit needs the private header for the `lv_draw_task_t` and
`lv_draw_unit_t` definitions it reads (`state`, `type`, `draw_dsc`,
`preference_score`, `preferred_draw_unit_id`, `target_layer`, `clip_area`) and
for the `lv_draw_aic_ge2d_unit_t` base member. The create/dispatch entry points
it calls (`lv_draw_create_unit`, `lv_draw_get_available_task`,
`lv_draw_layer_alloc_buf`, `lv_draw_dispatch_request`) are all public. It does
not depend on any other private structure.

## OS integration decision

The configuration selects LVGL 9.6's `LV_OS_RTTHREAD` backend and sets the
software draw-thread priority explicitly. The Phase 1 touch producer uses the
ArtInChip OSAL (`aicos_thread_t`, `aicos_sem_t`, `aicos_mutex_t`) rather than
LVGL 9.6 private `lv_thread_*` types, because those types are no longer public.
This is a BSP event-source integration and does not duplicate LVGL's core
object/timer implementation.

The touch callback runs in the RT-Thread device callback/worker path and only
copies state. It does not call LVGL from an ISR.

## Validation status

| Area | Status |
|---|---|
| Repository skeleton | complete |
| LVGL 9.6 host compile | PASS |
| D13x software display | hardware validation pending |
| GT911 touch | hardware validation pending |
| VSync/PAN/rotation board test | hardware validation pending |
| MPP decoder | code complete, board validation pending |
| GE2D draw unit (opaque FILL) | code complete, board validation pending |

A missing board test must be reported as `Hardware validation pending`.
