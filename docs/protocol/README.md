# Original VGA2USB protocol notes

These notes cover the original Epiphan VGA2USB (`5555:1110`), **not** the VGA2USB LR. The starting point for the reverse engineering was the symbolized vendor Linux object from Epiphan 3.33.0.17:

```text
vga2usb_bins.o_shipped
SHA256 536b9be25b310d57a77d2888fe83cd2e499417dbb963129edc90357cf9736dbb
```

Addresses in the reverse-engineering reports are ELF section offsets unless a report explicitly says otherwise. They're not addresses in a loaded kernel module. Firmware 8051 addresses are a separate address space.

This page describes how the implemented path fits together. The linked reports contain the packet layouts, measurements, disassembly references, and remaining uncertainties.

## Startup

The native program first matches USB `5555:1110` and, optionally, a physical port path. When necessary, it reconstructs the *original model's* firmware from the hash-pinned vendor object and uploads it to volatile device RAM.

The upload uses Cypress vendor OUT request `0xa0`: hold the device CPU at CPUCS `0xe600`, transfer the firmware segments in order with 64-byte chunks, then release it. The recovered image contains 65 segments and 7736 decoded bytes. The expected initialized firmware version is `0102`.

See [firmware provenance and extraction](../../firmware/README.md). No proprietary executable code is loaded into the host kernel, and no EEPROM write is involved.

## Signal detection and mode selection

The ten-byte `B1` control response reports sync timing. The implementation uses the recovered timing scoring and 36 progressive VESA table records through 1024×768, with explicit `720x400` and `640x480` presets available.

On the original model the analog front end is the AD9884A, with its own register bridge and setup defaults. The common `B0` serializer appears in the vendor driver but is **skipped** for this model; the native driver doesn't send it.

See [ADC control](adc.md), [mode scoring and parameters](mode-parameters.md), and [the B0/B4 setup trace](frame-setup.md).

## Frame capture

The original device uses bulk IN endpoint `0x82` (512-byte packets). The native path queues the first asynchronous read before the `B4` capture command, then receives the other row-aligned reads and decodes the original HOME RGB565 layout.

Short frames aren't treated as valid images. After three consecutive incomplete frames, the capture path advances to the next vertical phase divisor. The first physical 720×400 capture needed five phases.

See [frame protocol](frames.md), [receive transport](receive-transport.md), and [HOME short-frame recovery](home-recovery.md).

## Keeping a live stream alive

There's an important trap here: the firmware's sync/control handling can interrupt an active capture. The live path therefore finishes or cancels and drains outstanding image reads **before** issuing a timing request. On a lost or changed VGA signal it clears the viewer and can reopen its USB capture session while keeping the viewer pipe open.

The stream outputs a constant 720×480 RGB24 PPM canvas for decoding, regardless of the source's capture dimensions. This is a viewer transport choice, not a property of the VGA source.

See [firmware control interruption](firmware-control-interruption.md) and [signal-loss hardware verification](../hardware/reset-recovery-2026-09-29.md).

## Reports

| File | What it covers |
| --- | --- |
| [adc.md](adc.md) | Device binding, AD9884A register bridge, and sync observations |
| [frames.md](frames.md) | Original B4 command, capture endpoint, HOME frame layout |
| [frame-setup.md](frame-setup.md) | Startup defaults, B0 skip, complete B4 setup templates |
| [mode-parameters.md](mode-parameters.md) | VESA matching, timing scores, partitions, and clock thresholds |
| [receive-transport.md](receive-transport.md) | Async USB request order and frame completion |
| [home-recovery.md](home-recovery.md) | Vertical phase selection after incomplete frames |
| [firmware-control-interruption.md](firmware-control-interruption.md) | Why sync/control requests must not overlap frame capture |

These reports were written during investigation. Some describe a proposed next change that has since been implemented; use this overview and the [current usage guide](../usage.md) for present behavior.

## Evidence and limits

**Recovered from the vendor object:** model dispatch, firmware tables, request assembly, ADC defaults, mode-scoring logic, frame unpacking, and the HOME retry rules.

**Checked with offline tests:** decompression, bootstrap transfer failures, B4 layouts, frame assembly, mode transitions, and viewer decoding.

**Physically checked:** original-model startup and 720×400 text capture; signal-loss clearing and resumption after a cold power cycle of the source PC.

Other graphics modes and physical USB disconnect/reconnect remain unqualified. See [hardware tests](../hardware/README.md). Nothing here should be read as permission to send unverified commands to the device.
