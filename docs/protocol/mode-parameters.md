# Original VGA2USB mode scoring and generated parameters

Bounded offline reverse engineering, 2026-09-28. No USB requests, target writes,
builds, module loads, resets, or reboots were performed for this analysis.
Addresses below are section-relative offsets in the vendor relocatable ELF,
not runtime addresses. Input:

`vendor-driver/usr/src/vga2usb-3.33.0.17/vga2usb_bins.o_shipped`

SHA256: `536b9be25b310d57a77d2888fe83cd2e499417dbb963129edc90357cf9736dbb`.
The complete disassembly, relocations, and raw `.rodata` in this scratch
directory are the evidence oracle. Existing `ABI-AUDIT.md` and `ADC-PROTOCOL.md`
remain applicable. Hardware startup and the source-off B1 response reported by
the parent are separate observations; the synthetic examples below are not
hardware measurements.

## Selection path for the original AD9884A

The original model's AD9884A constructor is `.text+0x11480`, which passes ops
`.rodata+0x1a3620` to `adc_default_create`. Its mode callback is
`.text+0x11160`. This callback asks the I/O ops for 10 bytes (0x111b2), the B1
status request documented in `ADC-PROTOCOL.md`. It decodes the first four
16-bit values as big endian (0x111c8–0x111e6), but only the first two participate
in this matching path:

* `T = BE16(response[0..1])`, the measured period. `T == 0` clears generated
  parameters and returns `-123` (0x111ea, 0x113a0).
* `L = BE16(response[2..3])`, measured total vertical lines.
* Measured refresh stored at `gp+0x37` is `floor(2,000,000,000 / T)` in mHz
  (0x11218–0x1122a).
* The value passed to matching is `R = floor(20,000,000 / T)` in tenths of Hz
  (0x1122f–0x11238).

At 0x11203–0x11250 the exact matching call shape is:

```c
VESA_find_match(board, L, R, 0, 0, 0, 0,
                (load_u32(gp + 0x83) >> 17) & 1);
```

Thus it supplies no polarity or sync-width measurements and no interlace
constraint. In particular, `kind == 0` does **not** discard interlaced table
entries: the helper mask is zero. The optional final argument is a resolution
aspect-ratio preference. Response byte 8's low two bits are copied into
`gp+0x02` only after a changed mode has been computed (0x112ab–0x112bd).
They do not inform matching. No match clears parameters and returns `-124`
(0x113b2).

`VESA_get_vesamodes_table` at 0x255d0 returns `.rodata+0x1a3fc0` and count 137.
`vgrabber_init` at 0x283d0–0x28418 allocates and copies all 137 rows unchanged
and in order into `board+0x1e0`, setting count at `board+0x1e8`. A row is 24
bytes, little endian:

```text
u32 refresh_mHz;
u16 width, hfront, hsync, hback, height, vfront, vsync, vback;
u32 flags;
```

## Exact scoring and filtering

`VESA_find_match` is 0x25700, candidate helper 0x25470, score helper 0x253c0.
For positive ordinary timing values the arithmetic is:

```text
blank = vfront + vsync + vback
interlaced = (flags & 0x20) != 0
table_lines = height + blank + (interlaced ? blank + 1 : 0)
if interlaced: table_lines = floor(table_lines / 2)
dr = floor((refresh_mHz + 49) / 100) - R
dl = table_lines - L
ds = S > 0 ? S - vsync : 0
score = dr*dr + dl*dl + ds*ds
if preference != 0:
    score += 300 - 3 * floor(width * 100 / height)
```

The vendor's rounding constant is **49**, not 50. The score helper computes
these differences and squares at 0x253d6–0x25442; the preference is
0x25445–0x2545e. Width is divided by active height, including for interlaced
entries.

The candidate helper checks `(flags & 3) == 3` (0x254c3–0x254ce), and requires
`L >= height+6`, or `L >= floor((height+6)/2)` for interlaced rows
(0x254d0–0x254e3). It then checks `(flags & mask) == desired` (0x254e5), and,
only if explicitly requested, `vsync == S` (0x254ed–0x254fb). Best score begins
at 3000 (0x25741), and a row replaces the winner only for a **strictly smaller**
score (0x25526–0x2553b). Therefore a score of 3000 is rejected and a tie preserves
the earlier row.

The generic matcher first normalizes `S`: if `L/2 < S < L`, replace `S` by
`L-S`; if `S > 39`, use zero (0x2573f–0x25764, 0x258af). Its base desired/mask
value is 0x20 only when interlace kind equals 2, otherwise zero (0x2576a–0x25784).
Equal supplied polarity arguments take one helper pass using the base as both
desired and mask (0x25864–0x2587b). Unequal polarities first add mask 0xc0 and
desired 0x80 when the sixth argument is nonzero, otherwise 0x40
(0x25791–0x257a6). That pass enforces exact sync width only for `4 <= S <= 7`.
It then falls back to a pass without polarity/exact-width constraints. The
fallback wins if no constrained match exists, or `2*fallback_score <
constrained_score` (0x25816–0x2582d). None of those optional constraints is
enabled by the original callback.

