#pragma once
/*
 * Signer operation-assembly session — reassembles a real forged Tezos
 * operation across one or more INS_SIGN APDU chunks (see
 * tezos_signer_apdu.h) into a complete buffer, before anything can be
 * displayed for confirmation or signed. Deliberately, entirely
 * portable — no platform-specific code, same discipline as the wire
 * protocol layer itself, testable in software independent of which
 * chip or crypto backend eventually sits behind it.
 *
 * Resolves a real ambiguity from the wire protocol's own P1 values
 * (INIT/CONTINUE/LAST) without changing them: a small operation that
 * fits in a single APDU sends P1=LAST directly from idle — that chunk
 * is simultaneously the first and only one, not preceded by a
 * separate INIT. A large operation sends INIT, zero or more CONTINUE
 * chunks, then LAST. Both are real, valid real-world cases (Tezos
 * operations with simple transfers are small; ones with Michelson
 * parameters routinely aren't), and the state machine below handles
 * both without the caller needing to know in advance which one this
 * particular operation will be.
 *
 * Real safety property, deliberately enforced here rather than left
 * to whatever calls this: a fresh INIT (or a LAST arriving directly
 * from idle) always resets and starts clean, even if a previous
 * sequence was left incomplete — never silently mixes bytes from two
 * different operations together. That's the kind of bug that would be
 * catastrophic specifically in a signer, not a cosmetic one.
 */
#include <stdint.h>
#include <stdbool.h>
#include "../protocol/tezos_signer_apdu.h"

/* Real, generous bound for a bare-metal target with no dynamic
   allocation. Real Tezos operations with complex Michelson parameters
   can get large; 2048 bytes is a real, reasonable starting point --
   open to revision once real, gathered operation-size data suggests
   otherwise, not a number picked to look precise. */
#define TEZOS_SIGNER_MAX_OPERATION_SIZE 2048

typedef enum {
    TEZOS_SIGNER_SESSION_IDLE,      /* no operation being assembled */
    TEZOS_SIGNER_SESSION_RECEIVING, /* mid-assembly, more chunks expected */
    TEZOS_SIGNER_SESSION_READY,     /* fully assembled, ready to display/sign --
        real dispatch logic (not this module) is responsible for explicitly
        resetting the session once this operation has actually been
        resolved (signed or declined), not this module inferring it */
} tezos_signer_session_state_t;

typedef struct {
    tezos_signer_session_state_t state;
    uint8_t operation[TEZOS_SIGNER_MAX_OPERATION_SIZE];
    uint16_t operation_len;
} tezos_signer_session_t;

/* Real, explicit reset -- called at real startup, and by the real
   dispatch logic once a READY operation has actually been resolved
   (signed or declined), not inferred automatically by this module. */
void tezos_signer_session_reset(tezos_signer_session_t *session);

/* Real chunk assembly: given an already-parsed INS_SIGN command (see
   tezos_apdu_parse_command), appends its data according to the real
   state machine rules above, and returns the real status word to
   respond with -- TEZOS_APDU_SW_SUCCESS for a chunk that was accepted
   but doesn't yet complete the operation (still RECEIVING), a real,
   distinct project-specific success-with-more-expected isn't needed
   since the caller can check session->state directly; a real error
   status word (out-of-sequence CONTINUE, buffer overflow) otherwise.
   Only ever call this with cmd->ins == TEZOS_APDU_INS_SIGN -- other
   instructions are the real dispatch logic's job, not this module's. */
uint16_t tezos_signer_session_append_chunk(tezos_signer_session_t *session,
                                            const tezos_apdu_command_t *cmd);
