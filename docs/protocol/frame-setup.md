# Original 5555:1110 setup defaults and B0/B4 dispatch

Static investigation, 2026-09-28. No USB traffic, target writes, builds, module loads, or production edits were performed. Input is vendor `vga2usb_bins.o_shipped` version 3.33.0.17, SHA256 `536b9be25b310d57a77d2888fe83cd2e499417dbb963129edc90357cf9736dbb`. Addresses are section-relative; nearest-symbol objdump labels do not establish anonymous function ownership. Full disassembly/relocations/rodata are retained alongside this report.

**Result: a complete default first B4 payload is established for each of the two exact standard timing records, with byte 2 supplied by valid B1 measurement. Original 5555:1110 does not send B0: its USB function table makes the common B0 helper return before serialization or USB transfer.** Firmware acceptance of unsolicited B0 is not established by this code.

This report corrects earlier generic fallback analog values in FRAME-PROTOCOL.md: the selected AD9884A table supplies `1f / 70 70 70 / 20 20 20` at gp offsets 6..12. Those values are not the generic fallbacks `10 / 80 80 80 / 00 00 00`.

## Dispatch and default allocation evidence

Original board `usb_board_vga2usb` is `.rodata+0x11b240`; `+0x30` relocates to `usb_io_create_vga2usb` `.text+0x4d10`, and `+0x40` to one-channel table `.rodata+0x11d1c0`. The channel `+0x38` relocates to `AD9884A_hinterlaces`, `+0x40` to `grab_protocol_home`, and `+0x58` to `adc_AD9884A_create`.

`usb_board_init` allocates a 0x338-byte vgrabber at `.text+0x19ce..0x19dc`, then explicitly zeroes the whole allocation: first/last qwords at 0x19eb/0x19fb and aligned `rep stosq` at 0x1a1f. `gp` is embedded at vgrabber+0xf0 (established at `vgrabber_detect_video_mode` 0x290bb and callers). Therefore gp byte 29 = vgrabber+0x10d starts at zero. `vgrabber_init` writes word 0x8080 at vgrabber+0x10b at 0x283a1..0x283a6, establishing gp bytes 27/28 = 128/128. Its global default flags are `v2ucom_vgrab_flags`, an initialized-zero `.bss` symbol (offset 0x20, size 4), copied to vgrabber+0x173 = gp+0x83 at 0x28387..0x28399.

`VESA_clear_gp` 0x25640 preserves gp+27..29 and gp+31..34; it clears byte 30. Noise setter `vgrabber_set_noise_filter` 0x28cd0..0x28cfd accepts levels 0..5 and writes vgrabber+0x10d; threshold setters 0x28d20/0x28d80 write +0x10b/+0x10c. No explicit setting means these fresh-allocation defaults persist.

`adc_AD9884A_create` 0x11480 passes `.rodata+0x1a3620` to `adc_default_create`. That constructor explicitly writes ADC instance+0x28 = 0 at `.text+0xeaab` (also +0x20 = 0 at 0xeaa3). Original AD9884A detect callback is `.text+0x11160`, established by table relocation at `.rodata+0x1a3638`; it does not write instance+0x28. The nonzero adjustment stores at 0x12478 and companion zero stores 0x123a0/0x12513 belong to TDA8754 paths selected by a different ADC table.

## Horizontal start and analog defaults

`VESA_compute_VGA_parameters` begins at 0x25920. At 0x25936..0x2593d it loads vgrabber+0x1c8 (ADC instance), then its selected ADC operations table. Its controls therefore come from **ADC ops**, not the channel descriptor.

| Source in original ADC ops `.rodata+0x1a3620` | Raw value | Result in gp |
|---|---:|---|
| +0x6a | u16 0 | No horizontal sync override |
| +0x6c | u8 31 | gp[6] = 0x1f (0x25a39..0x25a4f) |
| +0x5c / +0x5e | u16 32 / 192 | gp[7..9] = (32+192)/2 = 112 = 0x70 (0x25a52..0x25a8a) |
| +0x50 / +0x52 | u16 16 / 48 | gp[10..12] = (16+48)/2 = 32 = 0x20 (0x25a95..0x25ab5) |

The horizontal start expression at 0x25ae9..0x25b13 is:

```
start = (adc_ops.override_at_6a != 0 ? override : h_sync) + h_back
if adc_instance.word32_at_28 != 0:
    start += h_sync
```

