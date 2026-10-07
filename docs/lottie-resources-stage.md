# Checked Lottie source replacement

With `AIC_LVGL_USE_LOTTIE`, include `lv_aic_lottie.h` and use
`lv_aic_lottie_load_data` or `lv_aic_lottie_load_file`. Both operate on an existing
native Lottie widget with a configured canvas buffer and a live native animation.
They return distinct INVALID, LIMIT, IO, NO_MEMORY, DECODE, RENDER and BUSY errors.
Native void setters remain available.

Supply `lv_aic_lottie_limits_t` with nonzero `source_bytes` and `staging_bytes`.
The first bounds encoded bytes; the second bounds the staging pixel payload
(stride times height). Data is copied and its size must exclude a trailing NUL.
A file source uses an application-registered LVGL drive and requires seek/tell;
it reads to the advertised size, tolerates progressing short reads, rejects EOF
before that size, closes the file and only then attempts replacement. Open,
seek/tell, read and close errors leave the current widget intact. Native ThorVG
file setters still use C filesystem paths.

Load on the LVGL owner thread outside drawing. The candidate gets its own ThorVG
animation/canvas and temporary pixel buffer. Parse, finite positive frame count
and duration, canvas setup and first-frame rendering must all succeed before
pixels, renderer and source timing are replaced. Old timing, paused state,
renderer and caller-owned pixels survive a reported failure. Success publishes
frame zero and resets source timing while preserving pause intent. The caller's
canvas allocation and ownership do not change. A deleted/completed native
animation is rejected without dereferencing its stale pointer; recreate that
widget to resume its lifetime. A faulted GE backend returns BUSY before loading.

The limits are not a complete renderer heap quota: file loading can hold an
encoded staging copy plus ThorVG's own copy, and parsed objects/render state use
additional heap. Upstream ThorVG/LVGL internal allocation assertions and supported
JSON semantics still apply. NO_MEMORY covers checked outer allocations; backend parsing/rendering failures
use DECODE/RENDER. This does not establish complete internal
OOM recovery or an untrusted-document sandbox. External assets/expressions and
whole-animation pre-render validation remain outside this stage. Later native
animation frames follow the ordinary widget runtime.

## Validation

The native renderer contract now repeats checked successful replacements and
failure preservation across 20 widget lifetimes. It verifies malformed/truncated
JSON, embedded NUL, zero-duration metadata, encoded/pixel limits, staged-buffer
allocation failure, six filesystem failure modes, seven-byte progressing reads,
file/data first-frame equality, preserved pause/time/pixels after failure,
continued animation after commit, a valid empty animation and rejection after
explicit deletion of the native animation. File opens/closes remain balanced.
Strict warnings are enabled for the adapter in host builds. Full combined host
suite **77/77 PASS**. The combined D13x 90-degree firmware passes boot/app
compilation, final-link/live-symbol checks for both loaders, image validation
and a clean-source manifest. Exact pins and image hash are recorded in
[validation](validation.md).
Hardware file access, renderer heap pressure and long-duration playback are
**NOT_RUN**. No SDK/LVGL source edits and no flashing.
