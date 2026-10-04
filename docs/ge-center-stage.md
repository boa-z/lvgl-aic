# GE ROTATE center bounds

Both checked-out SDK register headers pack SRC_ROT1_CENTER_SET and
DST_ROT1_CENTER_SET with a 0x3fff mask per component. The original executor
validated rectangle sizes but passed unbounded source and destination centers.
For example, pivot 16384 silently aliases zero in the command registers.

The component now uses a conservative signed 14-bit domain (-8192..8191) for
both the source pivot and crop-relative destination center. It computes
image_origin + pivot - destination_clip_origin in int64_t, checks the complete
result and only then narrows. Rejected requests perform no cache maintenance
or GE submission, including tile preflight. Ordinary executor fallback remains
available and source lifetime handling is unchanged. This domain is a port
acceptance policy; it does not claim new board validation of register endpoints.

Tests cover 1,210 boundary/large-translation combinations, unchanged outputs on
rejection, null arguments, invalid centers at both preflight and execution, and
real IMAGE/LAYER software pixels for a visible translated 45-degree source
with a remote pivot. The initial reproducer failed because the executor
submitted an unrepresentable center. Existing tenth-degree trigonometry and
orthogonal scale/crop contracts remain in place.

## Remaining transform work

The SDK ROTATE implementation explicitly calls ge_scaler0_enable(..., 0).
Arbitrary-angle scaling cannot be enabled by relaxing the evaluator alone;
it still needs a checked multi-pass geometry, alpha, buffer-budget and DMA
lifetime design. This stage does not add that capability.

Native LVGL software also quantizes forward/inverse rotation to Q10. With very
remote pivots its bounds and sampled pixels can diverge. An exploratory 0.1
degree case with pivot (16384,16384), source 32x32 and origin (100,230) produced
no visible software pixels despite an ideal visible transform. A 45-degree
case with origin (-16240,6982) rendered, but its central ideal sample differed
from the actual sampled region. The regression verifies handoff and an
interior pixel shared by the ideal/native footprints; it does not prove
whole-domain software accuracy. Higher-precision native fallback is a remaining
gap and must not be hidden by the GE command guard.

Host/full-firmware results will be recorded in validation.md. SDK/LVGL source
files remain unchanged; physical rotation/center limits **NOT_RUN**.
