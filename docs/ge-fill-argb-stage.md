# Straight ARGB solid fill and SDK pseudo-fill composition

Solid unrounded FILL now accepts partial opacity on straight ARGB8888 targets.
The SDK original submits these fills directly with hardware alpha enabled.
This port instead runs raw opaque GE FILL into a bounded visible intermediate,
then native LVGL CPU composition preserves target color and alpha. This is
hybrid execution; it claims neither direct hardware alpha equivalence nor a
speedup over native solid fill. Physical memory cost and timing remain to measure.

The existing `AIC_LVGL_GE2D_ALPHA_BYTES` budget (2 MiB default) also bounds this
surface. Width/height are at most 4096, stride is aligned to 64 bytes, and only
task/clip/layer intersection is allocated. Original destination geometry,
address, format and flags are checked before allocation/cache/DMA. No extra
concurrent scratch budget is introduced: the synchronous unit completes each
task before the next. Rounded/gradient fills and premultiplied destinations
remain outside this path. Opaque and explicit replacement fills keep their
existing direct GE path, including alpha-zero `.fake` replacement.

SDK `blend=1` `.fake` pseudo-images use this same staged fill. Their encoded
alpha retains SDK semantics; ordinary image opacity does not override it.
Both scheduler FILL and pseudo-fill fall back before DMA if staging cannot be
allocated, addressed or fitted in budget. Software scheduler fallback clips
the copied task to the layer before calling native fill. Neither the original
task nor descriptor is mutated. Completion/fallback counters distinguish actual
GE submission from software fallback.

GE stores opaque RGB with alpha blending disabled. The CPU tail reads those
pixels as **XRGB8888**, applying global opacity exactly once. Treating them as
ARGB images would introduce an extra alpha-times-opacity quantization and can
incorrectly suppress opacity `LV_OPA_MIN + 1`. The private shared helper now
takes an explicit source format; RGB/YUV retain their prior source semantics.

The original destination remains unchanged through submit/emit/sync. After
success the stage is invalidated, native composition runs, and the target is
cleaned for immediate DMA/readback. Failure at any GE step retains scratch and
the in-flight task/target, latches the existing shared GE fault and blocks replay
until reboot. Pre-DMA failure frees any rejected scratch without touching target
pixels or caches. No SDK/core source edits or board flashing are required.

## Validation

- **13,824 scenes**: all 256 global opacities, destination alpha 0/1/64/128/254/255,
  three solid colors, offset layer, two overlapping clips and a fully clipped
  case. All **70,778,880 channel/guard byte comparisons** exactly match the
  independent native solid-fill path; observed maximum error **0**.
- Full/split refresh bytes match exactly. Stride/clip guards, task/descriptor
  immutability, allocation/address rejection, premultiplied/format/stride
  rejection, pre-allocation budget rejection and software completion pass.
- Scheduler submit/emit/sync failures retain task/surface, leave target unchanged
  and never count completion or software replay. Real `.fake` image dispatch
  has another 24 exact native-oracle scenes, allocation fallback and all three
  DMA failure positions.
- Combined host **86/86 PASS** (21.80 s), disabled baseline **79/79 PASS** (20.54 s).
  Existing RGB/YUV alpha and transform suites pass with the explicit-format helper.
- **84 additional board probes** cover normal/pseudo-fill, six destination
  alphas and seven opacity values including both native thresholds. They compare
  all target and padding bytes with native solid fill, require ENGINE and
  immediately invalidate the target to check final CPU cache clean.

Full D13x boot/app, final-link/static, image and clean-source manifest gates
**PASS**. All 29 file hashes and three clean source pins were independently
verified; new probe strings are present in allocated ELF `.rodata`.
Exact build identities and hashes are recorded in [validation](validation.md).
Host GE is a descriptor-driven CPU model, not hardware FILL/cache proof.
Physical probes, CMA pressure, timing and panel acceptance remain **NOT_RUN**,
deferred to the unified board session.
