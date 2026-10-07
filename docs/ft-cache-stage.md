# FreeType cache management stage

## API and ownership

`AIC_LVGL_USE_FT_CACHE`, dependent on native FreeType, replaces the old reserved
placeholder with SDK-compatible `aic_lv_ft_cache_print_stats`,
`aic_lv_ft_cache_drop_all` and `aic_lv_ft_cache_drop_specific`. The additional
`aic_lv_ft_cache_get_stats` returns structured font/cache entry, capacity and
reference counts. NULL pathname and the SDK ANY style/render-mode constants
are wildcards; supplied filters combine with AND. GLYPH=1, DRAW_DATA=2, ALL=3;
zero and unknown mask bits are invalid. Empty selections succeed.

These are counts, not allocated bytes. Font objects and their face references
survive eviction, with native caches rebuilding on demand. The existing
per-cache glyph count limit remains; a global font byte budget is still absent.
The port keeps L1 disabled to support non-power-of-two cache capacities. The
conditional L1 replacement path has not been validated in an L1-enabled build.

Run calls between refreshes with draw workers idle, on the LVGL owner thread or
under its external lock, serializing font create/delete and glyph acquisition /
release as well. This is not a concurrent purge API. A query failure leaves its
output unchanged. All matched selected caches are preflighted before any
purge; referenced entries or planning allocation failure return INVALID without
eviction. A held draw bitmap does not prevent a glyph-metrics-only purge or a
purge selecting an unrelated font. Invalid/uninitialized context is rejected.

The pinned upstream `lv_cache_drop_all` cannot safely purge referenced nodes.
The adapter therefore checks every entry using native cache/iterator APIs and
pins actual font-cache nodes only after destroying the iterator (acquire moves
LRU links). It uses LVGL 9.6 private FreeType and iterator definitions rather
than SDK copies of LRU/tree layouts. Upgrading LVGL requires rechecking these
private contracts. No SDK or upstream LVGL source changes are required.

## Validation and board follow-up

The real-LVGL/FreeType host contract exercises three init/deinit cycles, Latin
and Chinese glyphs, shared faces at two sizes, style/path/render-mode filters,
selective glyph/draw purge, held-bitmap atomic rejection and unrelated purge,
identical bitmap reconstruction, invalid masks/context and every component
snapshot/plan/iterator allocation failure. Native font UI tests also execute
the manual cache probes before rendering and closing the four-row font panel.
Host FreeType and SDK FreeType differ, so hashes are compared before/after in
the same runtime, not across implementations.

The SDK `-WithFonts` profile enables cache controls and requires all four API
symbols in the live final link map. On the board expect three lines beginning
`PASS font cache size=` (18, 28, 42), followed by the existing font metrics /
bitmap/fallback/cache-churn results. Open/close Fonts repeatedly, inspect Latin
and Chinese rows, and retain complete serial output. Hardware execution,
heap/performance measurements and panel acceptance remain **NOT_RUN**.

## Completed host and firmware checks

Combined host **85/85 PASS** (39.19 s), disabled baseline **78/78 PASS**
(15.25 s). The cache contract rejects 45 injected component allocation failures
across three lifecycles without eviction. All three size/style manual cache
probes execute in the host font UI test. The combined host profile now explicitly
enables FreeType: earlier 83-test combined checkpoints did not include the two
native font contracts, although their target firmware enabled native fonts.

Full D13x boot/app compilation, final-link/static and image checks **PASS**.
All 29 manifest file hashes and all three clean source pins were independently
verified. Exact clean build identities and image hashes are recorded in
[validation](validation.md). This does not establish hardware acceptance.
