/* Real operation-assembly session state machine tests -- covers both
   real ways to complete an operation (single-chunk LAST from idle,
   and a real multi-chunk INIT/CONTINUE/LAST sequence), real
   out-of-sequence rejection, the real "fresh INIT/LAST always resets,
   never mixes stale bytes" safety property, and real buffer-overflow
   rejection. */
#include "../firmware/signer/protocol/tezos_signer_session.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

static tezos_apdu_command_t make_chunk(uint8_t p1, const uint8_t *data, uint16_t len) {
    tezos_apdu_command_t cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.cla = TEZOS_APDU_CLA;
    cmd.ins = TEZOS_APDU_INS_SIGN;
    cmd.p1 = p1;
    cmd.data_len = len;
    memcpy(cmd.data, data, len);
    return cmd;
}

int main(void) {
    tezos_signer_session_t session;
    tezos_signer_session_reset(&session);
    CHECK(session.state == TEZOS_SIGNER_SESSION_IDLE && session.operation_len == 0,
          "a fresh reset starts at IDLE with zero length");

    /* === real single-chunk operation: LAST directly from IDLE === */
    uint8_t small_op[] = { 0x01, 0x02, 0x03, 0x04 };
    tezos_apdu_command_t chunk = make_chunk(TEZOS_APDU_P1_SIGN_LAST, small_op, sizeof small_op);
    uint16_t sw = tezos_signer_session_append_chunk(&session, &chunk);
    CHECK(sw == TEZOS_APDU_SW_SUCCESS, "a single LAST chunk from idle is accepted");
    CHECK(session.state == TEZOS_SIGNER_SESSION_READY, "a single-chunk operation correctly reaches READY directly");
    CHECK(session.operation_len == 4 && memcmp(session.operation, small_op, 4) == 0,
          "the real, complete small operation's bytes are correct");

    /* === real multi-chunk operation: INIT, CONTINUE, CONTINUE, LAST === */
    tezos_signer_session_reset(&session);
    uint8_t part1[] = { 0xAA, 0xAA };
    uint8_t part2[] = { 0xBB, 0xBB, 0xBB };
    uint8_t part3[] = { 0xCC };
    uint8_t part4[] = { 0xDD, 0xDD, 0xDD, 0xDD };

    chunk = make_chunk(TEZOS_APDU_P1_SIGN_INIT, part1, sizeof part1);
    sw = tezos_signer_session_append_chunk(&session, &chunk);
    CHECK(sw == TEZOS_APDU_SW_SUCCESS && session.state == TEZOS_SIGNER_SESSION_RECEIVING,
          "INIT chunk accepted, session correctly moves to RECEIVING");

    chunk = make_chunk(TEZOS_APDU_P1_SIGN_CONTINUE, part2, sizeof part2);
    sw = tezos_signer_session_append_chunk(&session, &chunk);
    CHECK(sw == TEZOS_APDU_SW_SUCCESS && session.state == TEZOS_SIGNER_SESSION_RECEIVING,
          "first CONTINUE chunk accepted, stays in RECEIVING");

    chunk = make_chunk(TEZOS_APDU_P1_SIGN_CONTINUE, part3, sizeof part3);
    sw = tezos_signer_session_append_chunk(&session, &chunk);
    CHECK(sw == TEZOS_APDU_SW_SUCCESS, "second CONTINUE chunk accepted");

    chunk = make_chunk(TEZOS_APDU_P1_SIGN_LAST, part4, sizeof part4);
    sw = tezos_signer_session_append_chunk(&session, &chunk);
    CHECK(sw == TEZOS_APDU_SW_SUCCESS && session.state == TEZOS_SIGNER_SESSION_READY,
          "LAST chunk accepted, session correctly reaches READY");
    CHECK(session.operation_len == 2 + 3 + 1 + 4, "the real, complete multi-chunk operation has the correct total length");
    uint8_t expected[] = { 0xAA,0xAA, 0xBB,0xBB,0xBB, 0xCC, 0xDD,0xDD,0xDD,0xDD };
    CHECK(memcmp(session.operation, expected, sizeof expected) == 0,
          "all four real chunks were concatenated in the correct order, byte for byte");

    /* === real out-of-sequence rejection === */
    tezos_signer_session_reset(&session);
    chunk = make_chunk(TEZOS_APDU_P1_SIGN_CONTINUE, part1, sizeof part1);
    sw = tezos_signer_session_append_chunk(&session, &chunk);
    CHECK(sw == TEZOS_APDU_SW_INCORRECT_P1_P2, "a CONTINUE with nothing to continue (from IDLE) is correctly rejected");
    CHECK(session.state == TEZOS_SIGNER_SESSION_IDLE, "the rejected CONTINUE didn't change session state");

    /* CONTINUE after already reaching READY (previous op never explicitly resolved) */
    tezos_signer_session_reset(&session);
    chunk = make_chunk(TEZOS_APDU_P1_SIGN_LAST, small_op, sizeof small_op);
    tezos_signer_session_append_chunk(&session, &chunk);
    CHECK(session.state == TEZOS_SIGNER_SESSION_READY, "setup: reached READY");
    chunk = make_chunk(TEZOS_APDU_P1_SIGN_CONTINUE, part1, sizeof part1);
    sw = tezos_signer_session_append_chunk(&session, &chunk);
    CHECK(sw == TEZOS_APDU_SW_INCORRECT_P1_P2, "a CONTINUE arriving while READY (nothing being assembled) is correctly rejected");

    /* an unrecognized P1 value entirely */
    tezos_signer_session_reset(&session);
    chunk = make_chunk(0x42, small_op, sizeof small_op);
    sw = tezos_signer_session_append_chunk(&session, &chunk);
    CHECK(sw == TEZOS_APDU_SW_INCORRECT_P1_P2, "a genuinely unrecognized P1 value is correctly rejected");

    /* === real safety property: fresh INIT/LAST always resets, never mixes stale bytes === */
    tezos_signer_session_reset(&session);
    chunk = make_chunk(TEZOS_APDU_P1_SIGN_INIT, part1, sizeof part1);
    tezos_signer_session_append_chunk(&session, &chunk); /* start an operation, leave it unfinished (RECEIVING) */
    CHECK(session.state == TEZOS_SIGNER_SESSION_RECEIVING, "setup: an incomplete sequence is left mid-assembly");

    /* a fresh INIT arrives -- real, correct behavior: reset and start clean,
       not append onto (or otherwise mix with) the stale, incomplete bytes */
    chunk = make_chunk(TEZOS_APDU_P1_SIGN_INIT, part4, sizeof part4);
    sw = tezos_signer_session_append_chunk(&session, &chunk);
    CHECK(sw == TEZOS_APDU_SW_SUCCESS, "a fresh INIT while mid-assembly of a stale sequence is accepted");
    CHECK(session.operation_len == sizeof part4 && memcmp(session.operation, part4, sizeof part4) == 0,
          "the fresh INIT correctly discarded the stale, unfinished bytes -- no mixing of two different operations' data");

    /* a LAST arriving while a real, legitimately-started sequence is
       mid-assembly (INIT was already sent) must complete THAT
       sequence -- appending, not discarding it. The protocol has no
       way to distinguish "a stale INIT the host gave up on" from "a
       real INIT partway through" once RECEIVING; treating LAST as
       "always reset" here would silently drop part1's bytes with no
       error, which is worse than requiring an explicit fresh INIT to
       truly restart. This is also what an earlier version of this
       module got wrong, caught by this exact case. */
    tezos_signer_session_reset(&session);
    chunk = make_chunk(TEZOS_APDU_P1_SIGN_INIT, part1, sizeof part1);
    tezos_signer_session_append_chunk(&session, &chunk);
    chunk = make_chunk(TEZOS_APDU_P1_SIGN_LAST, part2, sizeof part2);
    sw = tezos_signer_session_append_chunk(&session, &chunk);
    CHECK(sw == TEZOS_APDU_SW_SUCCESS && session.state == TEZOS_SIGNER_SESSION_READY,
          "a LAST arriving after a real, legitimate INIT correctly completes that in-progress sequence");
    CHECK(session.operation_len == sizeof part1 + sizeof part2 &&
          memcmp(session.operation, part1, sizeof part1) == 0 &&
          memcmp(session.operation + sizeof part1, part2, sizeof part2) == 0,
          "part1's bytes were correctly preserved and part2 appended -- not silently discarded");

    /* === real buffer-overflow rejection === */
    tezos_signer_session_reset(&session);
    uint8_t big_chunk[255]; memset(big_chunk, 0x11, sizeof big_chunk);
    chunk = make_chunk(TEZOS_APDU_P1_SIGN_INIT, big_chunk, sizeof big_chunk);
    for (int i = 0; i < 8; i++) { /* 8 * 255 = 2040, still under the 2048 real bound */
        chunk.p1 = (i == 0) ? TEZOS_APDU_P1_SIGN_INIT : TEZOS_APDU_P1_SIGN_CONTINUE;
        sw = tezos_signer_session_append_chunk(&session, &chunk);
        CHECK(sw == TEZOS_APDU_SW_SUCCESS, "accumulating real chunks under the real bound succeeds");
    }
    /* one more chunk would push past TEZOS_SIGNER_MAX_OPERATION_SIZE (2048) */
    chunk.p1 = TEZOS_APDU_P1_SIGN_CONTINUE;
    sw = tezos_signer_session_append_chunk(&session, &chunk);
    CHECK(sw == TEZOS_APDU_SW_WRONG_LENGTH, "a chunk that would push the real total past the buffer bound is correctly rejected, not silently overflowed");
    CHECK(session.state == TEZOS_SIGNER_SESSION_IDLE, "a rejected overflow correctly resets the session, not leaving a half-valid state sitting around");

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
