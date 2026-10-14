#include "tezos_signer_session.h"
#include <string.h>

void tezos_signer_session_reset(tezos_signer_session_t *session) {
    session->state = TEZOS_SIGNER_SESSION_IDLE;
    session->operation_len = 0;
    /* the operation buffer's old contents are deliberately not
       zeroed here -- operation_len == 0 makes them logically
       inaccessible to anything reading through the real API, and
       this is a bare-metal target where a real, unnecessary 2KB
       clear on every reset is real, avoidable work; nothing sensitive
       (this buffer holds a forged operation, not key material) makes
       that trade-off wrong here the way it would be for a real key
       buffer */
}

static bool append_bytes(tezos_signer_session_t *session, const uint8_t *data, uint16_t len) {
    if ((uint32_t)session->operation_len + len > TEZOS_SIGNER_MAX_OPERATION_SIZE) {
        return false; /* real, reported overflow -- rejecting cleanly,
            not silently truncating or overflowing the real buffer */
    }
    memcpy(session->operation + session->operation_len, data, len);
    session->operation_len += len;
    return true;
}

uint16_t tezos_signer_session_append_chunk(tezos_signer_session_t *session,
                                            const tezos_apdu_command_t *cmd) {
    bool is_last = (cmd->p1 == TEZOS_APDU_P1_SIGN_LAST);
    bool is_init = (cmd->p1 == TEZOS_APDU_P1_SIGN_INIT);
    bool is_continue = (cmd->p1 == TEZOS_APDU_P1_SIGN_CONTINUE);

    if (!is_last && !is_init && !is_continue) {
        return TEZOS_APDU_SW_INCORRECT_P1_P2; /* a real, genuinely
            unrecognized P1 value -- not one of the three real chunk
            types this protocol defines */
    }

    if (is_continue) {
        /* real rule: CONTINUE is only valid mid-assembly -- there must
           already be a real operation in progress to continue.
           Arriving here from IDLE or READY is a real sequencing error,
           not something to silently accept (e.g. treating a stray
           CONTINUE as the start of a new operation would risk mixing
           unrelated bytes together). */
        if (session->state != TEZOS_SIGNER_SESSION_RECEIVING) {
            return TEZOS_APDU_SW_INCORRECT_P1_P2;
        }
        if (!append_bytes(session, cmd->data, cmd->data_len)) {
            tezos_signer_session_reset(session); /* real, safe recovery --
                don't leave a half-overflowed, inconsistent buffer sitting
                around after a rejected chunk */
            return TEZOS_APDU_SW_WRONG_LENGTH;
        }
        /* still RECEIVING -- more chunks (CONTINUE or LAST) expected */
        return TEZOS_APDU_SW_SUCCESS;
    }

    if (is_init) {
        /* INIT always starts fresh, regardless of current state -- a
           real, deliberate safety property: never let a fresh INIT
           silently append onto (or otherwise coexist with) bytes from
           a previous, possibly-unrelated sequence. */
        tezos_signer_session_reset(session);
        if (!append_bytes(session, cmd->data, cmd->data_len)) {
            tezos_signer_session_reset(session);
            return TEZOS_APDU_SW_WRONG_LENGTH;
        }
        session->state = TEZOS_SIGNER_SESSION_RECEIVING;
        return TEZOS_APDU_SW_SUCCESS;
    }

    /* is_last: two real, distinct cases, not one. If we're already
       legitimately RECEIVING (a real INIT started this sequence and
       it hasn't finished yet), this LAST is the genuine final chunk
       of that in-progress operation -- it must APPEND to the already-
       accumulated bytes, not discard them. Only if we're NOT
       currently RECEIVING (IDLE, or a stale/unresolved READY left over
       from a previous operation) does LAST mean "this one APDU is the
       entire operation" and reset first. Conflating these two real
       cases was a genuine bug caught by this module's own test suite:
       an earlier version always reset on LAST, silently discarding a
       real, legitimately-in-progress multi-chunk operation's already-
       received bytes right before completing it. */
    if (session->state != TEZOS_SIGNER_SESSION_RECEIVING) {
        tezos_signer_session_reset(session);
    }
    if (!append_bytes(session, cmd->data, cmd->data_len)) {
        tezos_signer_session_reset(session);
        return TEZOS_APDU_SW_WRONG_LENGTH;
    }
    session->state = TEZOS_SIGNER_SESSION_READY;
    return TEZOS_APDU_SW_SUCCESS;
}
