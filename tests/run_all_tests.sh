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
CHAIN_NET="$CHAIN_DIR/tezos_http.c $CHAIN_DIR/tezos_tzkt.c $CHAIN_DIR/tezos_micheline.c"
CHAIN_BEACON="$CHAIN_DIR/tezos_beacon_crypto.c $CHAIN_DIR/tezos_beacon_message.c $CHAIN_DIR/tezos_beacon_qr.c $CHAIN_DIR/tezos_matrix.c $CHAIN_DIR/tezos_beacon_pair.c"
CORE_ENGINE="$CORE_DIR/tezos_core.c $CORE_DIR/tezos_gfx.c $CORE_DIR/tezos_fonts.c $CORE_DIR/tezos_widgets.c $CORE_DIR/tezos_t9.c $CORE_DIR/tezos_popup.c $CORE_DIR/tezos_pin_entry.c $CORE_DIR/tezos_boot_splash.c"
APP_WALLET="$APPS_DIR/wallet/wallet.c $CORE_DIR/tezos_address_book.c $CHAIN_DIR/tezos_tez_domain.c"
APP_SETTINGS="$APPS_DIR/settings/settings.c $APPS_DIR/settings/import_recovery.c $CHAIN_DIR/tezos_wifi.c $APPS_DIR/snake/snake.c"
APP_CAMERA="$APPS_DIR/camera/camera.c $APPS_DIR/camera/camera_pipeline.c $APPS_DIR/beacon_wallet/beacon_wallet.c"
APP_SCANNER="$APPS_DIR/scanner/scanner.c"
APP_EXPLORER="$APPS_DIR/explorer/explorer.c"
APP_MESSENGER="$APPS_DIR/messenger/messenger.c"
APP_GALLERY="$APPS_DIR/gallery/gallery.c $APPS_DIR/camera/camera_pipeline.c $CORE_DIR/tezos_image_decode.c"
APP_DISCOVER="$APPS_DIR/discover/discover.c $APPS_DIR/camera/camera_pipeline.c $CORE_DIR/tezos_image_decode.c"

# --- chain layer: base crypto/forging ---
# tezos_micheline.c now uses cJSON internally (dynamic entrypoint-calling
# engine) -- every test linking $CHAIN_MICHELINE needs -lcjson now, not
# just tests that use cJSON themselves. Same class of dependency-
# propagation issue this project has hit before when a shared module
# gained a new dependency.
EXTRA_LIBS="-lcjson $(pkg-config --cflags --libs libsodium)"
run_test "test_base58"  test_base58.c  $CHAIN_CORE $CHAIN_MICHELINE
run_test "test_forge"   test_forge.c   $CHAIN_CORE $CHAIN_MICHELINE
run_test "test_signer"  test_signer.c  $CHAIN_CORE $CHAIN_MICHELINE
run_test "test_bip39_slip10" test_bip39_slip10.c $CHAIN_CORE $CHAIN_MICHELINE
run_test "test_micheline" test_micheline.c $CHAIN_CORE $CHAIN_MICHELINE
run_test "test_micheline_dynamic" test_micheline_dynamic.c $CHAIN_CORE $CHAIN_MICHELINE
run_test "test_micheline_walker" test_micheline_walker.c $CHAIN_CORE $CHAIN_MICHELINE
run_test "test_micheline_real_liquidity_baking" test_micheline_real_liquidity_baking.c $CHAIN_CORE $CHAIN_MICHELINE
run_test "test_micheline_real_quipuswap" test_micheline_real_quipuswap.c $CHAIN_CORE $CHAIN_MICHELINE
run_test "test_micheline_real_teia_channels" test_micheline_real_teia_channels.c $CHAIN_CORE $CHAIN_MICHELINE
run_test "test_signer_protocol" test_signer_protocol.c $CHAIN_CORE $CHAIN_MICHELINE $CHAIN_PROTO

# --- network client (attempts REAL live calls if network is available --
#     genuinely more capable here than in the original dev sandbox) ---
EXTRA_LIBS="-lcurl -lcjson $(pkg-config --cflags --libs libsodium)"
run_test "test_network" test_network.c $CHAIN_NET $CHAIN_DIR/tezos_forge.c $CHAIN_DIR/tezos_base58.c
run_test "test_tzkt_entrypoints" test_tzkt_entrypoints.c $CHAIN_NET $CHAIN_DIR/tezos_forge.c $CHAIN_DIR/tezos_base58.c

