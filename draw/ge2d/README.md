# GE2D draw unit

Phase 3A implements this backend for exactly one task type:
`LV_DRAW_TASK_TYPE_FILL`, opaque, `radius == 0`, no gradient, a supported
destination format and a GE-addressable buffer.

It uses an independent `lv_draw_aic_ge2d_unit_t` (`base_unit` + `task_act`), not
`lv_draw_sw_unit_t`, and routes every unsupported task back to the LVGL software
renderer by declining it in `evaluate()`.

## Files

| File | Responsibility |
| --- | --- |
| `lv_draw_aic_ge2d.{c,h}` | Registration, `evaluate`, `dispatch`, `delete`, counters |
| `lv_draw_aic_ge2d_fill.c` | One opaque `ge_fillrect` -> `mpp_ge_emit` -> `mpp_ge_sync` |
| `lv_draw_aic_ge2d_utils.{c,h}` | GE address window, destination format, destination cache prep |

The LVGL <-> MPP format translation lives in `common/lv_aic_pixel_format.{c,h}`,
because the decoder needs it too.

## Contract

- Execution is synchronous on the dispatching thread. There is no render thread,
  no task queue and no saved layer/clip state; `task_act` only enforces "one task
  in flight".
- `evaluate()` returns 1 on acceptance and claims at preference score 70. The
  software unit claims at `>= 100`, so 70 wins, and a declined task still reaches
  software.
- `dispatch()` keeps an explicit `preferred_draw_unit_id != AIC_GE2D_DRAW_UNIT_ID`
  check: `lv_draw_get_available_task()` also returns tasks whose
  `preferred_draw_unit_id` is `LV_DRAW_UNIT_NONE`.
- A GE failure marks the task `LV_DRAW_TASK_STATE_FAILED`, never `FINISHED`.
- If `mpp_ge_open()` fails the unit is still registered and declines everything,
  so software rendering keeps the display alive.

Not in scope for Phase 3A, and not started: IMAGE, LAYER, scale, rotate.

See `docs/validation.md` for the current verification status.
