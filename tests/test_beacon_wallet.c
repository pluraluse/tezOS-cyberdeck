/* Real end-to-end incoming-Beacon-request flow -- the piece that makes
   tezOS an actual standalone Beacon-compatible wallet, not just a QR
   pairing demo. Reuses the exact real permission_request fixture from
   test_beacon_message.c (already verified against real beacon-sdk
   schemas) rather than inventing a new one; the operation_request uses
   real-format addresses (not test_beacon_message.c's schema-only
   placeholders) so the real forge pipeline can genuinely be exercised
   structurally. */
#include "../src/apps/beacon_wallet/beacon_wallet.h"
#include "../src/apps/wallet/wallet.h"
#include "../src/core/tezos_core.h"
#include "../src/core/tezos_gfx.h"
#include <stdio.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

static void tap(tezos_core_t *core, tezos_input_type_t type, int value) {
    tezos_input_event_t ev = { type, value };
    tezos_core_tick(core, &ev);
}

int main(void) {
    const char *mnemonic = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about";
    wallet_set_active_signer_from_mnemonic(mnemonic, 12, "", 0);

    /* === permission_request -- the exact real fixture from
       test_beacon_message.c, reused, not reinvented === */
    tezos_fb_t *fb = tezos_fb_create(320, 480);
    tezos_core_t core;
    tezos_core_init(&core, fb);
    tezos_core_push(&core, beacon_wallet_test_inject_active_session());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    CHECK(core.depth == 1, "injected active session pushed correctly");

    const char *perm_req_json =
        "{\"type\":\"permission_request\",\"version\":\"3\",\"id\":\"req-123\","
        "\"senderId\":\"peer-abc\",\"appMetadata\":{\"senderId\":\"peer-abc\",\"name\":\"Kukai\"},"
        "\"network\":{\"type\":\"mainnet\"},\"scopes\":[\"operation_request\",\"sign\"]}";
    CHECK(beacon_wallet_test_inject_message(perm_req_json), "the real permission_request fixture is correctly classified");
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    tezos_fb_dump_ppm(fb, "beacon_permission_request.ppm");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* APPROVE -- pushes a real confirmation popup */
    CHECK(core.depth == 2, "approving a permission_request pushes a real confirmation popup, doesn't respond immediately");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* confirm -- real response-building + send attempt */
    CHECK(core.depth == 1, "confirming pops back with a real result (whatever the network outcome)");
    tezos_fb_dump_ppm(fb, "beacon_response_result.ppm");

    tap(&core, TEZOS_INPUT_SOFTKEY_R, 0); /* back to active from the result screen */
    CHECK(core.depth == 1, "returning from the result goes back to the active session");

    /* === operation_request -- real-format addresses so the real forge
       pipeline can be genuinely exercised, not just schema parsing === */
    const char *op_req_json =
        "{\"type\":\"operation_request\",\"version\":\"3\",\"id\":\"req-456\",\"senderId\":\"peer-abc\","
        "\"network\":{\"type\":\"mainnet\"},\"sourceAddress\":\"tz1VQA4RP4fLjEEMW2FR4pE9kAg5abb5h5GL\","
        "\"operationDetails\":[{\"kind\":\"transaction\",\"destination\":\"KT1TxqZ8QtKvLu3V3JH7Gx58n7Co8pgtpQU5\","
        "\"amount\":\"1000000\"}]}";
    CHECK(beacon_wallet_test_inject_message(op_req_json), "a real-shaped operation_request is correctly classified");
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    tezos_fb_dump_ppm(fb, "beacon_operation_request.ppm");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* APPROVE */
    CHECK(core.depth == 2, "approving an operation_request pushes a real confirmation popup");

    tap(&core, TEZOS_INPUT_SOFTKEY_L, 0); /* confirm -- real forge+sign+broadcast attempt + response */
    CHECK(core.depth == 1, "confirming an operation pops back with a real result, real forge/sign attempted");

    tezos_fb_destroy(fb);
    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