## Relevant exact table entries

Indices are zero based.

| Index / address | Refresh mHz | Horizontal: active, front, sync, back | Vertical: active, front, sync, back | Flags |
|---|---:|---|---|---:|
| 26 / 0x1a4230 | 59940 | 640, 16, 96, 48 | 480, 10, 2, 33 | 0x03 |
| 20 / 0x1a41a0 | 70000 | 720, 22, 108, 50 | 400, 14, 25, 11 | 0x83 |
| 101 / 0x1a4938 | 59942 | 720, 16, 62, 60 | 480, 9, 6, 30 | 0x83 |

The 720×400 vertical components are the actual vendor row. Do not replace
them with remembered standard timing values. Row 26 totals 800×525; row 20
totals 900×450.

Synthetic `T=33367, L=525` gives `R=599`. Rows 26 and 101 both score zero
without preference, so the unchanged default list chooses 640×480. With
preference enabled, row 26 scores -99 and row 101 -150, choosing 720×480.
This callback cannot independently prove the horizontal resolution from its
B1 observations. Synthetic `T=28571, L=450` gives `R=700`; row 20 is the only
candidate below 3000, scoring zero without preference and -240 with it.

## Generated fields for the two selected rows

`VESA_compute_VGA_parameters` is 0x25920. The values below assume the original
AD9884A ops, default ADC instance, progressive selected row, full crop, and no
user tuning/format overrides. The ADC instance is 48 bytes; `adc_default_create` at
0xea70 initializes offsets 0x20/0x28 to zero at 0xeaa3/0xeaab. Ops bytes prove:

```text
ops+0x50 u16 = 16, ops+0x52 u16 = 48
ops+0x5c u16 = 32, ops+0x5e u16 = 192
ops+0x6a u16 = 0,  ops+0x6c u8  = 31
```

The constructor reference to these ops is at 0x11489; raw values are in
`.rodata+0x1a3670` through 0x1a368c.

| gp offset / width | Formula and evidence | 640×480 59.94 | 720×400 70 |
|---|---|---:|---:|
| 0x00 / u16 | Htotal, 0x25a1b–0x25a39 | 800 | 900 |
| 0x02 / u8 | B1 byte 8 & 3, 0x112ab–0x112bd | measured | measured |
| 0x03 / u8 | clock control below | 0x00 | 0x00 |
| 0x04 / u8 | clamp-related generated value, 0x25b53–0x25bac | 8 | 8 |
| 0x05 / u8 | clamp-related generated value, 0x25ba3–0x25bc2 | 20 (0x14) | 20 (0x14) |
| 0x06 / u8 | ops+0x6c, fallback16 only if zero, 0x25a3c–0x25a4f | 31 (0x1f) | 31 (0x1f) |
| 0x07,08,09 / u8 | midpoint `(32+192)/2`, 0x25a52–0x25a8a | 112 (0x70) each | 112 (0x70) each |
| 0x0a,0b,0c / u8 | midpoint `(16+48)/2`, 0x25a95–0x25ab5 | 32 (0x20) each | 32 (0x20) each |
| 0x0d / u8 | OR3 into cleared value, 0x25a76–0x25abc | 3 | 3 |
| 0x0e / u16 | active width, 0x25ac7 | 640 | 720 |
| 0x10 / u16 | active height, 0x25ac0 | 480 | 400 |
| 0x12 / u16 | progressive factor D=1, 0x25963–0x25989 /0x25acf | 1 | 1 |
| 0x14 / u16 | height/D, 0x25ac4–0x25ad4 | 480 | 400 |
| 0x16 / u16 | `(ops6a ? ops6a : hsync)+hback`, 0x25ae9–0x25b13 | 144 | 158 |
| 0x18 / u8 | `vback + ((flags&0x80) ? vsync : 0)`, 0x25ad8–0x25ae6 | 33 | 36 |
| 0x19 / u8 | D, 0x25acb | 1 | 1 |
| 0x1a / u8 | horizontal partition P, 0x259df–0x25a17 | 1 | 1 |

If ADC instance+0x28 is nonzero, gp+0x16 adds a further hsync at 0x25b07.
That instance field is zero at construction. For gp+0x04/+0x05, the threshold
is 1366 active pixels per partition, or 1280 when gp+0x33 bit0x20 is set. Below
the threshold gp4=8, `delta=hback-16`, gp5=8 if delta<8, otherwise
`min(delta,20)`. Both rows give gp5=20. Above threshold the corresponding
values are gp4=56, delta=hback-112, minimum16 and maximum32.

