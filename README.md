# EpiphanVGA2USB

Native Linux support and protocol documentation for the original Epiphan VGA2USB
USB VGA capture device, initially `5555:1110`.

ChickenParts uses this device to observe physical PC boot and retain screenshots.
The intended driver runs in userspace through libusb: loading an obsolete kernel
module or restarting the host is unnecessary.

## Current status

Reverse engineering and capture bringup are in progress. Live capture is **not yet hardware-qualified**.
The original model differs from VGA2USB LR; LR firmware and setup requests must
not be sent to it. The vendor's modern Linux wrapper still contains an old binary
core with incompatible kernel structure accesses.

Implemented: hash-pinned firmware extraction, descriptor inspection, RAM-only
bootstrap, and an evidence-backed sync-status query. Offline decoder and
bootstrap transfer/failure-path checks pass. RAM startup and a no-signal status read
are verified on the test host; frame capture is still in progress.

See [the design](docs/design.md), [the implementation plan](docs/plan.md),
[kernel compatibility evidence](docs/kernel-abi.md), and
[firmware provenance](firmware/README.md), [frame-format notes](docs/protocol/frames.md),
and [hardware startup evidence](docs/hardware/startup-2026-09-28.md).

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
These are bringup commands; they do not yet capture or display video.

Vendor firmware remains vendor-owned. Extraction tools and manifests will make
it reproducible from an identified official package; generated binary data lives
under the ignored build directory.

## Capture prototype

The native capture path queues bulk IN before the documented B4 request and
unpacks the original HOME RGB565 row order. These commands are implemented and
have offline request/decoder checks; real-frame qualification is pending:

```sh
sudo ./build/vga2usb-native --capture snapshot.ppm --port 1-12.2
sudo ./build/vga2usb-native --stream --port 1-12.2 | ffplay -f image2pipe -vcodec ppm -i -
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
