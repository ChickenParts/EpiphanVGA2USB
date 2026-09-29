# Viewer backpressure, 2026-09-29

A tester reported another capture outage.
The original VGA2USB was still enumerated as 5555:1110, initialized firmware 0102,
device 24 at USB path 1-12.2. Kernel logs showed no capture-device disconnect.
Both the producer and ffplay processes were alive; the desktop mirror was black.

The previous log recorded complete frames through 4864, followed by two observed
VGA interruptions, phase recovery, and no further completed-frame diagnostics.
At 05:11 UTC, `/proc/1435615/wchan` was `anon_pipe_write`. A GDB backtrace confirmed
the main producer thread was blocked inside `fwrite` in `ppm`, called from the
successful decode/output path at `src/capture.c:209`. It was not waiting for a
pending bulk transfer at that point. The libusb worker was idle in poll.

The ffplay main thread was sleeping in `av_usleep`; its read thread was waiting
on an SDL condition, and its decoder threads were waiting on conditions. These
observations locate the immediate stall at output backpressure from the viewer.
They do not establish whether playback was paused, timestamp scheduling was
blocked, or the decoder/parser had stalled. No permanent viewer fix is claimed.

After saving both backtraces and the log, only this session's native producer and
viewer were stopped. The same pipeline was restarted without changing flags.
No USB reset, firmware upload, host reboot or kernel change was performed.
The resulting capture decoded 720x400, settled at five vertical phases and
advanced through frame counts 64, 128, 192, 256, 320 and 384. A desktop screenshot
showed the source BIOS's F1/F2 boot prompt again. The producer returned to its USB
poll loop rather than remaining blocked on the output pipe.

A subsequent ffplay `Invalid maxval: 0` warning occurred while delivered-frame
counts continued to advance. Its relationship to the stall remains unresolved.
The status watcher handles observed source changes while USB reads are pending;
it cannot progress while a synchronous output write is blocked by a viewer.

Next investigation should distinguish viewer pause/queue state, timestamp
scheduling, and PPM parsing using controlled playback of complete frames with
source interruptions. Do not infer a USB fault merely because the mirror freezes.

## Subsequent tester-correlated reboot

The tester then reported that reboots appeared to drop the signal. The new
pipeline had delivered complete frames through 1024 before recording `Waiting
for supported VGA signal`, `VGA signal returned`, and short-frame phase retries
through V=5. The mirror was black again. The device remained connected.

A separate GDB trace at approximately 05:16 UTC located this stall inside the
synchronous B1 `libusb_control_transfer` in `source_matches`, called from the
pending-read observation path in `capture_wire`. Thus this later stall was not
the output backpressure observed earlier. The observation poll itself has no
deadline, and its status query also has no USB deadline. An uncompleted control
query can therefore prevent the observation loop from progressing.

The tester's observation and the absence/return messages establish the reboot
trigger for this incident. They do not explain why the B1 control query stopped
completing. Investigate control/bulk interaction across reacquisition separately
from ffplay backpressure; do not conflate these two stalled paths.

## Complete image packets and recovery check

