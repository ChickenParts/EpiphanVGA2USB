# Original VGA2USB 5555:1110 frame protocol: static evidence

Investigated 2026-09-28 without USB traffic, target-machine writes, kernel builds, or module loading. Scope is the original device, not VGA2USB LR/HR/PRO. Follow-up `FRAME-SETUP.md` resolves allocation defaults and shows that B0 is skipped on this device. This is a bounded map from the symbolized vendor object; unresolved bytes are preserved as unresolved instead of supplying a speculative command.

Input: `vendor/vga2usb_bins.o_shipped`, ELF64 x86-64 relocatable, SHA256 `536b9be25b310d57a77d2888fe83cd2e499417dbb963129edc90357cf9736dbb`.

Evidence files alongside this report: `disassembly.txt` (complete `objdump -drwC -Mintel`), `relocations.txt`, and `rodata.bin`. All addresses below are section-relative `.text` or `.rodata` addresses, not loaded virtual addresses. Objdump labels such as `grab_protocol_sdi+...` merely identify the nearest preceding *named* symbol; anonymous functions in that address range belong to the HOME table described below.

## Device dispatch and capture endpoint

- `usb_board_vga2usb` is `.rodata+0x11b240`.
- Its `+0x30` relocation points to `usb_io_create_vga2usb` (`.text+0x4d10`), whose table is `.rodata+0x1a2840`.
- USB table `+0x50` points to anonymous `.text+0x40c0`, the capture/read-lines implementation.
- USB table `+0x108` points to anonymous `.text+0x37c0`, the original B4 sender. This differs from adjacent device tables.
- Board `+0x40` points to `.rodata+0x11d1c0`; board `+0x48` is one capture channel. Channel entry `+0x08` is `0x82`.
- `usb_board_init` at `0x19ce..0x1a2f` selects a channel entry and passes its byte `+0x08` to `os_usb_pipe_create`.
- `os_usb_pipe_create` at `0x42b33` compares that exact byte against USB endpoint-descriptor `bEndpointAddress`; at `0x42b44` it recognizes transfer type 2, and `0x42c70..0x42c9e` constructs the bulk-IN pipe.

**Capture endpoint: 0x82, bulk IN.** Endpoint 2 OUT is not the capture endpoint.

## Original B4 command

Anonymous sender `.text+0x37c0..0x38c1` allocates exactly 31 bytes. The caller at `.text+0x45a1..0x45c1` passes `rdx = gp`, `rcx = gp+14`. Although it also passes the requested output format in `esi` and another parameter in `r8`, the original sender does not consume these registers.

USB setup fields, established by `0x37f8..0x389a` and the call to `os_bususb_control_msg` at `0x389a`:

| Setup field | Value |
|---|---:|
| bmRequestType | 0x40 (vendor, device recipient, OUT) |
| bRequest | 0xB4 |
| wValue | 0 |
| wIndex | 0 |
| wLength | 31 |
| Data | packed parameter prefix described below |

The vendor driver supplies 500 ms to its USB control helper. This is observed vendor behavior, not a process timeout used during this investigation.

The sender copies gp bytes 0..13, then gp bytes 14..30. It byte-swaps precisely six 16-bit fields: offsets 0, 14, 16, 18, 20, and 22. The remaining bytes keep their host layout. Thus these six wire fields are **big endian**, while returned pixel words are **little endian**.

