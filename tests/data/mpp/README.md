# MPP decoder test assets (non-product)

Mainstream suites only; nothing here is D50T product artwork.

## Sources and licenses

- PNG files `bas*.png`, `basi*.png`, `s*.png`, `x*.png`, `z*.png`:
  PngSuite by Willem van Schaik (http://www.schaik.com/pngsuite/),
  mirrored at https://github.com/lunapaint/pngsuite (`png/` =
  PngSuite-2017jul19). PngSuite is freeware per `PngSuite.LICENSE`.
- JPEG/PNG `testorig.*`, `testimg*.jpg`: libjpeg-turbo
  (`testimages/` and `ijg/` reference repos, BSD-style licenses).
  `testimgp.jpg` is progressive, `testimgari.jpg` is arithmetic-coded.
- `aic_*.jpg`: generated with Pillow (gradient ramps, quality 85) for
  product-size (800x480) and odd-stride (801x479) coverage.
- `empty.*`: zero-byte files. `bad_crc_rgb.png`: `basn2c08.png` with one
  IDAT CRC byte flipped (info parses, MPP decode must fail).

## Expected decoder decisions (checked by `check_headers.py`)

| File | Size | Decision |
|---|---|---|
| `testorig.jpg`, `testimg.jpg`, `testimgint.jpg` | 227x149 SOF0 4:2:0 | info+open OK → RGB888 |
| `aic_800x480.jpg` | 800x480 SOF0 | info+open OK → RGB888 |
| `aic_801x479.jpg` | 801x479 SOF0 | info+open OK → RGB888 (16B MPP stride) |
| `aic_160x120.jpg` | 160x120 SOF0 | info+open OK → RGB888 |
| `testimgp.jpg` (progressive SOF2) | 227x149 | info INVALID |
| `testimgari.jpg` (arithmetic SOF9) | 227x149 | info INVALID |
| `empty.jpg` | 0 B | info INVALID |
| `basn2c08.png`, `z09n2c08.png` | 32x32 RGB8 | info+open OK → RGB888 |
| `basn6a08.png` | 32x32 RGBA8 | info+open OK → ARGB8888 |
| `basn3p08.png`, `s33n3p04.png`, `s37n3p04.png` | palette | info+open OK → ARGB8888 |
| `basi2c08.png` (Adam7 interlaced RGB) | 32x32 | info OK, MPP open INVALID |
| `bad_crc_rgb.png` | 32x32 RGB8, bad IDAT CRC | info OK, MPP open INVALID |
| `basn0g08.png`, `testorig.png`, `xcsn0g01.png`, `xhdn0g08.png`, `empty.png` | gray/corrupt/empty | info INVALID |

Run: `python3 tests/data/mpp/check_headers.py` (22/22 must match).

## Board mapping

Copy into the image `data/` tree before packing `data.fatfs`:

```text
data/mpp_test/a.jpg <- tests/data/mpp/aic_800x480.jpg
data/mpp_test/b.png <- tests/data/mpp/basn2c08.png
data/mpp_test/c.png <- tests/data/mpp/basn6a08.png
```

The manual page shows A/B/C at once for color, alpha, stride, and
repeat-refresh checks. Negative files (`testimgp.jpg`, `bad_crc_rgb.png`,
`empty.*`) can be pushed to the same directory to prove safe `INVALID`
without reflashing (LVGL shows placeholders, no hang).

## Gate coverage

- small JPEG: `testorig.jpg` / `aic_160x120.jpg`
- 800x480 JPEG: `aic_800x480.jpg`
- non-tight stride: `aic_801x479.jpg` (2403 B/row → 16B MPP stride),
  `s33n3p04`/`s37n3p04` (odd widths)
- illegal JPEG: `testimgp.jpg`, `testimgari.jpg`, `empty.jpg`
- missing file: any absent `L:/data/mpp_test/*.png` path
- RGB/RGBA PNG + alpha/channel-order: table above
- corrupt PNG: `bad_crc_rgb.png`, `xcsn0g01.png`, `xhdn0g08.png`
