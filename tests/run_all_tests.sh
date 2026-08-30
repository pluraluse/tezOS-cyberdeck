#!/bin/bash
# Master test runner -- builds and runs every verified test suite in
# this project with one command. Designed to run on real hardware
# (a Pi, or anywhere) as a genuine first-hardware-verification step,
# not just in the original dev sandbox this was built in.
#
# Usage: ./tests/run_all_tests.sh
#
# Required packages (see README.md / docs/build-notes for why each is
# needed):
#   sudo apt update
#   sudo apt install -y build-essential libdrm-dev libsodium-dev \
#       libcurl4-openssl-dev libcjson-dev libqrencode-dev libzbar-dev pkg-config

set -uo pipefail
cd "$(dirname "$0")"

CC=gcc
CFLAGS="-Wall -Wextra -std=c11"
CORE_DIR="../src/core"
CHAIN_DIR="../src/chain"
APPS_DIR="../src/apps"
BUILD_DIR="/tmp/tezos-cyberdeck-test-build"
mkdir -p "$BUILD_DIR"

PASS_COUNT=0
FAIL_COUNT=0
FAILED_TESTS=()

run_test() {
    local name="$1"
    shift
    local sources="$@"
    local bin="$BUILD_DIR/$name"

    echo ""
    echo "=================================================================="
    echo "  $name"
    echo "=================================================================="

    if ! $CC $CFLAGS -o "$bin" $sources $EXTRA_LIBS 2>"$BUILD_DIR/$name.compile.log"; then
        echo "COMPILE FAILED -- see $BUILD_DIR/$name.compile.log"
        FAIL_COUNT=$((FAIL_COUNT+1))
        FAILED_TESTS+=("$name (compile failed)")
        return
    fi

    if "$bin"; then
        PASS_COUNT=$((PASS_COUNT+1))
    else
        FAIL_COUNT=$((FAIL_COUNT+1))
        FAILED_TESTS+=("$name")
    fi
}

CHAIN_BIP="$CHAIN_DIR/tezos_bip39.c $CHAIN_DIR/tezos_slip10.c"
CHAIN_CORE="$CHAIN_DIR/tezos_base58.c $CHAIN_DIR/tezos_forge.c $CHAIN_DIR/tezos_signer.c $CHAIN_BIP"
CHAIN_MICHELINE="$CHAIN_DIR/tezos_micheline.c"
CHAIN_PROTO="$CHAIN_DIR/tezos_signer_protocol.c"
CHAIN_NET="$CHAIN_DIR/tezos_http.c $CHAIN_DIR/tezos_tzkt.c"
CHAIN_BEACON="$CHAIN_DIR/tezos_beacon_crypto.c $CHAIN_DIR/tezos_beacon_message.c $CHAIN_DIR/tezos_beacon_qr.c $CHAIN_DIR/tezos_matrix.c $CHAIN_DIR/tezos_beacon_pair.c"
CORE_ENGINE="$CORE_DIR/tezos_core.c $CORE_DIR/tezos_gfx.c $CORE_DIR/tezos_fonts.c $CORE_DIR/tezos_widgets.c $CORE_DIR/tezos_t9.c $CORE_DIR/tezos_popup.c"
APP_WALLET="$APPS_DIR/wallet/wallet.c"
APP_SETTINGS="$APPS_DIR/settings/settings.c $APPS_DIR/settings/import_recovery.c"
APP_CAMERA="$APPS_DIR/camera/camera.c $APPS_DIR/camera/camera_pipeline.c"

# --- chain layer: base crypto/forging ---
EXTRA_LIBS="$(pkg-config --cflags --libs libsodium)"
run_test "test_base58"  test_base58.c  $CHAIN_CORE $CHAIN_MICHELINE
run_test "test_forge"   test_forge.c   $CHAIN_CORE $CHAIN_MICHELINE
run_test "test_signer"  test_signer.c  $CHAIN_CORE $CHAIN_MICHELINE
run_test "test_bip39_slip10" test_bip39_slip10.c $CHAIN_CORE $CHAIN_MICHELINE
run_test "test_micheline" test_micheline.c $CHAIN_CORE $CHAIN_MICHELINE
run_test "test_signer_protocol" test_signer_protocol.c $CHAIN_CORE $CHAIN_MICHELINE $CHAIN_PROTO

