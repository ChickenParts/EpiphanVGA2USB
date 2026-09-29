# Original VGA2USB AD9884A control and mode evidence

Scope: original Epiphan `5555:1110`, vendor module version `3.33.0.17`, object SHA-256 `536b9be25b310d57a77d2888fe83cd2e499417dbb963129edc90357cf9736dbb`. Addresses below are section-relative ELF addresses, not runtime kernel addresses. This was an offline investigation only: no USB requests, firmware writes, module loading, build, or target changes were performed. `docs/kernel-abi.md` is preserved.

## Original-model binding

The `usb_board_vga2usb` model at `.rodata+0x11b240` selects `usb_io_create_vga2usb` at model offset `0x30` and the one-entry grabber table `.rodata+0x11d1c0` at offset `0x40`.

That grabber table has:

| Field | Evidence |
| --- | --- |
| image endpoint | byte at `0x11d1c8` is `0x82` |
| I/O parameter block size | little-endian u32 at `0x11d1d4` is `0x11` = 17 bytes |
| interlace/capture partition helper | relocation at `0x11d1f8` is `AD9884A_hinterlaces` |
| capture protocol | relocation at `0x11d200` is `grab_protocol_home` |
| ADC constructor | relocation at `0x11d218` is `adc_AD9884A_create` |
| ADC I2C address argument | byte at `0x11d220` is `0x4c` |

`usb_board_init` at `0x1a8e..0x1aad` obtains the parameter-block size from table+0x14, address from table+0x60, and calls `usb_adc_io_create`; it then invokes the constructor from table+0x58. The address is passed as-is; no shift or read/write bit is added by the host bridge. Treat `0x4c` as the exact wire `wValue` value, without guessing whether firmware internally represents a 7-bit or 8-bit I2C address.

## Exact ADC register bridge

`adc_io_read` (`0xf4c0`) calls `i2c_read(board, adc_address, register, buffer, length)`. `adc_io_setreg` (`0xf510`) calls `i2c_write` with a single data byte. `i2c_read` and `i2c_write` (`0x1b960`, `0x1b980`) select direction and dispatch to grab I/O ops+0x68.

The original model's `usb_io_create_vga2usb` (`0x4d10`) selects the operations table at `.rodata+0x1a2840`. Its slot+0x68 has a relocation to `.text+0x3160`, the exact USB bridge implementation.

| Setup field | Read | Write |
| --- | --- | --- |
| endpoint | control endpoint 0 | control endpoint 0 |
| bmRequestType | `0xc0` | `0x40` |
| bRequest | `0xb7` | `0xb7` |
| wValue | `0x004c` | `0x004c` |
| wIndex | `(chunk_length << 8) | (register & 0xff)` | same |
| wLength | chunk length, 1..64 | chunk length, 1..64 |
| data | raw register bytes returned by firmware | raw register bytes supplied by caller |

Evidence: `0x3165..0x3171` calculates the direction; `0x31b3..0x31c3` limits each chunk to 64; `0x31cc` obtains the low register byte; `0x31d0` passes the ADC address as wValue; `0x31d3` sets request `0xb7`; `0x31de..0x31e7` places length in the high wIndex byte. For longer operations the next register increases by the transferred count, the buffer advances, and the low register byte wraps modulo 256. A short transfer causes `-EPIPE` (`-32`) rather than silent success. No automatic register retry is present.

`os_bususb_control_msg` (`0x43170`) and wrapper source `drvwrapper.c:45` establish argument order: request type, request, value, index, buffer, length. This matters because request and type are reversed relative to some commonly used USB APIs. The vendor supplies a 500 ms per-transfer deadline; that is evidence about the old driver, not a proposed process timeout or an instruction to execute a probe.

There is no evidence-backed specific register write to recommend for original-model startup. A write-form example is therefore deliberately not supplied. The bridge format is mapped so a later firmware/register audit can produce an exact justified sequence.

## Host-side ADC startup

`adc_AD9884A_create` (`0x11480`) calls `adc_default_create` using ADC ops at `.rodata+0x1a3620`. The table contains a mode callback at ops+0x18 (`.text+0x11160`) and a 140 MHz numeric limit at ops+0x0c. Its direct register-image length at ops+0x10 is zero; its register-image callback at ops+0x30 is null. Other callbacks in this table, including any model-specific explicit init callback, are null.

`adc_default_create` initializes the ADC instance's calibration bytes to zero. `adc_init` (`0xf320`) checks the ADC I/O table's init slot+0x10; in the USB ADC I/O table `.rodata+0x17cca0` that slot is null. Its optional calibration read is skipped because the ADC calibration address at instance+0x24 is zero. Thus the identified original-model host ADC initialization path itself issues no I2C register writes and no calibration request.

This does **not** prove that an uninitialized device can capture. `usb_board_init` invokes `usb_board_autofirmware` before constructing the grabber. Original `5555:1110` takes the special branch at `0x1681..0x1694`, bypassing the generic `board_init_grabber_hw` call at `0x1b87`. These facts place the necessary hardware initialization outside a conventional host AD9884A register-init callback. The likely locations are device firmware startup and the later firmware-owned parameter command. Hardware startup and the original B0 skip are now separately established; firmware inspection is still required to name the exact chip register initialization; copying initialization from AD9887A or another chip would be speculation.

