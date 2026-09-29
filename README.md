# EpiphanVGA2USB

Native Linux support and protocol documentation for the original Epiphan VGA2USB
USB VGA capture device, initially `5555:1110`.

ChickenParts uses this device to observe physical PC boot and retain screenshots.
The intended driver runs in userspace through libusb: loading an obsolete kernel
module or restarting the host is unnecessary.

## Current status

Physical 720×400 text capture and a full-screen live mirror are verified on the test host
without a host reboot. Signal loss and automatic return on a test-PC cold power
cycle are verified with the same native capture and viewer processes. Graphics
mode transitions still need physical qualification. [Recovery evidence](docs/hardware/reset-recovery-2026-09-29.md).

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
binutils, FFmpeg (for viewer checks), and Ninja are needed. First obtain the official vendor object as described
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
./tools/view-live.sh --port 1-12.2
```

Auto mode uses the vendor's timing score and table order for 36 progressive timing
records up to 1024×768, including 640×480, 720×400 and 800×600. Higher pixel
clocks use two or four horizontal capture partitions. `--mode 640x480` and
`--mode 720x400` pin the original presets. B1 supplies vertical timing, so some
horizontal resolutions remain ambiguous; explicit presets help when the source
is known. Interlaced modes are not supported by this path.

A one-shot snapshot keeps the source dimensions and rejects absent or unsupported
timing. A continuous stream emits complete 720×480 RGB PPM canvases, including its
black signal-loss frame, so the viewer's packet size remains stable on mode changes.
Short frames are discarded and adaptive phase tuning is retained.

Timing measurement B1 interrupts the original firmware's capture engine. It runs
before starting a frame or after all image reads have completed or been cancelled
and drained. If reads remain unfinished at a scheduled reacquisition tick, the host
drains them before measuring again. The interval allows four requested phase
schedules, with a minimum of one second. USB control and bulk timeout arguments
remain zero; the viewer has no runtime limit. A healthy completed frame wins when
its callback and the tick arrive together.

Unavailable or changed timing clears the display and reopens the USB capture
session while the viewer's stdout pipe stays open. An unchanged timing measurement
retries acquisition; repeated scheduled interruptions reopen the session instead
of exhausting phase tuning. Initial startup without VGA waits for supported timing.
Negative USB errors remain fatal; physical USB unplug recovery is separate from
VGA reset recovery. `ninja test-session test-transport test-stream-frame test-viewer`
checks session cleanup, stalled reads, explicit presets, mode changes, rendering
and real FFmpeg decoding. Hardware qualification is recorded separately below.

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

The viewer reads each complete streaming PPM canvas as one 1036815-byte packet.
This avoids header parsing across arbitrary 4096-byte chunks and source-size changes.
`ninja test-viewer` feeds the actual renderer across four source geometries through
real FFmpeg and checks exact returned-frame pixels and the black clear frame.
It does not exercise the graphical player or impose a program deadline.

The viewer uses [arrival timestamps](https://ffmpeg.org/ffmpeg-formats.html#Format-Options)
and [bounded input buffering](https://ffmpeg.org/ffplay.html#Advanced-options),
so variable capture speed does not rely on a guessed playback rate. `-framerate 60`
sets timestamp precision; it does not claim 60 captured frames per second. The test host
currently delivers about 8.8 complete frames per second at five capture phases.
Every 64 completed stream frames, stderr records frame count and monotonic time.

The [firmware trace](docs/protocol/firmware-control-interruption.md) records why
status queries and active frame reads must be separated. A later
[viewer stall](docs/hardware/viewer-stall-2026-09-29.md) also blocked the
producer's output pipe. Complete frame packets resolve the reproduced PPM parser
errors; the recovery changes address unfinished reads and geometry changes.
A [cold power-cycle observation](docs/hardware/reset-recovery-2026-09-29.md)
verified signal-loss clearing and automatic return without restarting either
process. Graphics/text transitions and repeated reset endurance remain pending.