| Wire offset | Size | Established meaning / source |
|---:|---:|---|
| 0 | BE16 | Horizontal total: active width + front porch + sync width + back porch. `VESA_compute_VGA_parameters` 0x25a1b..0x25a39. |
| 2 | byte | ADC/measured signal flags. AD9884A detector at 0x112ab..0x112bd copies measured status low two bits here. Generic decoder also understands bit 0x10 as a factor of two in segmentation; its use for this original board remains unresolved. |
| 3 | byte | ADC clock/range control derived from estimated pixel clock and horizontal phase count; 0x1132e..0x11394. Not a generic pixel-format selector. |
| 4 | byte | Analog timing/clamp control initialized to 8 for these low pixel-clock modes, 0x25b63..0x25bac. Hardware register semantics not fully traced. |
| 5 | byte | Analog timing/clamp control initialized by bounded back-porch calculation, 0x25b99..0x25bc2. For listed modes this calculation gives 20. Hardware semantics not fully traced. |
| 6 | byte | ADC-table control: original AD9884A ops +0x6c is 31 (0x1f), used by 0x25a39..0x25a4f. Generic fallback 16 applies only when this ADC-table byte is zero. |
| 7..9 | 3 bytes | Three analog-channel controls, original defaults all 112 (0x70), midpoint of AD9884A ops +0x5c/+0x5e = 32/192, 0x25a52..0x25a8a. Generic fallback 128 is not used on this ADC. |
| 10..12 | 3 bytes | Three analog-channel controls, original defaults all 32 (0x20), midpoint of AD9884A ops +0x50/+0x52 = 16/48, 0x25a95..0x25ab5. Generic fallback zero is not used on this ADC. |
| 13 | byte | Analog-channel control flags, low two bits initialized to 3, 0x25a8d..0x25abc. |
| 14 | BE16 | Active width W, 0x25ac7. |
| 16 | BE16 | Active height H, 0x25ab8..0x25ac0. |
| 18 | BE16 | Vertical phase count V. Initial VESA value is 1 for progressive modes; HOME initializer changes this to 2 normally or 4 when mode flags 0x20 is set, 0x220e4..0x22117. |
| 20 | BE16 | Lines per vertical phase L = H / V, 0x2210c..0x22119. |
| 22 | BE16 | Horizontal capture start. Normally sync width + back porch. AD9884A table +0x6a override is zero and ADC instance +0x28 adjustment is explicitly zero; original default therefore uses this normal value. 0x25ae9..0x25b13; see FRAME-SETUP.md. |
| 24 | byte | Vertical capture start. Back porch plus sync width when mode flag 0x80 is set, 0x25ad8..0x25ae6. |
| 25 | byte | Vertical phase selector/count byte, kept synchronized with offset 18 by HOME initializer and retry path 0x221e0..0x22229. |
| 26 | byte | Horizontal sample phase count P, determined by `AD9884A_hinterlaces`, 0x113d0..0x11474. |
| 27 | byte | Horizontal sync threshold. `vgrabber_set_hsync_threshold` 0x28d20 writes vgrabber+0x10b = gp+27. Default 128 from `vgrabber_init` 0x283a1..0x283a6. |
| 28 | byte | Vertical sync threshold. `vgrabber_set_vsync_threshold` 0x28d80 writes vgrabber+0x10c = gp+28. Default 128 from the same 0x8080 initializer. |
| 29 | byte | Noise-filter level, range 0..5. `vgrabber_set_noise_filter` 0x28cd0..0x28cfd validates and writes vgrabber+0x10d = gp+29. Initial value zero: usb_board_init 0x19e7..0x1a1f clears the entire 0x338-byte vgrabber before vgrabber_init. Retained by VESA_clear_gp. |
| 30 | byte | Capture flags. Bit 0x02 follows gp+0x33 bit 0x04; bit 0x04 follows gp+0x83 mask 0x42000; bit 0x08 is adjusted for interlaced modes, 0x25b0a..0x25bd1. Default progressive, uncropped RGB path sets none of these three bits. |

`VESA_clear_gp` 0x25640 preserves bytes 27..29 and the following 32-bit property at offset 31; it clears the rest of the parameter prefix. Byte 31 itself is **not sent** in B4.

## Mode records and minimum examples

`VESA_get_vesamodes_table` returns `.rodata+0x1a3fc0`, count 137. Record size is 24 bytes, and the fields consumed by VESA/AD9884A callers establish this layout:

