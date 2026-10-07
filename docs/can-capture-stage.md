# CAN screenshot stage

Framebuffer streaming over classic CAN (500 kbit/s product convention,
CAN ID `0x1CA`), host-decoded with
[`python-can`](https://python-can.readthedocs.io/). RLE runs and CRC32
match the UART capture, so PNG output shares `capture_to_png`.

One command, no serial console (needs the CAN OTA endpoint running; that
endpoint belongs to the lvgl-aic-smoke application, `ota/`,
`AIC_LVGL_SMOKE_CAN_OTA`, which autostarts it):

    python tools/sdk/can_capture.py --interface pcan --channel PCAN_USBBUS1 --trigger out.png

`--trigger` sends a standard frame `0x1CB` with payload `CAP`; the OTA
RX thread (the only `can0` reader) forwards it to
`lv_aic_can_capture_request()`. Without the OTA endpoint, start the
listener first and then run `lv_aic_can_capture start|stop|status
[can0|can1]` on the shell.

## Protocol (see `tests/manual/lv_aic_can_capture.h`)

Little-endian 8-byte frames, one RLE run per DATA frame:

- BEGIN `flags=0x01`: `seq=0 | w | h | fmt`
  (`fmt`: 0=RGB565 1=RGB888; ARGB/XRGB stay UART-only).
- DATA `flags=0x00`: `seq | count | p0 p1 p2`; `seq` runs 1, 2, ...
  modulo 65536 (a busy 800x480 frame has more than 65535 runs).
- END `flags=0x02`: `seq=data frame count mod 65536 | crc32`, the
  finalized (zlib) CRC32 of the raw pixels.

The receiver drops other IDs, checks sequence continuity and the CRC32.

## Wiring and adapters

- Board CANH/CANL to the adapter; 120 ohm termination on the bus;
  adapter and board at the same bitrate (default both 500 kbit/s).
- The adapter must stay on the bus: classic CAN needs a second node
  for ACK, otherwise every frame fails (`begin not acked`).
- `python-can` interface names: `slcan` (CANable/cantact serial),
  `socketcan` (Linux `can0`), `pcan` (PCAN-USB), `canalystii`
  (CANalyst-II). Pass through `--interface/--channel`.

## Device behavior

- Snapshot runs on the LVGL owner thread (`..._poll`, reuses
  `lv_aic_display_snapshot`); a priority-25 worker streams frames with
  a 1 ms yield every 64 frames so the UI stays alive.
- Every frame goes through one sender: `hdr=-1` and a bounded retry
  (single-frame TX mailbox); `status` reports the last retry count.
- `can0` is configured (baud, interrupts) only when idle. When the OTA
  endpoint already owns it, capture just takes a reference and releases
  it afterwards, so OTA keeps working across captures.
- `stop`/lifecycle teardown aborts mid-stream; the snapshot is always
  released and the CAN device closed.
- Throughput estimate (800x480 RGB888, 500 kbit/s, ~4500 frames/s):
  raw ~30 s, RLE typically 3-10x less depending on content.

## Evidence

- Host: `can_capture.py --selftest` PASS (RGB565/RGB888 round-trip).
- Target (`demo-meter-cancap`, `sdk@e4cb5194` / dirty `lvgl-aic` /
  `lvgl@80ca777e`): boot/app/static (`lv_aic_can_capture_poll/deinit`
  live symbols)/image/manifest 全 PASS after `scons -c` (the old SCons
  ignored SConscript-only changes and linked a stale ELF twice).
  Image SHA256
  `d9e9e5c9984b77188aab43d765989488f16796bd96ca21c3b11e2b236516f924`,
  meter assets 169 in package.
- CAN bring-up fixes (`can-500k` image): `RT_DEVICE_CTRL_SET_INT`
  after SET_BAUD (RX stayed silent without it — the board `rx=0`
  root cause), `msg.hdr = -1` under HDR mode on RX/TX, bus rate
  aligned to the product 500 kbit/s convention; `maintenance` argc
  fix. Image SHA256
  `7f99263fe792340c4a74a933bf21158ed68caf053f3aad4e1fe01d673131aa0c`,
  all gates PASS.
- Board PASS (2026-10-07, D50T-2-Lite, PCAN-USB 500 kbit/s): the
  smoke test page, 800x480 RGB888, 65941 runs, 17.3 s (~3.8k
  frames/s, bus-bound), board and host CRC32 equal (`0b55092a`),
  `retries=0`; second capture PASS (only the animated spinner
  differs); OTA `info` answers before and after. Evidence: SDK
  `output/lvgl-evidence/board-2026-10-07-can-capture/`.
- Fixed by that run: the 16-bit `seq` wrap (host rejected `got 0 want
  65536`), END sent the running instead of the finalized CRC, and
  BEGIN/END bypassed `hdr=-1`.
