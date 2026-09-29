# Original VGA2USB receive transport

Read-only reverse engineering of `vga2usb_bins.o_shipped` (driver 3.33.0.17), SHA256
`536b9be25b310d57a77d2888fe83cd2e499417dbb963129edc90357cf9736dbb`.
Addresses below are ELF section offsets, not loaded kernel addresses. Evidence is
`disassembly.txt`, `relocations.txt`, and the offline Ruby helper evaluation
`evaluate-home-allocation.rb` in this directory. No hardware traffic was generated.

The original 5555:1110 board uses USB operation table `.rodata+0x1a2840`.
Its `+0x50` receive operation is `.text+0x40c0`; its `+0x108` grab command
operation is `.text+0x37c0` (B4). HOME is the decoder/protocol for this board.
The channel descriptor specifies bulk IN endpoint 0x82. Hardware after firmware
0102 independently reported endpoint 0x82, max packet size 512.

## Exact first-capture requests

For RGB565 format 0x400, V=2, P=1, and the initial maximum transfer size,
the row stride is 1536 bytes for both standard modes. The nominal wire frame
size E excludes trailing padding after the last useful row:

| Mode | Useful bytes per row | E | Active requested lengths, in submission order | Minimum accepted actual total |
| --- | ---: | ---: | --- | ---: |
| 640x480 | 1280 | 737024 | 130560, 130560, 130560, 130560, 130560, 84224 | 736513 |
| 720x400 | 1440 | 614304 | 130560, 130560, 130560, 130560, 92064 | 613793 |

130560 is 85 whole padded rows. The final request contains 55 rows for 640x480,
or 60 rows for 720x400, with useful bytes only for its last row. These are
multiple-row requests, not one request per scanline.

The USB table `+0x30` function `.text+0x3aa0` obtains the maximum through
`os_bususb_max_transfer_size` at `.text+0x43140`. That helper initially returns
131072 (0x20000); if board-instance `+0x110` exceeds 4096, it returns half that
stored value instead (`0x43146..0x43165`). Allocation failure can adapt that
state (`0x4401..0x442a`), so the table above describes the initial successful
allocation case, not every later fallback.

HOME table `.rodata+0x1a3e60` has `+0x28 -> 0x20d10` (subbuffer length),
`+0x30 -> 0x211a0` (allocated entry count), and `+0x38 -> 0x22030` (E).
With a synthetic initial cap of 131072, executing just the first two arithmetic
helpers offline gives length 130560 and allocated count 510 for 640x480,
425 for 720x400. Those counts are allocation capacity, not active USB request
counts. The Ruby harness patches profiling entry calls to NOP and indirect
thunks to register jumps, supplies only a constant cap callback, and rejects
unexpected external calls. It does not execute a vendor USB function.

`usb_lines_verify_realloc` (`0xf60`) obtains count through protocol `+0x30`,
length through `+0x28`, and calls `usb_lines_alloc(count, length)` at `0xff9`.
`usb_lines_alloc` (`0xe80`) creates all entries and their buffers. The lines
header has allocated/active count at +0, completed count at +4, pointer array
at +0x18; each 24-byte entry has index +0, requested length +4, actual length
+8, and data pointer +0x10. At `0x4830..0x49d7`, the format-0x400 receive path
walks entries against E, reduces the active count to ceil(E/130560), and
changes the last active entry's requested length to the remaining bytes.
The original count and last length are restored at `0x44d1..0x44f2`.

## Queue and completion ordering

Read objects are created for every active entry at `0x4305..0x431a` before B4.
Each object's requested length is passed into the bulk URB's
`transfer_buffer_length` (`os_usb_read_create`, `0x42e1f`; submit path
`0x42f3a..0x42f63`). Submission ordering is explicit:

1. Submit entry 0 with the submit argument `esi=1` (`0x4584..0x4591`).
2. Send B4 through USB table `+0x108` (`0x45a1..0x45c1`).
3. Submit entries 1 through active-count minus 1, also `esi=1`
   (`0x45f9..0x460e`).
4. Wait for callbacks; on completion or error, cancel pending requests in
   reverse index order (`0x46d9..0x4711`).

