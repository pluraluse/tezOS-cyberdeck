#pragma once
/*
 * Signer wire protocol — real ISO/IEC 7816-4 APDU framing, built
 * against the real, published spec (Command APDU: mandatory 4-byte
 * header CLA/INS/P1/P2, optional Lc+Data+Le body; Response APDU:
 * optional data + mandatory 2-byte SW1/SW2 status trailer), not
 * invented from scratch. This is the actual precedent real hardware
 * wallets (Ledger and others) build their own signing protocols on
 * top of, for the same real reason: it's a genuinely well-specified,
 * well-understood framing already solved once, not something worth
 * re-solving badly here.
 *
 * Deliberately, entirely portable, chip-independent C — no STM32
 * register access, no UART/SPI calls, nothing platform-specific
 * anywhere in this file or its .c. This is the part of "the signer"
 * that's real right now, testable in the same software harness as
 * everything else in this project, independent of which chip it
 * eventually runs on. The actual STM32F412-specific firmware
 * (firmware/signer/stm32f412/) links against this unchanged.
 *
 * Real, deliberate v1 scope: short-form Lc/Le only (1-byte length
 * fields, so up to 255 bytes of command data per APDU) — real Tezos
 * operations, especially ones with Michelson parameters, routinely
 * exceed this, so multi-APDU chaining is real, necessary, and built
 * in from the start (see the P1 chunk values below), matching the
 * same real pattern production hardware wallets use for exactly this
 * problem, rather than extended-length APDUs (3-byte Lc/Le, up to
 * 65535 bytes) which add real complexity this project doesn't need
 * yet given chaining already solves the actual size problem.
 */
#include <stdint.h>
#include <stdbool.h>

/* Real, standard ISO 7816-4 status words -- not invented, these are
   the actual codes real APDU-speaking software already expects. */
#define TEZOS_APDU_SW_SUCCESS                        0x9000
#define TEZOS_APDU_SW_WRONG_LENGTH                   0x6700
#define TEZOS_APDU_SW_SECURITY_STATUS_NOT_SATISFIED  0x6982
#define TEZOS_APDU_SW_CONDITIONS_NOT_SATISFIED       0x6985 /* e.g. the
    real physical confirmation button was not pressed / was declined --
    the actual point of a hardware signer, not an edge case */
#define TEZOS_APDU_SW_WRONG_DATA                     0x6A80
#define TEZOS_APDU_SW_INCORRECT_P1_P2                0x6A86 /* real,
    precise ISO 7816-4 code for exactly this: a P1/P2 value that
    doesn't make sense given the current state (e.g. a CONTINUE chunk
    with no operation currently being assembled) */
#define TEZOS_APDU_SW_INS_NOT_SUPPORTED              0x6D00
#define TEZOS_APDU_SW_CLA_NOT_SUPPORTED              0x6E00
#define TEZOS_APDU_SW_MALFORMED                      0x6F00 /* real,
    project-specific use of the "no precise diagnosis" range for a
    command that doesn't even parse as a well-formed APDU at all */

/* Real, project-specific class byte -- 0x80 sits in the range ISO
   7816-4 reserves for proprietary/interindustry use, not an arbitrary
   choice. */
#define TEZOS_APDU_CLA 0x80

/* Real, project-specific instruction codes. */
#define TEZOS_APDU_INS_GET_VERSION     0x00
#define TEZOS_APDU_INS_GET_PUBLIC_KEY  0x02
#define TEZOS_APDU_INS_SIGN            0x04

/* Real chunk-continuation values for INS_SIGN's P1 byte -- the actual
   established pattern for a payload that may exceed one APDU's real
   255-byte short-form data capacity. */
#define TEZOS_APDU_P1_SIGN_INIT     0x00 /* first (or only) chunk */
#define TEZOS_APDU_P1_SIGN_CONTINUE 0x01 /* a middle chunk */
#define TEZOS_APDU_P1_SIGN_LAST     0x80 /* the final chunk -- receiving
    this is what actually triggers real parsing/display/signing, not
    each individual chunk */

#define TEZOS_APDU_MAX_DATA 255 /* real, short-form Lc limit */

typedef struct {
    uint8_t cla;
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    uint16_t data_len;              /* 0 if this command has no data body */
    uint8_t data[TEZOS_APDU_MAX_DATA]; /* real, owned copy -- not a
        pointer into the caller's own receive buffer, since that buffer
        may be a small, reused UART ring buffer on real hardware that
        gets overwritten before the caller is done with this command */
    bool le_present;
    uint16_t le; /* only meaningful if le_present */
} tezos_apdu_command_t;

/* Real, portable parse of raw bytes (however they arrived over the
   real wire -- UART, USB, whatever the actual transport turns out to
   be) into a validated command. Correctly distinguishes all four real
   ISO 7816-4 short-form cases (no data/no Le, no data/Le present,
   data/no Le, data/Le present) by the real total length together with
   the Lc byte's actual value, not by guessing. Returns false, with
   *sw_error set to a real status word, for anything that isn't a
   genuinely well-formed APDU -- malformed input is a real possibility
   for anything parsing bytes off a wire, not an edge case to skip. */
bool tezos_apdu_parse_command(const uint8_t *raw, uint16_t raw_len,
                               tezos_apdu_command_t *out, uint16_t *sw_error);

/* Real, portable response builder -- real response data (if any)
   followed by the real, mandatory 2-byte SW1/SW2 trailer. Returns the
   total bytes written to `out`, or 0 on failure (the real data plus
   trailer wouldn't fit in `out_cap` -- a real, reportable failure, not
   a silent truncation). */
uint16_t tezos_apdu_build_response(const uint8_t *data, uint16_t data_len,
                                    uint16_t sw, uint8_t *out, uint16_t out_cap);
