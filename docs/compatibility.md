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

Display and touch code use public LVGL APIs. Any future GE2D implementation
must isolate required private headers behind `compat/lvgl_aic_private.h` and
record the dependency in this document and `porting-notes.md`.

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
| Repository skeleton | in progress |
| LVGL 9.6 host compile | pending |
| D13x software display | pending |
| GT911 touch | pending |
| VSync/PAN/rotation board test | pending |
| MPP decoder | not started |
| GE2D | not started |

A missing board test must be reported as `Hardware validation pending`.
