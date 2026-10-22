# Documentation

Start with the [usage guide](usage.md) to build and run the driver. The [protocol index](protocol/README.md) covers the USB protocol and implementation.

## Using the device

- [Building, firmware, capture, live viewing, and recovery](usage.md)
- [Firmware package, extraction, checksums, and licensing](../firmware/README.md)

## How it works

- [Protocol and implementation overview](protocol/README.md) — startup, timing, frame transfer, and unresolved fields
- [Original driver/kernel ABI audit](kernel-abi.md) — incompatible kernel structures and APIs

The individual protocol reports are linked from the protocol index. They contain the actual offsets, request layouts, timing tables, and decoding details. They're kept separate so it's possible to trace a claim back to the vendor object or a hardware observation.

## Hardware tests

- [Hardware test index](hardware/README.md) — verified, simulated, and still untested
- [First physical VGA capture](hardware/capture-2026-09-28.md), including the [screenshot](hardware/images/first-capture-2026-09-28.png)
- [Signal-loss and cold power-cycle recovery](hardware/reset-recovery-2026-09-29.md)

The rest of `hardware/` is the dated debugging record. Some earlier approaches were later replaced; the test index points out the superseding observations.

## Historical design notes and raw evidence

- [Initial design](design.md)
- [Implementation plan and checklists](plan.md)
- [Kernel structure layout evidence](evidence/kernel-7.2.5/)

The design and plan describe how the project got here, not necessarily how the current code works. Check the usage guide and protocol index for current behavior.

The reports identify which results come from the vendor binary, offline tests, or physical hardware. See the [hardware test record](hardware/README.md) for tested modes and remaining gaps.
