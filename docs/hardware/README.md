# Hardware test record

What has been tried on a real original Epiphan VGA2USB (`5555:1110`), and what hasn't.

The test device was connected at USB path `1-12.2` to a host running Linux `7.2.5-1-default`. The verified source was a 16 MHz 80386SX PC displaying its Phoenix BIOS in 720×400 text mode. The device was initialized with firmware version `0102`, uploaded through the native userspace tool.

## Verified on physical hardware

| Test | Result | Notes |
| --- | --- | --- |
| Find and initialize device | Passed | [Startup, 2026-09-28](startup-2026-09-28.md): RAM firmware upload, descriptor and sync checks, no host reboot |
| 720×400 capture | Passed | [First capture, 2026-09-28](capture-2026-09-28.md): complete RGB565 frame decoded to PPM and inspected |
| Full-screen live mirror | Passed | [First capture](capture-2026-09-28.md): running through ffplay on the host |
| Incomplete-frame recovery | Observed | [First capture](capture-2026-09-28.md): HOME phase search reached a complete frame at five phases |
| VGA loss and return | Passed for one cold power cycle | [Recovery, 2026-09-29](reset-recovery-2026-09-29.md): same capture and viewer processes resumed, with a black clear frame on loss |

The first successful frame is here:

![Phoenix BIOS captured from a 16 MHz 80386SX](images/first-capture-2026-09-28.png)

## ChickenDOS splash observations

The [ChickenDOS resident-shell hardware log](https://github.com/ChickenParts/ChickenDOS/blob/metachicken/docs/testing/command-resident-2026-09-29/README.md) records Epiphan screenshots of the memory diagnostics on the same 386SX-16. After the DOS memory allocation fix, the user reported that `SPLASH.EXE` ran successfully and then repeated the launches. A subsequent Epiphan screenshot showed the memory layout remained unchanged.

ChickenDOS has two splash paths: CGA-compatible BIOS mode `04h` (320×200, four colors) and VGA mode `13h` (320×200, 256 colors). The splash [selects the mode at runtime](https://github.com/ChickenParts/ChickenDOS/blob/metachicken/src/demo/splash.c). The hardware log does not record the selected mode or a captured splash framebuffer, so it is not yet evidence of a verified 320×200 capture through this driver.

## Not yet physically qualified

- A dimension-verified 320×200 ChickenDOS splash frame (CGA-compatible or VGA mode)
- 640×480 graphics capture, or other progressive table modes
- Text/graphics geometry changes on a real source
- Repeated warm-reset or power-cycle endurance
- Unplugging and reconnecting the **USB capture device** during a running session
- Interlaced capture (not implemented)

The physical recovery test involved a cold power cycle of the source PC; warm-reset recovery has not been independently verified.

## Debugging notes

These are dated observations, not separate user manuals. Where later tests superseded an approach, the original account is kept for traceability.

- [Startup — 2026-09-28](startup-2026-09-28.md): firmware transfer and early sync response
- [First capture — 2026-09-28](capture-2026-09-28.md): 720×400 frame, pixel inspection, and first live mirror
- [Source restart — 2026-09-28](restart-2026-09-28.md): an earlier pending-read watcher; later withdrawn after the [firmware control interruption finding](../protocol/firmware-control-interruption.md)
- [Viewer stall — 2026-09-29](viewer-stall-2026-09-29.md): pipe backpressure, packet framing, and incomplete reads
- [Signal-loss recovery — 2026-09-29](reset-recovery-2026-09-29.md): clearing and resuming a live mirror after source power loss
- [Viewer backpressure — 2026-09-29](viewer-backpressure-2026-09-29.md): input shortcuts and the ffplay interaction policy

## Offline checks

Regression tests cover simulated 720→signal-loss→640→800→1024→720 transitions, short reads, USB session cleanup, PPM packet boundaries, and decoding through FFmpeg. The simulated modes have not all been tested on the capture device.

For commands and current limitations, see the [usage guide](../usage.md). For the USB transfer and timing details, see the [protocol index](../protocol/README.md).