# --- network client (attempts REAL live calls if network is available --
#     genuinely more capable here than in the original dev sandbox) ---
EXTRA_LIBS="-lcurl -lcjson"
run_test "test_network" test_network.c $CHAIN_NET
run_test "test_rpc" test_rpc.c $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_http.c

# --- Matrix client ---
EXTRA_LIBS="-lcurl -lcjson"
run_test "test_matrix" test_matrix.c $CHAIN_DIR/tezos_matrix.c $CHAIN_DIR/tezos_http.c

# --- Beacon: crypto, message schema, full pairing flow ---
EXTRA_LIBS="-lcjson $(pkg-config --cflags --libs libsodium)"
run_test "test_beacon_crypto" test_beacon_crypto.c $CHAIN_DIR/tezos_beacon_crypto.c $CHAIN_DIR/tezos_base58.c
run_test "test_beacon_message" test_beacon_message.c $CHAIN_DIR/tezos_beacon_message.c
EXTRA_LIBS="-lcurl -lcjson $(pkg-config --cflags --libs libsodium)"
run_test "test_beacon_pair" test_beacon_pair.c $CHAIN_BEACON $CHAIN_DIR/tezos_base58.c $CHAIN_DIR/tezos_http.c

# --- Beacon QR (needs real qrencode + zbar) ---
EXTRA_LIBS="-lqrencode -lzbar -lcjson $(pkg-config --cflags --libs libsodium)"
run_test "test_beacon_qr" test_beacon_qr.c $CHAIN_DIR/tezos_beacon_qr.c $CHAIN_DIR/tezos_base58.c

# --- core engine: render/state-machine widgets ---
EXTRA_LIBS=""
run_test "test_t9" test_t9.c $CORE_DIR/tezos_t9.c $CORE_DIR/tezos_gfx.c $CORE_DIR/tezos_fonts.c
run_test "test_popup" test_popup.c $CORE_DIR/tezos_popup.c $CORE_DIR/tezos_core.c $CORE_DIR/tezos_gfx.c $CORE_DIR/tezos_fonts.c
run_test "test_letterbox" test_letterbox.c $CORE_DIR/tezos_gfx.c

# --- real apps, exercising the full stack together ---
EXTRA_LIBS="-lcurl -lcjson $(pkg-config --cflags --libs libsodium)"
run_test "test_wallet_screens" test_wallet_screens.c $APP_WALLET $CORE_ENGINE $CHAIN_CORE $CHAIN_MICHELINE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c

EXTRA_LIBS="$(pkg-config --cflags --libs libsodium)"
run_test "test_import_recovery" test_import_recovery.c $APP_SETTINGS $APP_WALLET $CORE_ENGINE $CHAIN_CORE $CHAIN_MICHELINE $CHAIN_DIR/tezos_http.c $CHAIN_DIR/tezos_tzkt.c $CHAIN_DIR/tezos_rpc.c -lcurl -lcjson

EXTRA_LIBS="-lqrencode -lzbar -lcjson $(pkg-config --cflags --libs libsodium)"
EXTRA_LIBS="-lqrencode -lzbar -lcjson $(pkg-config --cflags --libs libsodium) -lm"
run_test "test_camera_scan" test_camera_scan.c $APP_CAMERA $CORE_ENGINE $CHAIN_DIR/tezos_beacon_qr.c $CHAIN_DIR/tezos_base58.c
run_test "test_camera_pipeline" test_camera_pipeline.c $APPS_DIR/camera/camera_pipeline.c

EXTRA_LIBS="-lqrencode -lzbar -lcjson $(pkg-config --cflags --libs libsodium) -lm"
run_test "test_camera_capture_flow" test_camera_capture_flow.c $APP_CAMERA $CORE_ENGINE $CHAIN_DIR/tezos_beacon_qr.c $CHAIN_DIR/tezos_base58.c

echo ""
echo "=================================================================="
echo "  SUMMARY: $PASS_COUNT passed, $FAIL_COUNT failed"
echo "=================================================================="
if [ $FAIL_COUNT -gt 0 ]; then
    echo "Failed test suites:"
    for t in "${FAILED_TESTS[@]}"; do echo "  - $t"; done
    exit 1
fi
echo "All test suites passed."
exit 0