A finite synthetic stream of complete P6 images reproduced FFmpeg's `Invalid
maxval: 0` messages with the old viewer input flags. The stream consisted of
200 red images, one black clear image, and 399 green images. The headers and
pixel payloads were valid. The image2pipe demuxer normally reads 4096-byte
chunks when a parser is active; see upstream `libavformat/img2dec.c` in
`ff_img_read_packet`, the `frame_size` option, and `libavcodec/pnm_parser.c`.

The wrapper now specifies `-frame_size` equal to the complete P6 frame length:
864015 bytes for 720x400 and 921615 bytes for 640x480. Each supported native
stream uses fixed dimensions and a 15-byte P6 header, including its generated
black clear image. Arrival timestamps and external synchronization are retained.
There is no elapsed frame, USB or program deadline.

The regression drives the actual shell wrapper with a synthetic native producer
and substitutes real FFmpeg decoding for FFplay's graphical display. It failed
with the old flags on the parser diagnostics, then passed with complete frame
packets locally and on the test host. Both supported modes decode 599 frames after the
one initial discovery packet discarded by `nobuffer`. Exact Adler32 pixel checks
verify 199 red images, one black image and 399 green images. The regression is
included in the standard Ninja `test` target and needs FFmpeg installed.

The changed viewer was deployed on the test host. Its uninterrupted process log records
source loss and return after frame 128, another observed status change during
phase retries, then resumed delivery at frame 192 and continued through 1216.
There were no PPM parser diagnostics in that run. The tester explicitly
confirmed that the picture came back automatically. A subsequent screenshot
showed the changed source message `No boot device available` while frame counts
continued to advance, so the mirror no longer hid this physical boot failure.

This is a successful hardware recovery observation after the framing change.
It does not establish that PPM parsing was the sole cause of every previous
stall, or independently resolve the synchronous B1 control-query stall observed
earlier. USB disconnect/reconnect and unsupported modes remain unqualified.

## Final bulk read after a later reboot

The tester reported another failure after the complete-packet viewer change.
The old process had delivered frames through 2176, observed a source change,
then retried short frames through five vertical phases without completing another
frame. At approximately 05:32 UTC, GDB in `capture_wire` showed one pending read:
four 130560-byte chunks were complete, while the final 92064-byte read had not
completed. Status observations still matched 720x400 timing. This was a native
capture wait, not the earlier output-pipe backpressure. USB remained connected.

A trial change now returns `V2U_CAPTURE_REOPEN` after observing unavailable or
changed timing in an active streaming session. Pending reads are cancelled and
drained before capture storage is freed. Main then releases interface 0 and
closes the handle before opening and claiming a fresh handle to the same selected
device. The stdout pipe stays open, so the viewer remains alive. If the new
session also starts without signal it may emit another complete black image;
multiple clears across sessions are acceptable. No USB reset, firmware upload,
new device request or elapsed deadline is introduced. This follows the evidence
that manually restarting capture restored the picture; it does not establish
why the firmware stopped completing a read.

Regression checks failed before the production change, then passed locally and
on the test host. They cover source loss during pending reads and between complete
frames, cleanup before reopening, failed reopen, and one-shot capture remaining
strict. Selecting `--stream` after an earlier `--capture FILE` sends the stream
to stdout, avoiding reopening and truncating that file on every interruption.
The full Ninja test suite and native build passed on both machines.

The updated pipeline produced complete physical 720x400 frames on startup.
Hardware recovery after a new reboot remains pending. This change cannot recover
an uncompleted synchronous B1 query, or an interruption missed by the status
observer, and does not add discovery after a physical USB disconnect.

## Reopen trial failure and independent confirmation

A tester reported a stationary red fragment.
At approximately 06:02 UTC the native process had completed 201 frames in its
latest reopened session, then stalled with all five reads pending. Timing before
that frame was `6f 7d 01 c2 00 f4 0a c4 02 00`; the layout was 720x400/V5.
The log proves that several observed changes had invoked the new reopen path
and resumed capture before this later failure. Reopening alone was insufficient.

After preserving the stalled state, stopping that capture and taking an
independent one-shot image succeeded at V4. It showed a DOS prompt, so the
mirror had been stale while the PC continued running.

Firmware analysis then established that the pending-read B1 status observer
was destructive: the USB setup handler aborts GPIF before processing a new
request, and B1 measurement itself aborts GPIF too. See the
[firmware path and host correction](../protocol/firmware-control-interruption.md).
The host now measures between frames or after drained short reads, never during
active image collection. A delayed-completion regression failed before this
change and passed afterward; full Ninja tests/build passed locally and on the test host.
The corrected capture was deployed and resumed live frames.
Reboot recovery after this correction remains pending physical qualification.

