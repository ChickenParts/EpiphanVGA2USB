# Building and using the original VGA2USB

This is the user guide for the original Epiphan VGA2USB, USB ID `5555:1110`. The VGA2USB LR and other Epiphan models are not supported by this driver.

Everything runs in userspace through libusb. There's no legacy kernel module to load, no host kernel downgrade, and no host reboot required.

## Requirements and firmware

You'll need:

- A C compiler, Ninja, pkg-config, and the libusb-1.0 development files
- Ruby with minitest and zlib
- Binutils (for the vendor object)
- FFmpeg/ffplay for live viewing and viewer tests
- `curl`, `ar`, and `tar` to unpack the vendor package as shown in the firmware instructions

The original firmware is **not** in this repository. Fetch the identified official Epiphan package and extract `vga2usb_bins.o_shipped` into `vendor/vga2usb_bins.o_shipped` following [the firmware instructions](../firmware/README.md). The package and the generated firmware remain ignored by Git. Extraction is hash-checked, and it doesn't involve loading the vendor module.

From the repository root:

```sh
ninja test driver firmware-audit
```

The executable will be `build/vga2usb-native`. The tests are offline checks; they do not, by themselves, prove that a particular VGA mode works on physical hardware.

## Find and start the device

```sh
./build/vga2usb-native --inspect
```

The original model reports vendor/product ID `5555:1110`. An uninitialized unit may report `bcdDevice=0000`; the tested firmware reports `0102` after startup.

If it needs firmware:

```sh
sudo ./build/vga2usb-native --bootstrap
```

This uploads the firmware into **volatile RAM** and releases the device CPU. It doesn't flash EEPROM. The USB device may re-enumerate, so run `--inspect` again before capturing.

USB read/write permission is needed for startup and capture. The examples use `sudo`; a suitable device-access rule is another option.

A physical USB path can be used when more than one device matches:

```sh
./build/vga2usb-native --inspect --port 1-12.2
sudo ./build/vga2usb-native --bootstrap --port 1-12.2
```

`1-12.2` was the test unit's path. Use the path printed for *your* device; it may change if you move cables or ports.

To query the 10-byte VGA sync response after startup, with capture stopped:

```sh
sudo ./build/vga2usb-native --sync
```

Don't run a separate sync query while capturing. The original firmware's control request interrupts active frame acquisition; the capture program handles measurements between drained frame reads.

## Screenshots

```sh
sudo ./build/vga2usb-native --capture screen.ppm
ffmpeg -i screen.ppm -frames:v 1 screen.png
```

A one-shot PPM retains the captured source dimensions. It refuses unsupported or absent timing instead of inventing a frame.

Mode selection defaults to automatic. You can pin two original timing presets when the source is known:

```sh
sudo ./build/vga2usb-native --capture screen.ppm --mode 720x400
sudo ./build/vga2usb-native --capture screen.ppm --mode 640x480
```

Automatic selection uses the original timing score and a table of 36 progressive modes through 1024×768. That does **not** mean every one has been tried on physical hardware. At present, 720×400 text capture is the physically qualified mode. Interlaced modes aren't supported.

The timing response doesn't always uniquely identify horizontal geometry. The explicit presets are useful for that case.

## Live viewing

```sh
sudo -v
./tools/view-live.sh
```

This pipes the native stream into ffplay, opening a full-screen window. `tools/view-live.sh` runs the USB producer with `sudo -n`, so the password must already be cached or USB access must otherwise be permitted.

To select a USB path:

```sh
./tools/view-live.sh --port 1-12.2
```

The viewer disables ffplay's playback keyboard/mouse shortcuts; typing at the captured machine shouldn't accidentally pause the mirror. Close the window through the window manager.

The native `--stream` option writes consecutive binary PPM images to standard output. Unlike a screenshot, continuous output is normalized to a 720×480 RGB canvas (1036815 bytes per complete PPM packet) so the viewer can keep decoding when source dimensions change. A missing signal produces a black frame. This is **not** a 720×480 capture claim: the physically verified source was 720×400.

The viewer's `-framerate 60` setting controls input timestamp precision, **not** the capture frame rate. The test host measured about 8.8 complete frames per second at five capture phases. Other sources, modes, and USB paths may behave differently.

Only one process can claim the capture interface. Close the live viewer before taking an independent screenshot with `--capture`. You can, of course, take screenshots of the desktop while the viewer is running.

## Recovery and current limits

If the **VGA source** disappears, the live capture path clears the displayed image, drains outstanding reads before checking the signal, and waits for it to return. Returning signal can reopen the USB capture session without restarting ffplay. This was verified for one physical cold power cycle of the source PC; [the test record](hardware/reset-recovery-2026-09-29.md) has the details.

Physical **USB unplug/reconnect** is different. Automatic recovery from unplugging the capture device has not been qualified. For now:

1. Stop the viewer.
2. Reconnect the VGA2USB.
3. Run `--inspect` and, if needed, `--bootstrap`.
4. Restart the viewer.

If a firmware transfer fails partway through, disconnect and reconnect the device before trying again. Don't attempt guessed vendor commands or EEPROM writes to repair it.

Short or incomplete image transfers are discarded. After three consecutive incomplete frames, the capture path advances its vertical phase count and tries again. The background is documented in [HOME recovery](protocol/home-recovery.md) and [receive transport](protocol/receive-transport.md).

## Tests and debugging

The normal check is:

```sh
ninja test driver firmware-audit
```

For focused capture/viewer regression checks:

```sh
ninja test-session test-transport test-stream-frame test-viewer
```

These exercise transfer failures, session cleanup, frame boundaries, mode changes, and decoding through real FFmpeg. They are not a replacement for tests with a VGA source attached.

Start with the [hardware test index](hardware/README.md) for the actual validation record and [protocol notes](protocol/README.md) for request layouts, ADC setup, mode detection, and transport ordering.
