#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}
export WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-wayland-0}
width=720 height=400 previous=
for argument do
  if [ "$previous" = --mode ]; then
    case "$argument" in
      640x480) width=640 height=480 ;;
      720x400) width=720 height=400 ;;
    esac
  fi
  previous=$argument
done
# Both supported P6 headers are 15 bytes; keep each frame in one input packet.
frame_bytes=$((width * height * 3 + 15))
# USB access is required only for the producer; the viewer runs as the desktop user.
sudo -n "$project_dir/build/vga2usb-native" --stream "$@" |
  ffplay -hide_banner -loglevel warning -fs -window_title 'VGA2USB live capture' \
    -f image2pipe -vcodec ppm -frame_size "$frame_bytes" -framerate 60 -use_wallclock_as_timestamps 1 \
    -sync ext -fflags nobuffer -noinfbuf -probesize 32 -analyzeduration 0 \
    -framedrop -vf setdar=4/3 -i -
