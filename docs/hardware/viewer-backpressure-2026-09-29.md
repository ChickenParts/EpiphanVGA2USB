# Live-viewer backpressure

A tester reported that commands were not appearing on the live mirror. At 20:18:43 UTC,
the native producer PID 1457285 was blocked in `anon_pipe_write`, not a USB read.
Its frame counter stopped at 13312, monotonic 986081.780 (20:17:24 UTC). ffplay
PID 1457283 remained alive with its main thread sleeping and decoder workers
waiting.

This establishes viewer backpressure. It does not independently establish
which player state caused it. FFplay's interactive keyboard policy is a
plausible trigger: P/Space pause, S steps and arrows seek. Those bindings are
unsuitable for a live monitor that may receive command keystrokes. See the
[upstream event loop](https://github.com/FFmpeg/FFmpeg/blob/master/fftools/ffplay.c).

After restarting only the owned capture/viewer pipeline, newly captured pixels
showed the tester's previously typed characters. The PC had
continued accepting keyboard input while the monitor appeared frozen. No USB
reset, firmware write or PC restart was performed for this recovery. Producer
frame progress resumed.

`tools/view-live.sh` now scopes a small SDL2 input-policy preload to ffplay. It
filters keyboard and mouse playback inputs while preserving window expose,
resize and close events. The USB producer is not preloaded and its capture
protocol is unchanged. Build with `ninja driver` or `ninja viewer-input`; the
viewer refuses to start without the policy library. Close through the window
manager instead of ffplay playback shortcuts.

`ninja test-viewer-input` injects pause/step/seek-class keyboard and mouse events
through the actual SDL runtime, asserts their rejection, and checks that expose
and quit events still reach the queue. The test failed before the policy, then
passed. It uses SDL's dummy video backend and no runtime deadline. The existing
599-frame decoder test still verifies exact pixels, signal-loss clearing and
return across four geometries. This prevents interactive pause/seek; it does
not claim to fix every possible ffplay or USB stall.