run_test "test_tzkt_entrypoint_types" test_tzkt_entrypoint_types.c $CHAIN_NET $CHAIN_DIR/tezos_forge.c $CHAIN_DIR/tezos_base58.c
run_test "test_tzkt_token_balances" test_tzkt_token_balances.c $CHAIN_NET $CHAIN_DIR/tezos_forge.c $CHAIN_DIR/tezos_base58.c
run_test "test_tzkt_get_message" test_tzkt_get_message.c $CHAIN_NET $CHAIN_DIR/tezos_forge.c $CHAIN_DIR/tezos_base58.c
run_test "test_tzkt_owned_tokens" test_tzkt_owned_tokens.c $CHAIN_NET $CHAIN_DIR/tezos_forge.c $CHAIN_DIR/tezos_base58.c
run_test "test_tzkt_contract_info" test_tzkt_contract_info.c $CHAIN_NET $CHAIN_DIR/tezos_forge.c $CHAIN_DIR/tezos_base58.c
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
run_test "test_beacon_wallet" test_beacon_wallet.c $APPS_DIR/beacon_wallet/beacon_wallet.c $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_BEACON $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c

# --- Beacon QR (needs real qrencode + zbar) ---
EXTRA_LIBS="-lqrencode -lzbar -lcjson $(pkg-config --cflags --libs libsodium)"
run_test "test_beacon_qr" test_beacon_qr.c $CHAIN_DIR/tezos_beacon_qr.c $CHAIN_DIR/tezos_base58.c

# --- core engine: render/state-machine widgets ---
EXTRA_LIBS=""
run_test "test_t9" test_t9.c $CORE_DIR/tezos_t9.c $CORE_DIR/tezos_gfx.c $CORE_DIR/tezos_fonts.c
run_test "test_boot_splash" test_boot_splash.c $CORE_ENGINE
run_test "test_icon_bitmap" test_icon_bitmap.c $CORE_DIR/tezos_gfx.c
run_test "test_grid_nav" test_grid_nav.c $CORE_DIR/tezos_grid_nav.c
EXTRA_LIBS="-lm"
run_test "test_image_decode" test_image_decode.c $CORE_DIR/tezos_image_decode.c
EXTRA_LIBS=""
EXTRA_LIBS="-lcjson $(pkg-config --cflags --libs libsodium)"
run_test "test_address_book" test_address_book.c $CORE_DIR/tezos_address_book.c $CHAIN_CORE $CHAIN_MICHELINE $CHAIN_DIR/tezos_store.c
run_test "test_address_book_persist_p1" test_address_book_persist_p1.c $CORE_DIR/tezos_address_book.c $CHAIN_CORE $CHAIN_MICHELINE $CHAIN_DIR/tezos_store.c
run_test "test_address_book_persist_p2" test_address_book_persist_p2.c $CORE_DIR/tezos_address_book.c $CHAIN_CORE $CHAIN_MICHELINE $CHAIN_DIR/tezos_store.c
run_test "test_snake" test_snake.c $APPS_DIR/snake/snake.c $CORE_ENGINE $CHAIN_DIR/tezos_store.c
run_test "test_popup" test_popup.c $CORE_DIR/tezos_popup.c $CORE_DIR/tezos_core.c $CORE_DIR/tezos_gfx.c $CORE_DIR/tezos_fonts.c
run_test "test_pin_entry" test_pin_entry.c $CORE_DIR/tezos_pin_entry.c $CORE_DIR/tezos_core.c $CORE_DIR/tezos_gfx.c $CORE_DIR/tezos_fonts.c

EXTRA_LIBS="$(pkg-config --cflags --libs libsodium)"
run_test "test_store" test_store.c $CHAIN_DIR/tezos_store.c
run_test "test_letterbox" test_letterbox.c $CORE_DIR/tezos_gfx.c

# --- real apps, exercising the full stack together ---
EXTRA_LIBS="-lcurl -lcjson $(pkg-config --cflags --libs libsodium)"
run_test "test_wallet_screens" test_wallet_screens.c $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c

EXTRA_LIBS="$(pkg-config --cflags --libs libsodium)"
run_test "test_import_recovery" test_import_recovery.c $APP_SETTINGS $APP_WALLET $CORE_ENGINE $CHAIN_CORE $CHAIN_MICHELINE $CHAIN_DIR/tezos_http.c $CHAIN_DIR/tezos_tzkt.c $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c -lcurl -lcjson
run_test "test_settings_snake" test_settings_snake.c $APP_SETTINGS $APP_WALLET $CORE_ENGINE $CHAIN_CORE $CHAIN_MICHELINE $CHAIN_DIR/tezos_http.c $CHAIN_DIR/tezos_tzkt.c $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c -lcurl -lcjson

# Device name persistence -- TWO SEPARATE test binaries run in sequence,
# sharing only a file on disk. Must run in this order; p2 depends on
# the file p1 creates.
run_test "test_device_name_persist_p1" test_device_name_persist_p1.c $APP_SETTINGS $APP_WALLET $CORE_ENGINE $CHAIN_CORE $CHAIN_MICHELINE $CHAIN_DIR/tezos_http.c $CHAIN_DIR/tezos_tzkt.c $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c -lcurl -lcjson
run_test "test_device_name_persist_p2" test_device_name_persist_p2.c $APP_SETTINGS $APP_WALLET $CORE_ENGINE $CHAIN_CORE $CHAIN_MICHELINE $CHAIN_DIR/tezos_http.c $CHAIN_DIR/tezos_tzkt.c $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c -lcurl -lcjson