`adc_set_ioparams` (`0xeb10`) has no direct AD9884A register image to generate, but still invokes USB I/O ops+0x28. That slot points to `.text+0x670`. Its first conditional checks USB I/O table byte +0x0e bit 0; the original model sets that bit, so the function returns without sending B0. The generic serializer otherwise produces a 39-byte payload, but it is not part of original 5555:1110 startup. See [completed setup evidence](frame-setup.md). Its bytes depend on the current computed capture parameters. This is a firmware parameter command, not evidence of a host B7 startup register list. Do not send a fabricated zero-filled B0 payload.

## Minimum evidenced sync/mode observation

AD9884A mode callback `.text+0x11160` invokes USB ADC I/O ops+0x20 with a requested length of 10. That slot points to `.text+0x630`:

```
bmRequestType = 0xc0
bRequest      = 0xb1
wValue        = 0x0000
wIndex        = 0x0000
wLength       = 10
USB setup bytes, little-endian fields: c0 b1 00 00 00 00 0a 00
```

There is no preceding B7 ADC read or write in this mode observation. `.text+0x630` accepts exactly the requested byte count and converts short results to `-EPIPE`. The mode callback propagates negative transport errors.

The 10-byte result is interpreted as follows:

| Byte offsets | Proven use |
| --- | --- |
| 0..1 | BE16 period counter, byte-swapped at `0x111da`; zero means no signal |
| 2..3 | BE16 line-count measurement, byte-swapped at `0x111de`; passed to VESA matching |
| 4..5 | BE16 measurement, byte-swapped at `0x111d0`; retained in diagnostic measurement block; specific meaning not established here |
| 6..7 | BE16 measurement, byte-swapped at `0x111d5`; retained likewise; specific meaning not established here |
| 8 | low two bits retained in capture parameters at `0x112ab..0x112bd`; exact named bit meanings not established here |
| 9 | retained with byte 8 in diagnostic measurement block; no distinct semantic use identified in this callback |

For nonzero first counter `P`, the callback stores `floor(2_000_000_000 / P)` at grab-parameters+0x37 and passes `floor(20_000_000 / P)` to `VESA_find_match` together with the second measurement. The VESA scoring function at `.text+0x253c0` compares its refresh quantity with the table's millihertz refresh divided by 100 (`0x253c5..0x253f6`) and compares the other quantity against total vertical lines (`0x253ce..0x2542a`). This establishes the quantities as refresh millihertz and refresh tenths of Hz respectively, and the second measurement as vertical line count. An inferred timer rate of 2 MHz is consistent with this exact scaling; firmware examination should confirm the counter source.

Examples here are synthetic offline arithmetic checks, **not observed hardware values**: `P=33333` and line count 525 gives 60000 millihertz and 600 tenths of Hz, consistent with a 60 Hz raster containing 525 total lines.

If `P==0`, the driver clears capture parameters and returns `-123` at `0x113a0..0x113ad`. If no VESA candidate matches, it clears them and returns `-124` at `0x113b2..0x113bf`. A valid candidate goes through `VESA_compute_VGA_parameters`; capture geometry comes from a VESA table match, not directly from the two unknown returned counters. Existing geometry and refresh are compared to determine whether a change occurred. Pixel-frequency-dependent configuration bytes are computed afterward. A generic B1 decoder therefore cannot honestly claim final active resolution without porting that VESA logic or comparing to a precisely known test pattern.

`AD9884A_hinterlaces` is a pure arithmetic helper: it calculates capture partition/interlace counts from geometry and estimated pixel clock, without issuing USB traffic. Its name is not evidence of an ADC register operation.

## Proposed executable sequence, without speculative requests

Precondition: firmware and device state have already been established by the separate original-model firmware investigation. The **minimal mode-observation sequence** justified by this host object is one IN request `c0:b1`, value=0, index=0, length=10, then exact-length validation and decoding above. Do not add an ADC reset, generic chip-init write list, B0 placeholder, or boot-firmware write to this sequence.

`adc-request-plan.rb` is runnable offline and contains no USB or network access. It emits the exact proposed setup packet and decodes an externally supplied saved 10-byte response:

```sh
ruby tools/adc-request-plan.rb mode-plan
ruby tools/adc-request-plan.rb decode-mode 8235020d000000000300
```

The second command uses the synthetic example above. The program also supports `i2c-read-plan REGISTER LENGTH` for an explicitly chosen register; this is an offline bridge encoder, not a recommendation to read a particular register or to send the output to hardware. No register defaults are selected, and no write plan is implemented.

For a later transport implementation, the request call is structurally:

```c
/* Only after firmware state and hardware traffic are separately authorized. */
unsigned char measurement[10];
int count = libusb_control_transfer(handle, 0xc0, 0xb1, 0, 0,
                                    measurement, sizeof measurement, 0);
/* Final zero supplies no USB-transfer timeout. Reject count != 10. */
```

This fragment was neither built nor run. The final zero preserves the user's no-timeout preference; the old driver's 500 ms value is not carried into the proposed implementation.

## Remaining bounded uncertainties

- Exact original firmware ADC reset and startup register sequence: not recoverable from the host ADC callbacks mapped here, and must be extracted from the original firmware.
- Exact semantic names of B1 bytes 4..9: preserve raw values; do not invent polarity/width/status labels beyond their proven uses.
- Exact 39-byte B0 payload for a chosen mode: requires the full capture-parameter structure and VESA parameter-generation port; zero-filled payloads are not justified.
- Hardware validation: no command in this document has been sent to the connected unit.

Evidence artifacts: `blob-disassembly.txt`, `blob-rodata-relocations.txt`, `blob-rodata.bin`, `show-range.rb`, and `adc-request-plan.rb`, all in the same scratch directory. These artifacts include vendor-derived binary material and remain outside the new repository pending its distribution policy.
