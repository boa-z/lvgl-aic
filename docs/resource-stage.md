# Resource stage: memory images and bounded decoded cache

Stage date: 2026-09-30. Branch: codex/sdk-basic-capabilities.
This is the complete development scope prepared for the next board session;
board acceptance is a separate gate. It includes the earlier solid-fill candidate.
Development gates are PASS at a5b5bd4 (8 host tests and all three firmware
profiles). Board feedback now confirms the numeric fill/blend/scale and CMA
stress checks plus operator visual acceptance. Overall board status is PARTIAL:
earlier resource logs, explicit touch/timed-running evidence and logging fixes
remain open. Exact evidence, image hashes and the immutable handoff archive
are in the [validation record](validation.md).

## Delivered scope

- FILE JPEG/PNG and borrowed lv_image_dsc_t RAW/RAW_ALPHA JPEG/PNG inputs use
  the same parser, bounded stream, packet CRC gate, MPP allocator and post-process.
- The encoded payload determines dimensions and output format. RAW_ALPHA does
  not force alpha on JPEG; PNG content determines RGB888 versus ARGB8888.
- Maximum encoded size is 8 MiB, each dimension 4096, total pixels 8 MiPixels.
  SDK limits remain: progressive/arithmetic JPEG, gray/Adam7/low-depth palette
  PNG, AICP, BMP, fake images and YUV are not added by this stage.
- A component-owned LRU retains decoded CMA or post-processed heap buffers.
  Default budget is 512 KiB (AIC_LVGL_MPP_CACHE_BYTES); at most 16 entries.
  File paths are copied; memory keys use the descriptor address. Decode options
  premultiply, stride_align and use_indexed are part of the key.
- no_cache bypasses both lookup and insertion. flush_cache still follows the
  caller's current decoder args through LVGL's normal post-open path.
- Budget counts retained decoded allocations, session metadata and copied path.
  It is not a total memory cap: encoded input, MPP internal allocations and live
  uncached decodes are additional. Oversize entries decode without caching.
- Active readers are never evicted. Invalidation hides an entry immediately;
  storage is freed on its final close. Shrinking below pinned bytes temporarily
  exceeds the budget until readers close. CMA allocation failure evicts idle
  entries and retries, without replacing any SDK allocator hook.

## Application API

Use these functions only in the serialized LVGL owner after lv_init().
The public declarations are in include/lvgl_aic.h when MPP is enabled.

    lv_aic_mpp_cache_set_limit(512U * 1024U);  /* zero disables and drops entries */
    const lv_aic_mpp_cache_stats_t *stats = lv_aic_mpp_cache_stats();
    lv_aic_mpp_cache_drop(source);           /* NULL drops all */

Cache statistics report hits, misses, evictions, entries, bytes and limit_bytes.
Hits/misses exclude no_cache and disabled-cache opens; misses can include failed
attempts. Existing decoder_last_stats describes the last actual decode, not a
cache hit. CMA stats count all wrapper-owned live CMA, including retained cache.
Resetting CMA counters preserves current bytes and starts peak at that baseline.
A reset with live allocations means later frees can exceed interval allocations.

To render encoded memory with normal LVGL widgets:

    static lv_image_dsc_t resource;
    resource.header.magic = LV_IMAGE_HEADER_MAGIC;
    resource.header.cf = LV_COLOR_FORMAT_RAW; /* RAW_ALPHA is also accepted */
    resource.data = encoded_png_or_jpeg;
    resource.data_size = encoded_byte_count;
    lv_image_set_src(widget, &resource);

The descriptor and encoded bytes remain application-owned. Keep them alive
while any widget can request the image. Before changing a file in place,
mutating encoded bytes, reusing a descriptor address or freeing a source, call
lv_aic_mpp_cache_drop(source) and stop/update widgets that refer to it.
For a file, pass the same LVGL path string value used to open it.
The wrapper also invalidates LVGL header metadata. Generic
lv_image_cache_drop() alone does NOT invalidate the component cache.
This explicit API avoids core patches and global cache/allocator replacements.

All successful lv_image_decoder_open calls must have matching close calls.
Close descriptors before lv_aic_deinit. An outstanding reader causes platform
teardown to be refused without losing its owned objects; close and retry.
Clean teardown drops retained cache and stale header-decoder pointers.

## Development completion checks

1. Eight host CTests pass, including the production-source memory/cache contract.
2. MPP, GE2D and GE-disabled Gate 1 profiles build, link and package successfully.
3. MPP/GE2D link checks require the resource-test and cache-control live symbols.
4. Source commits, config, logs, ELF/map and image hashes are recorded in manifests.
5. The original Hangcha worktree remains unchanged; SDK/core source is not edited.

Host coverage includes multiple active readers, invalidation with live references,
file-key ownership, all decode-option keys, byte and entry limits, LRU behavior,
zero budget/no_cache, oversized entries, eviction under allocation pressure,
decode/allocation failures, stream bounds, bad memory PNG CRC, and clean reinit.
The hardware calls are mocked; host results cannot certify pixels or IRQ timing.

## Next board session

Use the GE2D candidate as the integrated stage image; the MPP image is a software
rendering control and Gate 1 is the display/touch baseline. Refer to the latest
validation record for exact commit, image hash and isolated output location.
Do not use an older image merely because the filename matches.

The existing smoke owner runs these finite checks automatically before its UI:

- Existing OS lifecycle and FILE fixture acceptance checks.
- BEGIN resource stage: memory JPEG/PNG and bounded cache.
- Three PASS file-memory parity lines: padded 801x479 JPEG, RGB PNG, RGBA PNG.
  Hashes compare visible pixel rows, excluding padding. File and memory decode
  sequentially, avoiding two full JPEG CMA allocations at once.
- Two PASS cache 100 hits lines: no new CMA allocation and stable pixel hashes.
- PASS resource stage; file/memory parity and cache lifetime balanced.
- Existing 1000 uncached decode/close cycles and balanced CMA checks.
- On GE2D: twelve solid-fill numeric probes plus existing image/transform checks.

The resource probes temporarily use a 4 MiB cache budget and restore the prior
budget after dropping their entries. They check invalidation while one reader
remains open, then require zero retained entries and zero wrapper CMA bytes.
A failed fixture, resource probe, pixel comparison or allocation is a failed
stage, never a silent skip. Record the full serial log and the exact image hash.
After automated checks, inspect all three pages, alpha edges and transforms,
then confirm touch down/move/release and continued refresh. File-memory equality
checks two decoding routes; it does not replace panel correctness acceptance.
No board flashing is performed during development completion.