run_test "test_rpc_node_config" test_rpc_node_config.c $APP_SETTINGS $APP_WALLET $CORE_ENGINE $CHAIN_CORE $CHAIN_MICHELINE $CHAIN_DIR/tezos_http.c $CHAIN_DIR/tezos_tzkt.c $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c -lcurl -lcjson
run_test "test_lock_change_pin" test_lock_change_pin.c $APP_SETTINGS $APP_WALLET $CORE_ENGINE $CHAIN_CORE $CHAIN_MICHELINE $CHAIN_DIR/tezos_http.c $CHAIN_DIR/tezos_tzkt.c $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c -lcurl -lcjson

EXTRA_LIBS=""
run_test "test_wifi" test_wifi.c $CHAIN_DIR/tezos_wifi.c

EXTRA_LIBS="-lcurl -lcjson $(pkg-config --cflags --libs libsodium)"
run_test "test_wifi_screen" test_wifi_screen.c $APP_SETTINGS $APP_WALLET $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c

# Profiles/multi-account -- TWO SEPARATE test binaries in sequence,
# sharing only a file on disk. Must run in this order.
run_test "test_profiles_p1" test_profiles_p1.c $APP_SETTINGS $APP_WALLET $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c
run_test "test_profiles_p2" test_profiles_p2.c $APP_SETTINGS $APP_WALLET $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c

EXTRA_LIBS="-lqrencode -lzbar -lcurl -lcjson $(pkg-config --cflags --libs libsodium) -lm"
run_test "test_camera_scan" test_camera_scan.c $APP_CAMERA $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_BEACON $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c
run_test "test_camera_pipeline" test_camera_pipeline.c $APPS_DIR/camera/camera_pipeline.c

EXTRA_LIBS="-lqrencode -lzbar -lcurl -lcjson $(pkg-config --cflags --libs libsodium) -lm"
run_test "test_camera_capture_flow" test_camera_capture_flow.c $APP_CAMERA $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_BEACON $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c

EXTRA_LIBS="-lqrencode -lzbar -lcjson -lcurl $(pkg-config --cflags --libs libsodium)"
run_test "test_scanner" test_scanner.c $APP_SCANNER $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c
run_test "test_scanner_entrypoints" test_scanner_entrypoints.c $APP_SCANNER $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c
run_test "test_explorer_ui" test_explorer_ui.c $APP_EXPLORER $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c
run_test "test_explorer_call" test_explorer_call.c $APP_EXPLORER $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c
run_test "test_messenger_build_value" test_messenger_build_value.c $APP_MESSENGER $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c
run_test "test_messenger_ui" test_messenger_ui.c $APP_MESSENGER $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c
run_test "test_messenger_inbox_ui" test_messenger_inbox_ui.c $APP_MESSENGER $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c
EXTRA_LIBS="-lqrencode -lzbar -lcjson -lcurl $(pkg-config --cflags --libs libsodium) -lm"
run_test "test_gallery_ui" test_gallery_ui.c $APP_GALLERY $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c
run_test "test_gallery_viewer" test_gallery_viewer.c $APP_GALLERY $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c
run_test "test_discover" test_discover.c $APP_DISCOVER $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c
run_test "test_discover_suggested" test_discover_suggested.c $APP_DISCOVER $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c
run_test "test_wallet_send" test_wallet_send.c $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c
run_test "test_wallet_send_ui" test_wallet_send_ui.c $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c
run_test "test_wallet_contacts" test_wallet_contacts.c $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c

# --- Signer firmware: portable, chip-independent wire protocol (real
#     ISO 7816-4 APDU framing) -- pure C, no chain/core dependencies ---
EXTRA_LIBS=""
run_test "test_signer_apdu" test_signer_apdu.c ../firmware/signer/protocol/tezos_signer_apdu.c
run_test "test_signer_session" test_signer_session.c ../firmware/signer/protocol/tezos_signer_session.c ../firmware/signer/protocol/tezos_signer_apdu.c
run_test "test_signer_rng" test_signer_rng.c ../firmware/signer/protocol/tezos_signer_rng_core.c

# --- .tez name resolution ---
EXTRA_LIBS="-lqrencode -lzbar -lcjson -lcurl $(pkg-config --cflags --libs libsodium) -lm"
run_test "test_tez_domain" test_tez_domain.c $CHAIN_DIR/tezos_tez_domain.c $CHAIN_CORE $CHAIN_NET
run_test "test_wallet_tez_domain" test_wallet_tez_domain.c $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c
run_test "test_settings_registry" test_settings_registry.c $APP_WALLET $APP_SETTINGS $CORE_ENGINE $CHAIN_CORE $CHAIN_NET $CHAIN_DIR/tezos_rpc.c $CHAIN_DIR/tezos_store.c

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
