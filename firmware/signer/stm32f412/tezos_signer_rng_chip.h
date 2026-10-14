#pragma once
/*
 * Chip-specific entry points into the real STM32F412 hardware TRNG --
 * see ../protocol/tezos_signer_rng.h for the portable, testable
 * recovery logic these wrap, and tezos_signer_rng.c for the real
 * chip-specific implementation (real register access, real CMSIS
 * intrinsics) neither of which can run on anything but the real
 * target.
 */
#include <stdint.h>
#include <stdbool.h>

/* Real, one-time hardware init -- enables the RNG peripheral. */
bool tezos_signer_rng_init(void);

/* Real random word from the real hardware, with the real, documented
   seed-error recovery sequence applied automatically. Returns false
   if the hardware cannot produce a trustworthy word even after that
   recovery attempt -- the only correct response to false is
   tezos_signer_panic_halt(), never a fallback to any other entropy
   source. See ../protocol/tezos_signer_rng.h for the full reasoning. */
bool tezos_signer_rng_read_word(uint32_t *out);

/* The real, irrecoverable fail-safe halt. Disables interrupts and
   spins forever -- recovering requires a real physical power cycle.
   This is the correct, deliberate response to anything that means the
   device cannot currently guarantee it's safe to operate (an RNG
   failure being the motivating, but not the only, real case) —
   continuing to run in a possibly-unsafe state, even "gracefully," is
   the exact failure mode this project's signer firmware is built to
   never have (see the real Coldcard incident this is built against,
   documented in ../protocol/tezos_signer_rng.h). */
void tezos_signer_panic_halt(const char *reason) __attribute__((noreturn));
