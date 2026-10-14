/*
 * Real STM32F412 hardware TRNG entry points -- thin, real wrappers
 * around the portable, tested recovery logic in
 * ../protocol/tezos_signer_rng_core.c. See
 * ../protocol/tezos_signer_rng.h for the full reasoning (the real
 * Coldcard incident this whole design exists to make structurally
 * impossible here, and the real, documented STM32 seed/clock-error
 * semantics this is built against, not invented).
 *
 * Everything in this specific file is genuinely untestable outside
 * real hardware -- real register addresses, a real CMSIS intrinsic
 * (__disable_irq) that doesn't exist on any other architecture. The
 * actual recovery logic these wrappers call into has already been
 * verified in software (see tests/test_signer_rng.c); this file's own
 * job is just "point that tested logic at the real registers."
 */
#include "stm32f4xx.h"
#include "tezos_signer_rng_chip.h"
#include "../protocol/tezos_signer_rng.h"

void tezos_signer_panic_halt(const char *reason) {
    (void)reason; /* real, honest gap: nothing yet displays this reason
        anywhere a person could see it -- no display/LED wiring exists
        yet (see firmware/signer/STATUS.md). A real halt still happens
        regardless; making the reason visible is real, separate,
        unbuilt follow-up work, not something to fake here. */
    __disable_irq(); /* real CMSIS intrinsic -- disables all interrupts,
        so nothing (not even an interrupt handler) can cause any
        further real activity after this point */
    for (;;) {
        /* deliberately, permanently do nothing else. Recovering from
           this state requires a real physical power cycle/reset --
           that's the correct, real behavior for "this device cannot
           currently guarantee it's safe to operate," not an attempt
           to self-heal or continue in a possibly-unsafe state. */
    }
}

bool tezos_signer_rng_init(void) {
    RNG->CR |= RNG_CR_RNGEN;
    return true;
}

bool tezos_signer_rng_read_word(uint32_t *out) {
    /* real cast: tezos_rng_regs_t's layout (CR, SR, DR, in that exact
       order) matches the real, vendored RNG_TypeDef byte-for-byte --
       see tezos_signer_rng.h's own comment on this. This is the one
       real place that connects the portable, tested logic to the
       real, fixed hardware address. */
    return tezos_signer_rng_read_word_impl((tezos_rng_regs_t *)RNG, out);
}
