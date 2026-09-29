# EpiphanVGA2USB

Native Linux support and protocol documentation for the original Epiphan VGA2USB
USB VGA capture device, initially `5555:1110`.

ChickenParts uses this device to observe physical PC boot and retain screenshots.
The intended driver runs in userspace through libusb: loading an obsolete kernel
module or restarting the host is unnecessary.

## Current status

Reverse engineering and bringup are in progress. Live capture is **not ready**.
The original model differs from VGA2USB LR; LR firmware and setup requests must
not be sent to it. The vendor's modern Linux wrapper still contains an old binary
core with incompatible kernel structure accesses.

Implemented: hash-pinned firmware extraction, descriptor inspection, RAM-only
bootstrap, and an evidence-backed sync-status query. Offline decoder and
bootstrap transfer/failure-path checks pass; physical startup is not yet verified.

See [the design](docs/design.md), [the implementation plan](docs/plan.md),
[kernel compatibility evidence](docs/kernel-abi.md), and
[firmware provenance](firmware/README.md).

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
