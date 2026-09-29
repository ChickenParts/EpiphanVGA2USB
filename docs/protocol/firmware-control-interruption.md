# Original firmware control requests interrupt capture

Static audit on 2026-09-29, original VGA2USB firmware 0102 extracted from the
hash-pinned vendor object 3.33.0.17. No firmware writes, USB resets or new device
commands were used in this investigation. Addresses below are 8051 code addresses,
not offsets in the vendor ELF host driver.

## Evidence and dispatch

Firmware segments are reconstructed with `VendorFirmware` from
`tools/vendor_firmware.rb`. The 65 decoded segments contain 7736 bytes. For offline
analysis only, placing them at their recorded addresses in a zero-filled 8192-byte
buffer produces SHA256
`aa7bc2fe9ab8493c502fa1693db042989a21cc433ea1a715d2ba5de8e70e8e74`.
The zero-filled holes are an analysis representation, not a firmware upload image.
Dis51 0.5 follows the reset and interrupt entries; the computed two-byte jump
tables at 0x0a9c and 0x134f require explicit entry addresses.

The relevant paths are:

| Address | Meaning established by decoded instructions |
| --- | --- |
| 0x1700 | USB autovector entry jumps to 0x07e4 |
| 0x07ea | USB setup handler calls 0x1ed3 before setting its setup-pending bit |
| 0x1ed3 | Sets bit 6, then writes 0xff to XDATA 0xe6f5 |
| 0x1338..0x134e | Reads SETUPDAT request byte at 0xe6b9 and dispatches requests 0xb0..0xb7 |
| 0x1351 | Request 0xb1 jumps to 0x1364 |
| 0x1364 | Stores 0xb1 in command byte 0x70 |
| 0x10ec..0x1121 | Main command dispatcher selects 0x0800 for 0xb1 |
| 0x0849..0x084e | B1 measurement writes 0xff to 0xe6f5 again |
| 0x0857..0x085c | Measurement selects IFCONFIG 0xca; on exit it selects 0x88 |
| 0x01d3, 0x0364, 0x03c1, 0x0428 | Capture waits test bit 6 to leave interrupted acquisition |

The [Infineon/Cypress FX2 manual](https://www.infineon.com/assets/row/public/documents/24/44/infineon-ez-usb-fx2-trm-version2-2-usermanual-en.pdf?fileId=8ac78c8c7d0d8da4017d0ef4b2a80851),
section 15.12.16, identifies 0xe6f5 as GPIFABORT and establishes that writing
0xff immediately aborts the active GPIF transaction. Section 15.12.15 names
0xe6f4 GPIFREADYSTAT; it is not a passive copy of the complete B1 measurement.
Thus the host's periodic B1 request during a pending frame is destructive even
when the returned timing matches the previous mode. Reopening a USB handle after
observed timing loss does not remove that interference.

The relevant setup-interruption helper bytes are:

```text
1ed3: d2 06          setb bit 6
1ed5: 90 e6 f5       mov dptr, #0xe6f5
1ed8: 74 ff          mov a, #0xff
1eda: f0             movx @dptr, a
1edb: 22             ret
```

## Host correction and limits

B1 measurement runs before the first image read and B4, or after all submitted
image reads have completed or been cancelled and drained. No timing query runs
with an image read pending. This preserves the recovered read-before-B4 ordering.

The first correction removed intrusive pending-read queries; it could still wait
forever when VGA loss produced no image completion. Scheduled reacquisition now
wakes the event loop after four phase schedules, with a minimum interval of one
second. Available callbacks run first. If reads remain pending, the host cancels
and drains them, then measures timing. A complete frame at the tick is retained.
USB control and bulk timeout arguments remain zero. This is an interruption and
retry of unfinished acquisition; it is not a runtime limit on the program.

Unchanged supported timing retries collection. Repeated scheduled interruptions
reopen the capture session while the viewer remains running; they do not advance
short-frame phase tuning to exhaustion. Changed or absent timing emits a black
canvas and reopens the USB session. Every continuous frame uses the same720×480
canvas and1036815-byte packet, even when the source resolution changes.
One-shot screenshots retain native source dimensions.

The regression asserts no queued reads at every B1 call. Delayed completion after
250 ms crosses two old observation ticks and still yields a full frame without
cancellation. Stalled-read cases prove cancel/drain before absent, changed and
unchanged timing. A longer sequence runs720×400, loss,640×480,800×600,
1024×768 withP=2, and720×400 through actual USB mocks, HOME decode and rendering.
Explicit-preset divergence and repeated unchanged stalls are checked separately.
Real FFmpeg decodes actual-renderer packets across different source geometries.

The static finding explains one failure mechanism; it does not prove every
historical stall had the same cause. A [physical cold power cycle](../hardware/reset-recovery-2026-09-29.md)
verified clearing and automatic return with the same processes. Graphics/text
transitions and repeated reset endurance remain pending. A passive observer would require a separately
established firmware path: substituting an arbitrary ADC or memory request still
triggers the identified setup handler. Negative USB errors and physical unplug
remain separate from supported VGA timing reacquisition.

## Offline reproduction

Use [Dis51 0.5](https://plit.de/asem-51/dis51.html); its C source archive at
`https://plit.de/asem-51/dis51-0.5.tar.gz` has SHA256
`a6241f142324bb175ed94a663b2caec53e06943320ef6e127015746e13c9a4f6`.
Build its five C files with Ninja, `-std=gnu89 -Idis51-0.5`, and feed Intel HEX
records made from the reconstructed analysis buffer. Entry points are 0, 51, 67,
75, 83 and 99, plus every two-byte entry in 0x0a9c..0x0ab2 and 0x134f..0x135d.
Disassemble in listing mode (`-l`) to retain code addresses and raw instruction
bytes.
