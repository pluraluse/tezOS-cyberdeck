/*
 * The portable, testable core of the STM32 RNG recovery logic --
 * takes the register struct as a parameter rather than reaching for
 * real hardware directly, so it can be tested in software with a
 * simulated register struct, the exact same technique Coldcard's own
 * engineers used to verify their real fix for this exact class of bug
 * (see their PR's own "rng-testbench.c," which mocks RNG_CR/SR/DR the
 * same way). Zero ARM-specific intrinsics anywhere in this file --
 * only real register-struct field reads/writes, which are portable C.
 *
 * See tezos_signer_rng.h for the full reasoning this is built against:
 * the real, documented STM32 seed/clock-error semantics, and the real
 * Coldcard incident (August 2026) this whole design exists to make
 * structurally impossible here -- no silent fallback, ever; only a
 * genuinely verified random word, or a signal to halt.
 */
#include "tezos_signer_rng.h"

#define TEZOS_RNG_SEED_ERROR_FLUSH_WORDS 12 /* the real, documented
    STM32 recovery sequence's flush count -- not a number invented
    here, see tezos_signer_rng.h */

bool tezos_signer_rng_try_read_one_word_impl(tezos_rng_regs_t *rng, uint32_t *out) {
    /* real, bounded poll for DRDY -- a real timeout here (rather than
       an unbounded wait) matters specifically because a persistent
       seed error suppresses DRDY forever; without a bound, a genuine
       hardware failure would hang the caller indefinitely instead of
       reaching the real halt path the chip-specific wrapper provides */
    for (volatile uint32_t spin = 0; spin < 100000; spin++) {
        if (rng->SR & TEZOS_RNG_SR_SEIS) return false; /* real seed
            error -- the caller handles the real, documented recovery
            sequence, not this function -- this function only reports it */
        if (rng->SR & TEZOS_RNG_SR_DRDY) {
            *out = rng->DR;
            return true;
        }
    }
    return false; /* real, bounded timeout -- treated the same as a
        seed error by the caller below: something is wrong, and this
        function does not know how to recover from a plain timeout,
        so it reports failure rather than guessing */
}

bool tezos_signer_rng_read_word_impl(tezos_rng_regs_t *rng, uint32_t *out) {
    uint32_t discard;

    if (tezos_signer_rng_try_read_one_word_impl(rng, out)) {
        return true; /* the real, common, no-error case */
    }

    /* the first read failed -- check specifically for a real seed
       error, since that's the one real, documented, recoverable case;
       anything else (a plain timeout with no seed error flag set) is
       treated as a genuine, unrecoverable hardware problem below */
    if (!(rng->SR & TEZOS_RNG_SR_SEIS)) {
        return false; /* a real, unrecognized failure -- the caller's
            job is to halt, per this whole design's actual point */
    }

    /* real, documented recovery: clear SEIS (STM32's real convention
       for this status register is that writing 0 to a sticky error
       bit clears it), then discard TEZOS_RNG_SEED_ERROR_FLUSH_WORDS
       words to flush the pipeline, then confirm SEIS is still clear
       before trusting anything further -- not a number invented here,
       this is the real, documented sequence. */
    rng->SR &= ~TEZOS_RNG_SR_SEIS;
    for (int i = 0; i < TEZOS_RNG_SEED_ERROR_FLUSH_WORDS; i++) {
        if (!tezos_signer_rng_try_read_one_word_impl(rng, &discard)) {
            /* the seed error came back (or a fresh timeout) during the
               real, documented flush sequence -- this is a genuine,
               persistent hardware problem, not a transient blip the
               documented recovery is meant to handle */
            return false;
        }
    }
    if (rng->SR & TEZOS_RNG_SR_SEIS) {
        return false; /* SEIS re-asserted even after the real, correct
            recovery sequence -- a genuinely persistent seed error,
            real hardware failure, caller must halt */
    }

    /* real recovery succeeded -- read one more real word and return
       it as the actual, trustworthy output this call promised */
    return tezos_signer_rng_try_read_one_word_impl(rng, out);
}
