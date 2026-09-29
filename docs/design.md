# Original VGA2USB native capture design — 2026-09-28

## Accepted scope

The user selected porting or writing a native driver on the test host, requested operation
without restarting the host, and created ChickenParts/EpiphanVGA2USB for lasting
driver, firmware, and reverse-engineering documentation. No virtual machine or
host kernel downgrade is part of this design.

## Architecture

Use a fresh userspace libusb transport for the original `5555:1110` model. Use the
vendor's symbolized object as a protocol reference and firmware data source, never
as executable kernel code. The existing LR-only userspace implementation is useful
prior art but is not a model-compatible starting image.

Keep three boundaries explicit: firmware extraction and validation; USB startup,
ADC configuration and frame transport; and viewing/screenshot storage. A standard
frame stream goes to an existing viewer, initially ffplay, on the test host's KDE desktop.
Screenshots retain the captured pixels and timestamp; device status and missing
signal must be distinguishable from captured video.

## Startup and recovery

Match vendor/product and physical device identity before any proprietary request.
Extract only the original model's identified firmware table and validate every
segment length, compression result and load address. Preserve the table order and
the vendor's 64-byte RAM transfer splitting. The startup upload uses the Cypress
RAM request, not an EEPROM update. Reenumeration may affect this USB device but
does not require a host reboot.

Do not resume the MCU following an incomplete firmware upload. Report the exact
failure and recover by reconnecting the capture device. No guessed ADC commands,
forced kernel symbol compatibility, or exception-handler stubs are acceptable.

## Validation

Offline tests cover compressed firmware decoding, malformed inputs, complete image
provenance, packet layout, transfer failures and model rejection. Real-device checks
separately establish startup, signal detection, one complete frame, live viewing,
screenshot fidelity, and reconnect behavior. A compiler success or firmware upload
does not prove capture works.

Initial hardware modes include PC text VGA 720×400 and graphics VGA 640×480.
Additional modes follow measured evidence. Exact transport and ADC fields are
documented before use, with unknown fields marked as unresolved rather than assigned
speculative meaning.

## Alternatives considered

A wrapper-only kernel port is rejected because the old binary directly accesses
obsolete USB, URB, task/mm and V4L2 layouts. A complete new kernel driver remains a
possible later V4L2 integration, but adds kernel failure risk without helping initial
capture. Userspace libusb provides hot startup and keeps the implementation portable
across Linux kernel updates.