`os_usb_read_submit` at `0x42f00` preserves normal URB flags for `esi=1`;
it only ORs flag bit 0 when `esi=0` (`0x42fa9`). Thus the actual receive caller
permits a successful short read. The low-level completion function `0x42a20`
stores URB actual length on success (`0x42ab3..0x42abb`), or a negative status
on failure, then invokes the registered receive callback at `0x4050`.

That callback stores a nonnegative result at entry +8 and increments completed
count (`0x4069..0x4070`). If actual < requested it sets owner +0x30 (done) and
wakes the wait event (`0x4076..0x408c`). A full completion of the last active
entry also sets done (`0x409d..0x40a9`). Full intermediate entries merely wake
the event. A negative completion sets done too (`0x40ab..0x40b4`). No branch
refills, resubmits, or appends a second read to a partially completed entry.
The main receive path sums actual entry lengths through `lines_buf_total`
(`0x43f4..0x43f9`), after cancelling pending reads.

Consequently a successful 1536-byte first transfer is a short completion,
not a complete frame and not evidence of a one-row-per-URB legacy protocol.
The exact request segmentation above is a concrete difference from a single
614304-byte libusb request. Static analysis does not establish that this
difference alone causes, or will resolve, the observed one-row result.

## Frame acceptance and retry

HOME's capture operation at `0x22290` calls receive once. At
`0x22355..0x22366`, a nonnegative actual total is accepted only if
`actual <= E` and `actual + 512 > E`, equivalently `E-511 <= actual <= E`.
Exactly 512 bytes missing is rejected. This tolerance is an acceptance test,
not a promise that missing useful pixels are safe to decode: raw consumers
must retain actual length and avoid reading absent data. A 1536-byte total
fails and goes to the bad-frame error -32 (`0x22a4b`).

HOME retry helper `0x221e0` can advance the vertical phase count to a larger
value dividing H, set gp vertical increment/lines/count, and update retry
state. The failure tracking and conditional call are `0x22929..0x22a5c`.
It returns the current capture's error; it does not loop back into receive or
send another B4 inside that same capture attempt. A later capture may use
changed parameters. This is no mechanism for stitching short reads.

## Cleanup: no B5 or explicit drain for a positive short

Receive `0x40c0..0x4aa3` does not invoke USB table +0x110 and contains no
normal-short path issuing a stop/restart command or draining bulk data.
Cancellation is followed by reference-counted cleanup at `0x3ff0`: delete
read objects, release lines reference, signal the cleanup event. Before a
subsequent receive, `0x4159..0x416b` calls helper `0x3fa0` on vg +0x320 to
wait for that previous cleanup event. This is host request lifetime cleanup;
it reads no USB payload.

B5 does exist: USB table +0x110 (`.rodata+0x1a2950`) points to `0x32b0`,
which sends 40/B5, value=0, index=0, data length=0 (`0x32ba..0x32d6`).
The named wrapper `usb_io_reconnect` at `0x4c20` invokes that operation.
Firmware checker `0x3dd0` invokes it after a C6 comparison mismatch
(`0x3e77..0x3e85`). It is therefore not evidence of a capture-stop B5.
The named `usb_io_stop` wrapper (`0x4c50`) instead examines table +0x118;
the original table does not supply a relocation/function at that slot.

Endpoint reset at `0x49dc` is conditional on vg +0x328 (`0x4807..0x4811`).
The negative -71 result path sets that flag (`0x47cc -> 0x449e..0x44ab`).
`os_usb_pipe_reset` (`0x42cd0`) checks endpoint status and calls
`usb_clear_halt`; it is endpoint error recovery, not an unconditional action
after a positive short. A previously set flag can also reach this path.
Nothing in the identified normal-short path justifies adding B5, unsolicited
bulk drain, or unconditional clear-halt before the next B4.

Unresolved: why the current firmware/source combination returns exactly one
padded row; whether matching the legacy queued request sizes changes that
behavior; and which B1/sync state accompanied that particular attempt.
Those require hardware observations, not invented transport semantics.