`u32 refresh_mHz; u16 width, h_front, h_sync, h_back, height, v_front, v_sync, v_back; u32 flags`.

Exact records extracted from this object:

| Mode | Index / address | Refresh mHz | H fields active/front/sync/back | V fields active/front/sync/back | Flags |
|---|---|---:|---|---|---:|
| 640x480 standard VGA | 26 / 0x1a4230 | 59940 | 640 / 16 / 96 / 48 | 480 / 10 / 2 / 33 | 0x03 |
| 720x400 standard VGA | 20 / 0x1a41a0 | 70000 | 720 / 22 / 108 / 50 | 400 / 14 / 25 / 11 | 0x83 |

There are other timing variants with the same dimensions; dimensions alone do not select the record.

`AD9884A_hinterlaces` computes a pixel clock estimate from horizontal/vertical totals and refresh. It chooses at least ceil(W/1024) phases and at least ceil(clock/50000000), returning 1 if the result is <=1 and otherwise rounding up to an even number. Both standard records above return P=1 (25,174,800 and 28,350,000 Hz estimates). HOME initial V=2 gives L=240 or 200.

Derived normal, progressive parameter fields:

| Field | 640x480@59.94 | 720x400@70 |
|---|---:|---:|
| Horizontal total | 800 / wire 03 20 | 900 / wire 03 84 |
| Width | 640 / 02 80 | 720 / 02 d0 |
| Height | 480 / 01 e0 | 400 / 01 90 |
| V | 2 / 00 02 | 2 / 00 02 |
| L | 240 / 00 f0 | 200 / 00 c8 |
| Horizontal start, with no board-specific extra adjustment | 144 / 00 90 | 158 / 00 9e |
| Vertical start | 33 / 21 | 36 / 24 |
| Byte 25 | 02 | 02 |
| P / byte 26 | 01 | 01 |

Default first-capture payload templates for the exact standard timing records and fresh default controls (only SS requires the valid source measurement):

```
640x480: 03 20 SS 00 08 14 1f 70 70 70 20 20 20 03 02 80 01 e0 00 02 00 f0 00 90 21 02 01 80 80 00 00
720x400: 03 84 SS 00 08 14 1f 70 70 70 20 20 20 03 02 d0 01 90 00 02 00 c8 00 9e 24 02 01 80 80 00 00
```

`SS = B1[8] & 3` after confirming a valid measured signal matches the selected timing record. Other bytes are now established original-device defaults. A no-source B1 (first BE16 field zero) does not identify either timing record. Modes can be altered by forced mode, input selection, analog controls, or HOME retries; see FRAME-SETUP.md for setup ordering and evidence.

## Frame format, segmentation, and deordering

HOME constructor `grab_protocol_home` at 0x23370 chooses `.rodata+0x1a3e60`. Its `+0x40` function is `.text+0x23100`, which calls `.text+0x22290` to retrieve/unpack a frame (possibly through a temporary frame for crop). The latter calls `grab_io_read_image_lines` with output-format constant 0x400 at 0x222d8. That reaches original capture `.text+0x40c0` and the B4 sender.

The original sender does not encode an output pixel-format selector; software conversion selects the requested output format after receiving RGB565. The format constant 0x400 in this path corresponds to the software copy converter `.text+0x22130` selected at 0x228ee. This converter copies native 16-bit words directly (with stride when P>1).

**Wire pixel format: little-endian RGB565**, R in bits 15..11, G in 10..5, B in 4..0. Evidence: conversion `.text+0x20770` loads a LE16 word and expands low 5 bits into RGB24 blue, middle 6 into green, and high 5 into red; `.text+0x20720` swaps the 5-bit red/blue fields for the alternate 16-bit output. Expansion tables `.rodata+0x1a3f00` and `+0x1a3ec0` are the 5- and 6-bit full-range lookup tables.

For ordinary original-device modes, gp byte 2 bit 0x10 is clear. Let:

