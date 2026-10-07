# Bounded SVG post-composition opacity

The pinned LVGL 9.6 renderer honored image opacity but ignored ordinary SVG
`opacity` on groups and shapes. A 50% overlapping red/blue group therefore
rendered at full brightness. The application-owned SHA-guarded renderer now
isolates partial-opacity objects into transparent ARGB layers, composites their
contents first, and applies opacity once when queuing the layer to its parent.
The upstream LVGL pin and SDK sources are unchanged.

## Scope and representation

Root SVG opacity covers the entire document after its viewport/viewBox transform.
Groups, geometric leaves, use references, images and outline text use the same
scope; tspan layout runs before isolating its paint so zero opacity or resource
failure does not remove its text advance. Fill/stroke opacity remains independent.
Opacity defaults to one and is not inherited. Explicit `inherit` uses the direct
parent's computed opacity, rather than the product of ancestor opacities.
Zero-opacity objects are skipped; opaque objects allocate no isolation buffer.

All temporary SVG targets use straight ARGB. The staged vector renderer already
preserves that representation, and native layer blending writes straight ARGB.
Marking a mixed-content destination premultiplied previously turned red255/alpha128
into full-bright red when the decoded SVG was composited again. The new decoded
image, nested-layer and transparent-target oracles reproduce and prevent this.
Parent transforms/scissors and preceding/succeeding draw order are preserved.
The render-object ABI stays unchanged; bits 16..25 store opacity metadata and
remain disjoint from the pinned native flags, guarded by the source SHA.

## Resource and failure limits

`AIC_LVGL_SVG_LAYER_BYTES` defaults to 4 MiB. It counts cumulative eagerly
allocated opacity pixels per SVG render submission, including queued or already
retired siblings, and resets for the next submission. Buffers cover the current
intersected layer/vector clip, not each shape's tight bounds. A full 480x800 clip
costs about 1.46 MiB per partial-opacity scope. Deep full-screen isolation can
therefore exceed the default; it is not claimed equivalent under memory pressure.
The budget excludes parser/render objects, font/glyph caches, vector scratch and
the outer SVG image-effect raster, which keep their existing ownership/limits.

Allocation, buffer or descriptor failure skips the affected opacity object;
subsequent siblings still render. Span layout is retained. Native LVGL owns queued
layers and releases them through ordinary task completion. The 48-scope render
limit also bounds ordinary group recursion; the existing independent 32-use
reference limit remains. Neither bounds parsing/building/freeing an adversarially
deep document. Rendering is serialized on the LVGL owner; no new worker is added.

## Validation scope

The shared host/board set now has 22 analytic fixtures, including 12 opacity
cases. Host checks ten cycles in both direct and decoded-image modes (440 scenes).
An additional host executable runs the actual 44 board probes for ten cycles.
Every render checks native layer-list and layer-memory balance; the document
contract also tracks all LVGL heap allocations through teardown.

Additional checks cover destination alpha 0/64/255 with outer image opacity,
zero/half/full span opacity with following text, image-provider opacity in either
attribute order, 30 injected layer/buffer/descriptor
failures, exactly 256 full-canvas 16 KiB sibling layers plus a declined 257th,
ordinary group depth rejection, subsequent independent rendering and existing
reference-depth/dashed-stroke lifetimes. These are host oracles, not panel or
hardware memory-pressure acceptance. See [validation](validation.md) for the
exact build identities and results when this stage is packaged.

General paint/gradient inheritance, nested SVG viewport semantics, full text
layout, arbitrary renderer-internal OOM recovery and external resource providers
remain separate work. The SDK reference has no omitted GE operation corresponding
to this software SVG correction. Physical pixels/cache/memory/timing: NOT_RUN.

Host regression: combined 92/92 PASS (47.95 s), disabled baseline 81/81 PASS
(22.30 s), minimal SVG without FreeType 8/8 PASS (3.79 s). The image-provider
check was added during final review and is covered by the focused refresh.

The final focused refresh passes 8/8 combined (1.50 s) and 8/8 minimal (1.36 s).
Full D13x boot/app, final-link/static, image and clean-source manifest checks pass.
All 31 artifact hashes, source pins, SDK gitlink, compiled budget and allocated
new probe/limit markers were independently checked. The renderer, opacity scope
and document probe runner are live in the final ELF. Exact identities are in
[validation](validation.md); no board run or flashing was performed.
