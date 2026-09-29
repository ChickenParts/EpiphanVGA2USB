# Original VGA2USB Implementation Plan

**Goal:** Native original-model capture, live viewing and durable screenshots on
The test host, without restarting the host.

**Architecture:** A userspace libusb driver consumes validated vendor firmware and
implements a documented model-specific ADC/frame protocol. Existing display tools
consume captured frames. Ruby provides reproducible extraction and offline checks.

## Phase 1: Provenance and startup tools

- [x] Add failing Ruby tests for firmware RLE, incomplete zlib streams, incorrect
  lengths, unknown compression, and complete pinned vendor table extraction.
- [x] Implement `tools/vendor_firmware.rb` and `tools/extract_firmware.rb` with
  bounded ELF section/symbol/relocation decoding and a pinned source SHA256.
- [x] Run `ninja test` and `ninja firmware-audit` and record actual segment/image
  hashes, 65 segments, 7736 decoded bytes, and the original model's version.
- [x] Implement a libusb device inspection command and validated RAM-only startup
  transport; test wrong models, short writes and incomplete uploads before hardware.
- [x] Inspect then start the actual capture device, recording descriptor/firmware
  evidence. Do not claim video readiness from this check.

## Phase 2: Document and implement capture

- [x] Trace `adc_AD9884A_create`, register transport and sync/mode detection; document
  requests in `docs/protocol/adc.md` with source addresses and hardware status.
- [x] Trace `grab_protocol_home`, the original USB grab-command function and bulk
  reader; document command fields and pixel/row representation in
  `docs/protocol/frames.md`.
- [x] Add offline packet and synthetic-frame tests; implement only established
  fields, including bounds and exact transfer-length checks.
- [x] Verify no-signal handling and physical PC text frames; inspect size, colors
  and orientation. Graphics modes remain a separate hardware check.

## Phase 3: Repeatable ChickenParts use

- [x] Show live video on the test host's monitor and verify the displayed capture source.
- [ ] Exercise disconnect/reconnect and document the recovery commands.
- [x] Publish source and verified RE documentation to the user's GitHub repository.

Each phase reports its completed checks and remaining limits. The capture phases
depend on recovered protocol evidence and actual source video; they are not implied
complete by offline tests.

## Restart regression: observed 2026-09-28

Source-PC restart interrupted a frame after 612768/614304 bytes. The native
reader was still waiting for USB data after the source regained its original
720×400 timing. Restarting the owned capture process restored a complete image.

Repair within the accepted recovery design: monitor documented B1 status while
bulk reads are pending. A periodic Linux timer wakes the USB event loop, which
otherwise polls indefinitely. Cancel and drain reads only on observed lost or
changed VGA timing, never because a transfer has exceeded an elapsed deadline.
Continuous mode clears its display and waits for supported source timing to
return; fatal USB errors still stop it. Test loss during a pending frame, valid
signal returning, continued output boundaries, and unchanged one-shot rejection.
Then verify on the test host and record the actual limits of the hardware test.
