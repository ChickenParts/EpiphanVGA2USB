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
