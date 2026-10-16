#!/bin/bash
# Builds src/shim-linux and installs it as a boot-to-kiosk systemd
# service. Run this ON the Pi Zero 2 W, not in a dev environment —
# it expects a real /dev/fb1 (the confirmed bring-up config:
# dtoverlay=fbtft,ili9481,... — see docs/build-notes/
# display-bringup-build-notes.md) and real /dev/input devices to exist.
#
# No longer links against libdrm. linux_shim.c was rewritten against
# fbdev after real hardware bring-up found the piscreen overlay never
# worked and the overlay that does work (fbtft,ili9481) uses the legacy
# fbtft staging driver, which exposes a plain fbdev device, not a DRM
# one — see the file-top comment in linux_shim.c.
#
# NOTE: this script was stale for a long stretch of development --
# it only built the original core/ files and would have failed to
# link against everything added since (the entire chain/ layer, and
# every real app screen). Fixed to match the actual current build
# command, same one used throughout this project's own verification
# (see tests/run_all_tests.sh and docs/build-notes/*.md for what each
# piece does and how it was checked).
set -e

if [ ! -e /dev/fb1 ]; then
  echo "No /dev/fb1 found. Fix the display overlay first — see"
  echo "ROADMAP.md M1 and the config.txt steps in the project README."
  exit 1
fi

sudo apt update
sudo apt install -y build-essential libsodium-dev \
    libcurl4-openssl-dev libcjson-dev libqrencode-dev libzbar-dev pkg-config

cd "$(dirname "$0")/../src/shim-linux"
# Globs, not a hand-maintained file list -- this exact list has drifted
# stale and silently broken this script TWICE now (once missing the
# entire chain/ layer and every app, once missing three more files
# added in a later session). A new .c file in any of these directories
# now just needs to exist in the right place, not be remembered and
# added here by hand.
#
# ONE explicit exclusion: core/demo_idle_screen.c is a standalone demo
# program with its own main() (a duplicate-main link error otherwise),
# not a shared library module -- everything else in these directories
# genuinely is meant to be linked into the real shim.
CORE_SOURCES=$(ls ../core/*.c | grep -v demo_idle_screen.c)
# Real bug this exact pattern already caused twice before (see git history /
# ROADMAP.md): a hand-listed app directory list drifts stale whenever a new
# app goes from stub to real and nobody remembers to add it here -- this
# session it was messenger.c AND gallery.c, both silently missing. `find`
# (unlike a raw glob) doesn't error or leave a literal unmatched pattern on
# the command line for app directories that are still stub-only (no .c
# files yet, e.g. explorer/discover/art1bit) -- structurally immune to this
# drift the same way CORE_SOURCES already is, rather than needing yet
# another manual fix next time an app goes real.
APP_SOURCES=$(find ../apps -name '*.c')
gcc -O2 -Wall -Wextra -std=c11 -o tezos_shim linux_shim.c \
    $CORE_SOURCES \
    ../chain/*.c \
    $APP_SOURCES \
    -I../core -I../chain -I../apps/wallet -I../apps/settings \
    $(pkg-config --cflags --libs libsodium) -lcurl -lcjson -lqrencode -lzbar -lm

sudo mkdir -p /opt/tezos
sudo cp tezos_shim /opt/tezos/
sudo cp ../../scripts/tezos.service /etc/systemd/system/tezos.service
sudo systemctl daemon-reload
sudo systemctl enable tezos.service

echo "Installed. Before rebooting into this:"
echo "  1. Confirm hardware/pinouts/tca8418-overlay-notes.md's overlay is loaded"
echo "     and the keypad shows up in /proc/bus/input/devices"
echo "  2. Update the placeholder input device paths in linux_shim.c's main()"
echo "     to match your actual /dev/input/by-id/* entries, then rerun this script"
echo "  3. sudo systemctl start tezos.service   # test before rebooting into it"
echo "  4. sudo reboot"
