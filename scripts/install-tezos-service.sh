#!/bin/bash
# Builds src/shim-linux and installs it as a boot-to-kiosk systemd
# service. Run this ON the Pi Zero 2 W, not in a dev environment —
# it links against real libdrm and expects real /dev/dri, /dev/input
# devices to exist.
#
# NOTE: this script was stale for a long stretch of development --
# it only built the original core/ files and would have failed to
# link against everything added since (the entire chain/ layer, and
# every real app screen). Fixed to match the actual current build
# command, same one used throughout this project's own verification
# (see tests/run_all_tests.sh and docs/build-notes/*.md for what each
# piece does and how it was checked).
set -e

if [ ! -e /dev/dri/card0 ] && [ ! -e /dev/dri/card1 ]; then
  echo "No /dev/dri/cardN found. Fix the display overlay first — see"
  echo "ROADMAP.md M1 and the config.txt steps in the project README."
  exit 1
fi

sudo apt update
sudo apt install -y build-essential libdrm-dev libsodium-dev \
    libcurl4-openssl-dev libcjson-dev libqrencode-dev libzbar-dev pkg-config

cd "$(dirname "$0")/../src/shim-linux"
gcc -O2 -Wall -Wextra -std=c11 -o tezos_shim linux_shim.c \
    ../core/tezos_core.c ../core/tezos_gfx.c ../core/tezos_fonts.c ../core/tezos_widgets.c ../core/tezos_t9.c ../core/tezos_popup.c \
    ../chain/tezos_base58.c ../chain/tezos_forge.c ../chain/tezos_signer.c ../chain/tezos_bip39.c ../chain/tezos_slip10.c ../chain/tezos_micheline.c ../chain/tezos_signer_protocol.c ../chain/tezos_http.c ../chain/tezos_tzkt.c ../chain/tezos_rpc.c \
    ../chain/tezos_beacon_crypto.c ../chain/tezos_beacon_message.c ../chain/tezos_beacon_qr.c ../chain/tezos_matrix.c ../chain/tezos_beacon_pair.c \
    ../apps/wallet/wallet.c \
    ../apps/settings/settings.c ../apps/settings/import_recovery.c \
    ../apps/camera/camera.c ../apps/camera/camera_pipeline.c \
    -I../core -I../chain -I../apps/wallet -I../apps/settings \
    $(pkg-config --cflags --libs libdrm) $(pkg-config --cflags --libs libsodium) -lcurl -lcjson -lqrencode -lzbar -lm

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
