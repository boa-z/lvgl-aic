# Component BMP decoder

The MPP resource adapter now also decodes uncompressed 24/32-bit BMP in
software, using its existing CMA allocation and LRU ownership. No MPP codec
is invoked for BMP; the resulting complete RGB/ARGB draw buffer can follow
the existing GE image path when its address and geometry are supported.

支持大小写无关的 BMP 文件路径和 RAW/RAW_ALPHA 内存源、正负高度、
四字节文件行填充、非默认像素偏移。输出行按八字节对齐。
32 位输入沿用 SDK 的 BGRA/ARGB8888 字节解释，不自动猜测全零 alpha
是否代表不透明。资源调用方仍须遵守现有缓存失效约定。

Headers validate signature, DIB extent, planes, compression, dimensions,
declared length, pixel offset and complete row bounds. Unsupported indexed,
RLE and bitfield BMP are rejected. 16-bit is not yet implemented: BI_RGB
means RGB555, so it cannot safely be treated as RGB565 without conversion.

Host 16/16 PASS includes independent expected-pixel checks for FILE/RAW,
24/32-bit, both row directions, padding and offset; it also checks cache
sharing, active-reader invalidation and allocation failure cleanup.

Remaining: RGB555/RGB565 mask handling, target build and board rendered
pixels/cache acceptance. This is partial SDK parity, not complete BMP support.
