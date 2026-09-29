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

The host now performs B1 measurement before queueing the first image read and
before B4. Following a short frame, it cancels and drains all remaining submitted
reads before measuring again. It sends no status request while frame reads are
pending. This preserves the recovered read-before-B4 ordering while eliminating
an unrelated setup interrupt from active collection. USB transfer deadlines remain
zero; no elapsed frame cutoff is introduced.

The regression asserts no queued reads at every B1 call. A separate process
releases simulated USB completion after 250 ms, crossing two old observation
ticks, for streaming and one-shot capture. The old implementation fails this
regression by sending B1 while reads remain queued. The corrected implementation
collects the full image in both cases without cancelling it. Existing failure,
short-frame adaptation and between-frame source-loss tests remain included.

This static finding explains a mechanism by which the old watcher could stop
acquisition; it does not prove that every historical stall had the same cause.
If signal loss produces neither completion nor an error, the host still waits
for the device, and cannot independently clear the display or reopen that active
session. Physical reboot qualification is required. A reliable passive observer
would require a separately established firmware path; substituting an arbitrary
ADC or memory request would still trigger the identified setup handler.

## Offline reproduction

Use [Dis51 0.5](https://plit.de/asem-51/dis51.html); its C source archive at
`https://plit.de/asem-51/dis51-0.5.tar.gz` has SHA256
`a6241f142324bb175ed94a663b2caec53e06943320ef6e127015746e13c9a4f6`.
Build its five C files with Ninja, `-std=gnu89 -Idis51-0.5`, and feed Intel HEX
records made from the reconstructed analysis buffer. Entry points are 0, 51, 67,
75, 83 and 99, plus every two-byte entry in 0x0a9c..0x0ab2 and 0x134f..0x135d.
Disassemble in listing mode (`-l`) to retain code addresses and raw instruction
bytes.
