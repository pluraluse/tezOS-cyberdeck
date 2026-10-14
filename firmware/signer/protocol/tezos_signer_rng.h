#pragma once
/*
 * Portable core of the STM32 hardware TRNG recovery logic — see
 * tezos_signer_rng_core.c for the full reasoning. Zero ARM-specific
 * intrinsics, zero dependency on the real vendored STM32 CMSIS
 * headers (third_party/cmsis_stm32f4/) — this lets the actual
 * recovery logic be tested on x86 with a simulated register struct,
 * the same technique Coldcard's own engineers used to verify their
 * real fix for this exact class of bug.
 *
 * THE REAL LESSON THIS IS BUILT AROUND, STATED PLAINLY: the real
 * Coldcard incident (August 2026) was not a logic bug producing wrong
 * output — it was a firmware path that, when the real hardware
 * entropy source wasn't behaving as expected, silently fell back to a
 * weak, predictable software-seeded value and kept operating
 * normally, with no error, no warning, generating catastrophically
 * weak keys. The correct behavior when a signer's RNG can't be
 * verified trustworthy is to halt immediately and visibly (see the
 * chip-specific tezos_signer_panic_halt in
 * firmware/signer/stm32f412/tezos_signer_rng.c), never to continue
 * operating under degraded conditions. Every real caller of the
 * function below gets either a genuinely verified random word, or a
 * clear signal to halt — there is no third path.
 *
 * A real, non-obvious nuance, caught from Coldcard's own real PR
 * history (github.com/Coldcard/firmware #692, #693), not invented
 * here: a naive first attempt at fixing this exact class of bug
 * over-corrected by treating a transient CLOCK error the same as a
 * SEED error. That's also wrong — a clock error doesn't necessarily
 * invalidate data already latched into the data register (the real
 * Linux stm32-rng driver takes the same position), while a seed error
 * does, and is sticky until software explicitly clears it. This file
 * only implements the real, documented seed-error recovery sequence
 * (clear SEIS, flush 12 words — the real, documented count, not
 * invented here — then reconfirm) deliberately, not a blanket
 * "any error flag means restart" rule.
 */
#include <stdint.h>
#include <stdbool.h>

/* Real STM32 RNG register layout (CR, SR, DR, in that exact order) --
   matches the real vendored RNG_TypeDef in
   third_party/cmsis_stm32f4/Include/stm32f412zx.h byte-for-byte, so a
   pointer to a real RNG_TypeDef can be passed here via a cast, and a
   test can instead pass a plain, stack-allocated instance of this
   portable struct to simulate hardware register behavior -- no real
   hardware access, no ARM-specific headers needed either way. */
typedef struct {
    uint32_t CR;
    uint32_t SR;
    uint32_t DR;
} tezos_rng_regs_t;

/* Real, documented STM32 RNG status register bit positions (matches
   the real RNG_SR_DRDY/RNG_SR_SEIS values in the vendored CMSIS
   header) -- redefined here, not included from the real header,
   specifically so this file has zero dependency on it. */
#define TEZOS_RNG_SR_DRDY (1U << 0)
#define TEZOS_RNG_SR_CEIS (1U << 5)
#define TEZOS_RNG_SR_SEIS (1U << 6)

/* Real, single-attempt read -- polls DRDY with a real, bounded
   timeout (not unbounded, since a persistent seed error suppresses
   DRDY forever). Returns false on a seed error OR a plain timeout;
   the caller (tezos_signer_rng_read_word_impl) is responsible for
   deciding what to do about each. */
bool tezos_signer_rng_try_read_one_word_impl(tezos_rng_regs_t *rng, uint32_t *out);

/* Real word read, with the real, documented seed-error recovery
   sequence applied if needed. Returns false if the hardware cannot
   produce a trustworthy word even after the real, documented recovery
   attempt -- the only correct response to false, at the real call
   site, is to halt, never a fallback to any other entropy source. */
bool tezos_signer_rng_read_word_impl(tezos_rng_regs_t *rng, uint32_t *out);