Original AD9884A override and adjustment are both zero. Consequently start is exactly 96+48 = 144 for the listed 640 mode, and 108+50 = 158 for the listed 720 mode. The prior unresolved “board-instance adjustment” is actually this ADC-instance field.

Fresh VESA calculation sets gp[13] low bits to 3. Byte 30 begins zero after VESA_clear_gp, and its three conditional changes remain inactive for these defaults: mode flags 0x20 are clear; detector sets gp+0x33 = 1 at 0x11182 (bit 0x04 clear); default gp+0x83 flags are zero (mask 0x42000 clear). Thus gp[30] = 0 exactly in the initial progressive RGB path. Hardware meanings of optional flag bits are not fully named here.

## B1 status and selecting a real mode

AD9884A detector 0x11160 requests 10 bytes through its ADC I/O read callback; original USB callback 0x630 sends vendor IN 0xc0/B1/value 0/index 0/length 10. Detector byte-swaps the first four words at 0x111c8..0x111e6. It checks the first BE16 measurement for zero at 0x111ea..0x111ed; zero branches to VESA_clear_gp at 0x113a0 and returns a no-signal error.

For a successfully selected mode it writes `gp[2] = B1[8] & 3` at 0x112ab..0x112bd, and derives gp[3] from the selected timing's clock estimate at 0x112c2..0x11394. Both listed clocks are below 35 MHz and P=1, giving byte 3 zero.

The supplied hardware observation `00 00 00 00 00 00 0b b8 02 00` has status low bits 2 but its first measurement is zero. It is a no-source result, not evidence of either selected mode. Once valid measurements identify one of the exact timing records, `SS = B1[8] & 3` is the sole remaining measured byte in the default templates below. Do not substitute the no-source status into an arbitrary forced mode and call it measured setup.

## VESA calculation, HOME initialization, and first capture

Exact timing records are described in FRAME-PROTOCOL.md:

| Mode record | Refresh | Active/front/sync/back horizontally | Active/front/sync/back vertically | Record flags |
|---|---:|---|---|---:|
| table index 26 / .rodata+0x1a4230 | 59940 mHz | 640/16/96/48 | 480/10/2/33 | 0x03 |
| table index 20 / .rodata+0x1a41a0 | 70000 mHz | 720/22/108/50 | 400/14/25/11 | 0x83 |

VESA calculation initializes progressive gp vertical phases to V=1, L=H, gp[25]=1 at 0x25963..0x2598c and 0x25ac0..0x25ad4. Horizontal phase P=1; original channel's AD9884A_hinterlaces uses estimated clocks 25,174,800/28,350,000 Hz. VESA may add software mode flag 0x04 when its phase callback succeeds; neither mode acquires interlaced flag 0x20.

After successful changed-mode detection, `vgrabber_detect_video_mode` 0x2936b..0x2938d writes the full-frame ROI: gp+35=0, +39=0, +43=W, +47=H. It then applies ADC parameters through 0x293a4 -> 0x26460 -> 0x27650 -> adc_setparameters; this common path reaches B0's early return on original hardware.

Next the detector reaches protocol table +0x50 at 0x29218..0x29227. HOME table `.rodata+0x1a3e60` +0x50 relocates to `.text+0x220a0`. That initializer changes exactly these gp fields:

| gp offset | Type | Progressive value | Interlaced-mode value |
|---:|---|---:|---:|
| 18 | native u16 | 2 | 4 |
| 20 | native u16 | H / 2 | H / 4 |
| 25 | u8 | 2 | 4 |

Evidence: 0x220e4..0x22119 tests gp+0x7b mask 0x20, sets the phase count, divides gp height at +16, and stores L. It also resets HOME-private retry/time state at protocol-instance +0x60..+0x7b; it does not change any other gp byte. B4 later reads these HOME-modified values; mode calculation's V=1 state is not the default first B4 state.

Default first B4 setup uses vendor OUT `0x40 / B4 / value 0 / index 0 / length 31`. Sender 0x37c0 copies gp[0..30] and converts offsets 0,14,16,18,20,22 to BE16. No pixel-format selector or appended ROI is sent. Requested native RGB565 selects only the software decoder.

```
640x480@59.94:
03 20 SS 00 08 14 1f 70 70 70 20 20 20 03 02 80 01 e0 00 02 00 f0 00 90 21 02 01 80 80 00 00

720x400@70:
03 84 SS 00 08 14 1f 70 70 70 20 20 20 03 02 d0 01 90 00 02 00 c8 00 9e 24 02 01 80 80 00 00
```

