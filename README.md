# EpiphanVGA2USB

Native Linux support and protocol documentation for the original Epiphan VGA2USB
USB VGA capture device, initially `5555:1110`.

ChickenParts uses this device to observe physical PC boot and retain screenshots.
The intended driver runs in userspace through libusb: loading an obsolete kernel
module or restarting the host is unnecessary.

## Current status

Physical 720×400 text capture and a full-screen live mirror are verified on the test host
without a host reboot. The source is currently at a Phoenix BIOS POST prompt.

Implemented: hash-pinned firmware extraction, RAM-only bootstrap, sync-status
query, original RGB565 HOME decoder, row-aligned asynchronous USB collection,
and evidence-backed short-frame recovery. Offline checks include failure paths
and concatenated PPM output. The original model differs from VGA2USB LR; their
firmware and setup requests are not interchangeable.

See [the design](docs/design.md), [the implementation plan](docs/plan.md),
[kernel compatibility evidence](docs/kernel-abi.md),
[firmware provenance](firmware/README.md), and
[physical capture evidence](docs/hardware/capture-2026-09-28.md).

## Build and checks

Ruby with minitest and zlib, a C compiler, pkg-config, libusb development files,
binutils, and Ninja are needed. First obtain the official vendor object as described
in [firmware instructions](firmware/README.md), then run:

```sh
ninja test driver firmware-audit
./build/vga2usb-native --inspect
```

`--bootstrap` uploads only the generated original-model RAM image. `--sync` reads
the documented 10-byte status response after startup. Both need USB read/write
access. `--port BUS-PORT[.PORT]` restricts operations to one physical USB path.
The capture commands below produce screenshots or a continuous stream.

Vendor firmware remains vendor-owned. Extraction tools and manifests will make
it reproducible from an identified official package; generated binary data lives
under the ignored build directory.

## Capture and live viewing

The native capture path queues the first bulk read before B4, then collects the
remaining row-aligned requests and unpacks HOME RGB565 row order. Short frames
are discarded; after three consecutive incomplete frames the phase count advances
to the next divisor of the source height. Successful tuning persists in a stream.

```sh
sudo ./build/vga2usb-native --capture snapshot.ppm --port 1-12.2
sudo ./build/vga2usb-native --stream --port 1-12.2 | ffplay -fs -f image2pipe -vcodec ppm -framerate 14 -vf setdar=4/3 -i -
```

Auto mode currently accepts only the uniquely identified 720×400 text timing with
450 total lines and refresh 69–71 Hz. `--mode 640x480` explicitly selects the
640×480@59.94 preset, validating 525 lines and refresh within 1 Hz of the preset.
Horizontal resolution is ambiguous from the status counters alone for some modes,
so generic 640×480 auto-selection is deliberately not implemented. The program
rejects absent/unsupported timing and short or unexpected frame lengths.

[Capture setup](docs/protocol/frame-setup.md) records the original ADC-specific
defaults, the B0 skip, and the complete default B4 templates.
[Mode parameters](docs/protocol/mode-parameters.md) explains the vendor's VESA
matching and the unresolved or format-dependent cases.

Only one process may claim the capture interface. Stop the viewer before taking
an independent native snapshot; desktop screenshots can be taken while viewing.
PNG conversion uses `ffmpeg -i snapshot.ppm -frames:v 1 snapshot.png`.

[Receive transport](docs/protocol/receive-transport.md) and
[HOME recovery](docs/protocol/home-recovery.md) record the recovered USB scheduling
and phase-search evidence. Only 720×400 is physically qualified so far.
For now, reconnect recovery is manual: stop the viewer, reconnect the USB device,
inspect it, run `--bootstrap` if needed, and start the viewer again. This sequence
has not yet been exercised as a hardware recovery test.
