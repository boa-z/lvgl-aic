# MPP image decoder

The application-owned component registers one LVGL decoder. It supports FILE
JPEG/PNG through lv_fs and borrowed lv_image_dsc_t RAW/RAW_ALPHA encoded memory.
Both routes share bounded streams, header parsing, PNG chunk CRC validation,
SDK MPP decode and LVGL post-processing. Outputs are RGB888/ARGB8888 usable by
software or supported GE consumers; there is no YUV metadata trick.

- lv_aic_mpp_format.* uses the shared LVGL/MPP format mapping and acceptance policy.
- lv_aic_mpp_stream.* provides FILE and bounded, read-only memory streams.
- lv_aic_mpp_decoder.* owns decoder registration, sessions, CMA accounting and LRU.
- Encoded source limit: 8 MiB; dimensions: 4096 each / 8 MiPixels total.
- Codec support is unchanged: progressive/arithmetic JPEG, gray/interlaced or
  low-depth palette PNG, AICP, BMP, fake images and YUV are not added.

## Ownership and cache

Streams and MPP handles are released before a successful open returns. Decoded
CMA or post-process heap pixels belong to the session. Close releases a reader;
retained cache entries intentionally outlive close. Final release frees the
CMA base pointer or the owning LVGL heap draw buffer, never an interior pointer.

The component LRU defaults to 512 KiB and 16 entries. It keys copied FILE paths
or memory descriptor addresses plus premultiply/stride_align/use_indexed args.
no_cache bypasses lookup/insertion; too-large entries decode uncached. Active
readers cannot be evicted. Invalidation removes visibility immediately and
frees data after the final reader closes. Allocation pressure evicts idle cache
entries before retrying CMA, without replacing the SDK allocator.

Call lv_aic_mpp_cache_drop(source) before changing/freeing a source; NULL drops
all. It also invalidates LVGL header metadata. Generic lv_image_cache_drop()
alone does not invalidate this component cache. Cache APIs and decoding run
in the serialized LVGL owner. Close all readers before lv_aic_deinit(); pending
readers refuse platform teardown so the caller can close and retry safely.

See [resource-stage API, limits and board checklist](../../docs/resource-stage.md)
for the memory descriptor example, budget accounting and statistics semantics.

## Evidence and board fixtures

The host memory/cache contract exercises the production decoder with mocked
MPP allocation: bounds, CRC failures, option keys, LRU, pressure, active readers,
invalidation, disabled/oversized caches and clean teardown/reinit. It does not
prove decoder pixel correctness, CMA hardware coherency or IRQ timing.

Component tools stage the maintained fixtures under /data/mpp_test, including
a.jpg, padded aic_801x479.jpg, b.png (RGB) and c.png (RGBA). Automatic board
resource checks compare FILE and memory visible-pixel hashes, check repeated
cache hits without new CMA allocations, then require zero retained entries
and balanced wrapper CMA. The existing 1000-cycle uncached test follows.
The manual pages continue to show file JPEG/RGB/RGBA together with transforms.
Current-stage board checks are NOT_RUN until measured on the exact candidate.