The chosen 24-byte timing row is copied into gp+0x67 (0x25994–0x259af).
Normal successful partition calculation adds validity bit0x04 to row flags
at gp+0x7b (0x25a0f–0x25a17), resulting in 0x07/0x87 respectively. Those are
in-memory timing flags, not a claim about a B0 byte. Format/control fields
gp+0x1b..0x22 are preserved or depend on format/tuning flags and belong to
the independent B0/defaults analysis.

## Partition and exact clock-control thresholds

`AD9884A_hinterlaces` is 0x113d0. For the two progressive rows:

```text
pixel = floor(Htotal * Vtotal / 1000) * refresh_mHz
q = max(ceil(width / 1024), ceil(pixel / 50,000,000))
P = q <= 1 ? 1 : ((q + 1) & ~1)
```

For interlaced rows, Vtotal before this pixel calculation includes the extra
blank+1 and the resulting pixel value is divided by two after multiplication
(0x113f1–0x11441). Width ceiling and pixel ceiling are
0x11444–0x11460; even partition rounding is 0x11463–0x11471.

Row 26: `floor(800*525/1000)*59940 = 25,174,800`, hence P=1.
Row 20: `floor(900*450/1000)*70000 = 28,350,000`, hence P=1.
This integer ordering deliberately reproduces the binary.

The mode callback recomputes the same pixel estimate at 0x112c2–0x1132b,
then sets gp+0x03 at 0x11330–0x11394:

| Partition | Pixel range | gp+0x03 |
|---|---|---:|
| P=1 | pixel < 35,000,000 | 0x00 |
| P=1 | 35,000,000 <= pixel < 50,000,000 | 0x04 |
| P=1 | pixel >= 50,000,000 | 0x08 |
| P!=1 | pixel < 50,000,000 | 0x00 |
| P!=1 | 50,000,000 <= pixel < 100,000,000 | 0x04 |
| P!=1 | pixel >= 100,000,000 | 0x24 |

The comparisons use 35,000,000-1 and 50,000,000-1 where indicated by
`jbe`; the boundaries above preserve the exact inclusivity.

## The four extra B0 values are crop coordinates

gp+0x23, +0x27, +0x2b, +0x2f are four 32-bit signed rectangle values:
`x, y, width, height`. `gp_crop_apply` at 0xc990 proves their meaning: an
all-zero rectangle expands width from gp+0x0e and height from gp+0x10
(0xc9ea–0xca01), while any nonzero rectangle calls
`v2ucom_crop_normalize_rect` (0xc9b7–0xc9c1). It copies rectangle bytes 0..7
into gp+0x23 and 8..15 into gp+0x2b (0xc9cf–0xc9db).

`VESA_clear_gp` at 0x25640 does not clear gp+0x23..0x32.
`VESA_compute_VGA_parameters` does not write those offsets. Crop application
is a separate step after mode computation, so simply zeroing them does not
reproduce the full-crop request. The original B0 serializer at 0x0670 reads
their low 16 bits and converts to big endian at 0x077d–0x07b1. With full crop,
the eight trailing bytes after the common header and 17-byte I/O parameters
are:

```text
640×480: x=0 y=0 width=640 height=480 => 00 00 00 00 02 80 01 e0
720×400: x=0 y=0 width=720 height=400 => 00 00 00 00 02 d0 01 90
```

These are not four extra ADC registers. They are the reason the original
B0 request is 39 bytes (14-byte common header + 17 I/O bytes + 8 crop bytes).

## Offline reproduction and limits

`mode-score.rb` reads the dumped vendor `.rodata`; it performs the exact
original callback's unconstrained candidate filtering and scoring, including
interlaced rows. It does not send requests. Example commands:

```sh
ruby mode-score.rb blob-rodata.bin 33367 525
ruby mode-score.rb blob-rodata.bin 33367 525 1
ruby mode-score.rb blob-rodata.bin 28571 450
```

The optional fourth argument `1` enables the aspect-ratio preference. The
reported candidates are sorted by score then table index; the first candidate
matches the helper's strict-update selection. Ruby syntax check passed, and
all four combinations of the two examples with preference disabled/enabled
were run successfully. This reproduction covers selection, partition,
clock-control and crop/default parameter generation. It does not establish
that a generated USB request captures correctly on hardware, and it does
not provide a complete format-dependent B0 header. That serialization and
default-format mapping is a separate bounded task. A complete request must
use its proven format defaults, the measured B1 byte8&3, and the fields above;
there is no support here for speculative ADC register writes.
