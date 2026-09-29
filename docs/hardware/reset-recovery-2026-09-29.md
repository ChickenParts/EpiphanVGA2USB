# Signal-loss recovery — 2026-09-29

Implementation commit `17f0888`, deployed to the test host. `ninja test driver` passed locally;
`ninja test` and `ninja driver` passed on the test host. No test host reboot, firmware
upload, USB device reset or guessed ADC write was performed.

The live pipeline uses native PID 1449119 and ffplay PID 1449117, original
VGA2USB 5555:1110 firmware 0102 at USB path 1-12.2. The player receives complete
1036815-byte 720×480 PPM canvases; the recovered physical source remains 720×400.

## Observed sequence

The tester requested a PC reboot. Capture image reads stopped completing.
The scheduled observation cancelled and drained the five reads before B1,
emitted a black frame, reopened the USB capture session and waited for timing.
Both process IDs remained unchanged. The test PC apparently did not POST on
that attempt: the tester noted its Num Lock indicator was off, then removed
and restored power. This is not evidence of a successful software warm reset.

The unchanged pipeline logged `VGA signal returned; resuming capture`, discarded
short frames, adapted through V=4 to V=5, and displayed the Phoenix BIOS 3.10.01
memory test for the 80386SX 16 MHz PC.
Frame 64 at monotonic 936729.818 and frame 128 at 936737.122 establish continuing
acquisition after return. Neither native capture nor ffplay was restarted.

This qualifies signal-loss clearing and automatic recovery on one physical
cold power cycle. It does not qualify graphics/text geometry transitions,
repeated reset endurance or physical USB unplug. Synthetic transport checks
exercise 720→loss→640→800→1024 (P=2)→720 with exact rendered pixels. Real FFmpeg
checks fixed packets across four source geometries.
