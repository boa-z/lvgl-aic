# SVG image transforms and clipping

With SVG enabled, the application build now stages corrected copies of the
pinned native custom-image dispatcher and SVG decoder. Upstream files remain
unchanged. Both normalized source hashes must match the reviewed LVGL revision;
a dependency change fails generation and requires review.

The dispatcher preserves original image coordinates and computes transformed
bounds solely for visibility. The effective clip intersects the caller clip,
destination buffer and transformed bounds. It neither dereferences an optional
object nor overwrites the caller clip. Child layers retain absolute coordinates;
the software vector backend applies the destination-buffer translation once.
The SVG decoder applies position, pivot, rotation and per-axis scale directly,
removing its old second bounding-box offset.

The regression initially failed on native code (436 incorrect interior pixels
in its first canvas scene). After correction, 48 cases pass an independent
inverse-transform pixel oracle: eight geometries across canvas with/without an
object, offset child layers with/without an object, real clipped image widgets
and actual parent opacity layers. Geometries include nonuniform scale, right
angles, positive/negative arbitrary rotations, noncentral/outside pivots and a
source origin outside the clip whose transformed content is visible. Every
pixel outside the clip must stay black; interior/exterior samples tolerate at
most two channel levels and exclude a two-source-pixel antialias boundary.
Caller clip preservation is checked before dispatch.

Full SVG/vector/Lottie host suite **78/78 PASS**. Combined D13x target firmware passes boot/app compilation, final-link/live-symbol
ownership, image and clean-source manifest gates. Both generated corrections are
retained in the manifest; see [exact pins and image hash](validation.md). The existing FILE/VARIABLE contract also remains green.

This corrects native 9.6 SVG integration, an extension beyond the SDK's legacy
configuration. It is not GE vector acceleration. Parent layered opacity is
covered; native image-specific opacity/recolor/tiling and rounded-image clips
need separate handling/coverage. Rich SVG document semantics, font/embedded
assets, allocation budgets and animation remain open. Hardware **NOT_RUN**.