```
W = be16(command[14..15])
H = be16(command[16..17])
V = be16(command[18..19])
L = be16(command[20..21])
P = command[26]
T = V * L                         # H for these modes
pixels_per_segment = W / P
useful_bytes = 2 * pixels_per_segment
segment_stride = ceil(useful_bytes / 512) * 512
segments = T * P
expected_bytes = (segments - 1) * segment_stride + useful_bytes
```

The size expression is implemented by HOME function `.text+0x22030..0x2209b`, repeated at 0x222e5..0x22344. All preceding segments are padded to a 512-byte boundary; the final segment ends after useful pixels. At 0x22355..0x22366, the vendor accepts `actual_bytes <= expected_bytes` and `actual_bytes + 512 > expected_bytes`; preserve that observed last-packet tolerance separately from the nominal size.

For wire segment index `i` (zero-based), decoder `.text+0x227d7..0x22815` maps:

```
x_phase = i / T                   # integer division
r = i % T
output_y = (r % L) * V + (r / L)
wire_pixel[j] -> output[output_y][x_phase + j*P]
```

With V=2, P=1, the wire sends rows 0,2,4,...,H-2, then 1,3,5,...,H-1. Each useful row is followed by padding except the last. This is temporal/interleave ordering, not a final image containing alternating missing rows. Optional software flags can reverse row order; gp+0x85 bit 0x01 or sign bit of requested output format triggers the reversal at 0x22817..0x22831.

| Mode | Useful segment bytes | Padding per preceding segment | Segment stride | Segments | Nominal received bytes | RGB565 output bytes |
|---|---:|---:|---:|---:|---:|---:|
| 640x480, V=2, P=1 | 1280 | 256 | 1536 | 480 | 737024 | 614400 |
| 720x400, V=2, P=1 | 1440 | 96 | 1536 | 400 | 614304 | 576000 |

For the generic gp byte 2 bit 0x10 case the size code doubles the horizontal divisor and segment count. That case has not been fully traced for original 5555:1110 and is excluded from the simplified mapping above.

### Header and transport evidence

No protocol frame header, magic, timestamp prefix, or initial fixed skip is parsed in this HOME receive path. The segment decoder initializes the input offset to zero at 0x226fc and adds that offset directly to each received subbuffer's data pointer at 0x2276b..0x22772. Wire bytes are passed to the pixel converter immediately. The 40-byte `lines_buf` structure on the stack is a **host-side descriptor**, not bytes from USB.

The original read-lines implementation creates read requests over the capture pipe and caller buffers (`0x4305..0x431a`), submits the first read (`0x4584..0x4591`), issues B4 (`0x45a1..0x45c1`), then submits remaining reads (`0x45f9..0x460e`). It records each request's actual returned byte count in its host-side subbuffer descriptor. URB boundaries are transport boundaries, not row headers; row segmentation is established by the stride formula. Nothing examined strips a device-supplied prefix before these bytes reach the HOME decoder.

## Remaining bounded questions

1. Finish hardware meanings for byte 30 flags; fresh progressive RGB initial value zero is established in FRAME-SETUP.md.
2. Trace the measured ADC status byte 2 bits to physical sync polarity, and determine whether bit 0x10 can occur on original hardware. A measured value is required to finish the command templates.
3. Finish ADC register names for byte 3 and analog controls 4..13. Their derivation/defaults above are established, hardware-facing labels are not all established.
4. Initial ADC-instance horizontal adjustment is resolved as zero in FRAME-SETUP.md; table override is also zero. Revisit if supporting ADC models other than original AD9884A.
5. Confirm how HOME adaptive retries changing V beyond 2 affect firmware timing, not just the static deordering formula. Retry function 0x221e0 chooses a larger divisor of H and updates V/L/count bytes.
6. Verify nominal frame lengths, padding contents, and segment ordering with a controlled capture only after USB traffic is separately authorized. Static code supplies strong evidence of no frame header on this path, but does not replace a hardware capture.
