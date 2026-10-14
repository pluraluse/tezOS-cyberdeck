/* Real ISO 7816-4 APDU framing tests -- covers all four real short-form
   cases (case 1: no data/no Le; case 2: no data/Le present; case 3:
   data/no Le; case 4: data/Le present), real malformed-input rejection,
   and real response building, including the truncation-safety check. */
#include "../firmware/signer/protocol/tezos_signer_apdu.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    tezos_apdu_command_t cmd;
    uint16_t sw_error;

    /* === real case 1: no data, no Le === */
    uint8_t case1[] = { TEZOS_APDU_CLA, TEZOS_APDU_INS_GET_VERSION, 0x00, 0x00 };
    CHECK(tezos_apdu_parse_command(case1, sizeof case1, &cmd, &sw_error), "case 1 (bare 4-byte header) parses successfully");
    CHECK(cmd.cla == TEZOS_APDU_CLA && cmd.ins == TEZOS_APDU_INS_GET_VERSION, "case 1: CLA/INS extracted correctly");
    CHECK(cmd.data_len == 0 && !cmd.le_present, "case 1: correctly has no data and no Le");

    /* === real case 2: no data, Le present === */
    uint8_t case2[] = { TEZOS_APDU_CLA, TEZOS_APDU_INS_GET_PUBLIC_KEY, 0x00, 0x00, 0x20 };
    CHECK(tezos_apdu_parse_command(case2, sizeof case2, &cmd, &sw_error), "case 2 (header + Le byte) parses successfully");
    CHECK(cmd.data_len == 0 && cmd.le_present && cmd.le == 0x20, "case 2: correctly has no data, real Le value extracted");

    /* real Le=0x00 means 256, not literal zero -- the real short-form convention */
    uint8_t case2_zero_le[] = { TEZOS_APDU_CLA, TEZOS_APDU_INS_GET_PUBLIC_KEY, 0x00, 0x00, 0x00 };
    CHECK(tezos_apdu_parse_command(case2_zero_le, sizeof case2_zero_le, &cmd, &sw_error) && cmd.le == 256,
          "a real Le byte of 0x00 correctly means 256, not literal zero");

    /* === real case 3: data, no Le === */
    uint8_t case3[] = { TEZOS_APDU_CLA, TEZOS_APDU_INS_SIGN, TEZOS_APDU_P1_SIGN_INIT, 0x00,
                        0x04, 0xDE, 0xAD, 0xBE, 0xEF };
    CHECK(tezos_apdu_parse_command(case3, sizeof case3, &cmd, &sw_error), "case 3 (Lc + data, no Le) parses successfully");
    CHECK(cmd.data_len == 4 && memcmp(cmd.data, "\xDE\xAD\xBE\xEF", 4) == 0 && !cmd.le_present,
          "case 3: real data bytes extracted correctly, no Le present");
    CHECK(cmd.p1 == TEZOS_APDU_P1_SIGN_INIT, "case 3: P1 chunk-type byte extracted correctly");

    /* === real case 4: data + Le === */
    uint8_t case4[] = { TEZOS_APDU_CLA, TEZOS_APDU_INS_SIGN, TEZOS_APDU_P1_SIGN_LAST, 0x00,
                        0x03, 0x01, 0x02, 0x03, 0x40 };
    CHECK(tezos_apdu_parse_command(case4, sizeof case4, &cmd, &sw_error), "case 4 (Lc + data + Le) parses successfully");
    CHECK(cmd.data_len == 3 && memcmp(cmd.data, "\x01\x02\x03", 3) == 0, "case 4: real data bytes extracted correctly");
    CHECK(cmd.le_present && cmd.le == 0x40, "case 4: real Le value extracted correctly, after the real data");
    CHECK(cmd.p1 == TEZOS_APDU_P1_SIGN_LAST, "case 4: the LAST-chunk P1 value round-trips correctly");

    /* === real, maximum-size data (255 bytes, the real short-form Lc limit) === */
    uint8_t max_data_cmd[4 + 1 + 255];
    max_data_cmd[0] = TEZOS_APDU_CLA; max_data_cmd[1] = TEZOS_APDU_INS_SIGN;
    max_data_cmd[2] = TEZOS_APDU_P1_SIGN_CONTINUE; max_data_cmd[3] = 0x00;
    max_data_cmd[4] = 255;
    for (int i = 0; i < 255; i++) max_data_cmd[5 + i] = (uint8_t)i;
    CHECK(tezos_apdu_parse_command(max_data_cmd, sizeof max_data_cmd, &cmd, &sw_error),
          "the real maximum short-form data length (255 bytes) parses correctly");
    CHECK(cmd.data_len == 255 && cmd.data[254] == 254, "all 255 real data bytes preserved correctly, including the last one");

    /* === real malformed-input rejection === */
    uint8_t too_short[] = { TEZOS_APDU_CLA, TEZOS_APDU_INS_SIGN, 0x00 }; /* only 3 bytes -- not even a full header */
    CHECK(!tezos_apdu_parse_command(too_short, sizeof too_short, &cmd, &sw_error) && sw_error == TEZOS_APDU_SW_MALFORMED,
          "a real, too-short input (not even a full 4-byte header) is correctly rejected as malformed");

    uint8_t bad_length[] = { TEZOS_APDU_CLA, TEZOS_APDU_INS_SIGN, 0x00, 0x00, 0x05, 0x01, 0x02 };
    /* claims Lc=5 but only 2 real data bytes follow -- doesn't match case 3 or case 4 */
    CHECK(!tezos_apdu_parse_command(bad_length, sizeof bad_length, &cmd, &sw_error) && sw_error == TEZOS_APDU_SW_MALFORMED,
          "a real length mismatch (Lc claims more data than is actually present) is correctly rejected");

    uint8_t extended_lc[] = { TEZOS_APDU_CLA, TEZOS_APDU_INS_SIGN, 0x00, 0x00, 0x00, 0x01, 0x02 };
    /* Lc=0x00 would mean extended-length APDUs in full ISO 7816-4 -- this project's
       real v1 scope deliberately doesn't support that (see the header's own reasoning) */
    CHECK(!tezos_apdu_parse_command(extended_lc, sizeof extended_lc, &cmd, &sw_error) && sw_error == TEZOS_APDU_SW_MALFORMED,
          "an extended-length-style Lc=0x00 is correctly rejected -- real, disclosed v1 scope limit, not silently misparsed");

    /* === real response building === */
    uint8_t response_buf[300];
    uint8_t fake_pubkey[32]; memset(fake_pubkey, 0xAB, sizeof fake_pubkey);
    uint16_t resp_len = tezos_apdu_build_response(fake_pubkey, sizeof fake_pubkey, TEZOS_APDU_SW_SUCCESS,
                                                  response_buf, sizeof response_buf);
    CHECK(resp_len == 34, "a real 32-byte response correctly becomes 34 bytes (32 data + 2-byte SW trailer)");
    CHECK(response_buf[32] == 0x90 && response_buf[33] == 0x00, "the real SW1/SW2 trailer bytes are correct for SUCCESS (0x9000)");
    CHECK(memcmp(response_buf, fake_pubkey, 32) == 0, "the real response data itself is preserved correctly before the trailer");

    /* real response with no data (e.g. a real error response) */
    uint16_t err_resp_len = tezos_apdu_build_response(NULL, 0, TEZOS_APDU_SW_CONDITIONS_NOT_SATISFIED,
                                                       response_buf, sizeof response_buf);
    CHECK(err_resp_len == 2 && response_buf[0] == 0x69 && response_buf[1] == 0x85,
          "a real no-data error response is exactly the 2-byte trailer, correct SW for 'conditions not satisfied' (e.g. user declined)");

    /* real truncation-safety: response wouldn't fit the real output buffer */
    uint8_t tiny_buf[5];
    uint16_t truncated = tezos_apdu_build_response(fake_pubkey, sizeof fake_pubkey, TEZOS_APDU_SW_SUCCESS,
                                                    tiny_buf, sizeof tiny_buf);
    CHECK(truncated == 0, "a response that wouldn't fit the real output buffer is correctly reported as a failure (0), not silently truncated -- a truncated signature silently reported as complete would be a genuinely dangerous bug");

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
