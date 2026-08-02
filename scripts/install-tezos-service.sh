#!/bin/bash
# Builds src/shim-linux and installs it as a boot-to-kiosk systemd
# service. Run this ON the Pi Zero 2 W, not in a dev environment —
# it links against real libdrm and expects real /dev/dri, /dev/input
# devices to exist.
set -e

if [ ! -e /dev/dri/card0 ] && [ ! -e /dev/dri/card1 ]; then
  echo "No /dev/dri/cardN found. Fix the display overlay first — see"
  echo "ROADMAP.md M1 and the config.txt steps in the project README."
  exit 1
fi

sudo apt update
sudo apt install -y build-essential libdrm-dev

cd "$(dirname "$0")/../src/shim-linux"
gcc -O2 -Wall -Wextra -std=c11 -o tezos_shim linux_shim.c \
    ../core/tezos_core.c ../core/tezos_gfx.c ../core/tezos_fonts.c \
    ../core/tezos_widgets.c -I../core $(pkg-config --cflags --libs libdrm)

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