Every constant in these templates is established for the exact listed records with fresh defaults. Measured-mode identification, explicit analog-control changes, force/crop flags, and HOME retry changes still matter when extending the implementation beyond this initial path. FRAME-PROTOCOL.md documents bulk endpoint 0x82, buffer sizes, RGB565 decoding and deordering.

## B0 early return: original hardware skips this request

`usb_adc_io_create` 0x840 stores board at its instance +0x10 and registers ops `.rodata+0x17cca0`; that ops table +0x28 relocates to serializer `.text+0x670`. It stores the channel's copy length (channel +0x14 = 17) in USB ADC I/O instance +0x38 at 0x8ac.

At 0x686..0x692 serializer loads USB ADC I/O +0x10 -> board +0x60 -> USB I/O ops. Then:

```
0x695: xor eax,eax
0x697: test byte [usb_io_ops+0x0e], 1
0x69b: je 0x6ac             # serialization only when flag is clear
0x69d..0x6ab: return 0      # when flag is set
```

Original ops `.rodata+0x1a2840` bytes +0x0c..+0x0f are `00 00 01 00`; +0x0e is therefore exactly 1. The ops selection is `usb_io_create_vga2usb` 0x4d16; board_init stores the chosen USB I/O as board+0x60. This is static dispatch evidence, not a guess based on command names.

## Complete common 39-byte B0 serializer map (not emitted on original)

If the early-return flag were clear, original channel copy length 17 would yield 14+17+8 = 39 bytes. ADC ops +0x10 is zero, so adc_set_ioparams 0xebc5..0xebe1 supplies no additional ADC-specific block (`rdx=0`, `ecx=0`). The serializer's optional block at 0x7b8..0x7e1 is absent.

| Wire offsets | Source / serialization |
|---|---|
| 0..13 | copy gp[0..13]; swap native u16 at gp+0 to BE16 |
| 14..30 | copy 17 bytes gp[14..30]; swap gp native u16 offsets 14,16,18,20,22 to BE16 |
| 31..32 | BE16 of low 16 bits of native u32 gp+35: ROI x |
| 33..34 | BE16 of low 16 bits of native u32 gp+39: ROI y |
| 35..36 | BE16 of low 16 bits of native u32 gp+43: ROI width |
| 37..38 | BE16 of low 16 bits of native u32 gp+47: ROI height |

Evidence: prefix copy/swaps 0x6cb..0x754; appended words 0x77d..0x7b1; setup 0x7e4..0x800 is vendor OUT `0x40 / B0 / value 0 / index 0 / computed length`. It allocates a 64-byte scratch buffer; allocation size is not USB payload length. The conditional swaps of gp+31/+33 at 0x75a..0x777 apply only when copy length >19, and are inactive for original channel length 17. This means gp[31..34] is omitted from the prefix while ROI from gp[35..50] is packed separately.

At the first successful changed-mode apply point, before HOME reset, the conceptual serialized buffers would be:

```
640x480 (V=1,L=480,count=1, ROI 0,0,640,480):
03 20 SS 00 08 14 1f 70 70 70 20 20 20 03 02 80 01 e0 00 01 01 e0 00 90 21 01 01 80 80 00 00 00 00 00 00 02 80 01 e0

720x400 (V=1,L=400,count=1, ROI 0,0,720,400):
03 84 SS 00 08 14 1f 70 70 70 20 20 20 03 02 d0 01 90 00 01 01 90 00 9e 24 01 01 80 80 00 00 00 00 00 00 02 d0 01 90
```

These 39-byte arrays illustrate a complete serializer mapping. **They are not vendor-emitted original-device commands and should not be added to original capture startup.** If serialization were invoked after HOME initialization, offsets 18/20/25 would instead contain V=2/L=H/2/count=2; the sender serializes the supplied gp snapshot and does not select those values itself.

## Verification and remaining limits

Ruby constructed both prefix/payload families and checked lengths 31/39; raw rodata extraction independently verified ops values and skip flag. Full-frame ROI naming is established by the HOME crop caller at 0x23100..0x23265 and mode-detection stores at 0x29375..0x2938d.

The default first B4 no longer has unresolved preserved-control or horizontal-adjustment bytes. Remaining boundaries are physical meaning of the two measured status bits, optional byte-30 hardware semantics, and hardware verification of capture sizes/order. Fresh defaults provide a concrete initial implementation, but do not replace signal-mode selection or confirm a successful frame capture.
