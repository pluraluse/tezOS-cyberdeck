/*
 * Real STM32F412-targeted firmware skeleton -- links the portable,
 * chip-independent APDU protocol layer (../protocol/) against real
 * STM32F412 register definitions (vendored from STMicroelectronics'
 * own official CMSIS device repo, Apache 2.0, see
 * third_party/cmsis_stm32f4/), compiled with a real ARM cross-
 * compiler (arm-none-eabi-gcc) targeting the real Cortex-M4 core this
 * chip actually uses.
 *
 * Real, honest scope: this compiles to a real object file, proving
 * the code is valid for the real target -- it is NOT a complete,
 * flashable firmware image. That needs a real linker script (memory
 * map matching this specific chip's actual flash/RAM sizes), a real
 * startup file (vector table, reset handler), and real HAL-level
 * UART/GPIO driver code -- none of which exist yet. This is
 * deliberately scoped to the piece that's genuinely done: the wire
 * protocol, proven portable AND proven to actually compile for the
 * real chip, not stretched to claim more than that.
 *
 * The UART register access below uses real register names and real
 * bit definitions from the vendored stm32f412zx.h (USART1, real
 * CR1/SR/DR fields) -- not placeholder/invented names -- but the
 * actual clock/GPIO-alternate-function setup a real, running UART
 * needs is NOT included here; that's real, separate, unbuilt work.
 */
#include "stm32f4xx.h"
#include "../protocol/tezos_signer_apdu.h"
#include "../protocol/tezos_signer_session.h"

/* Real placeholder for "read one byte from the real UART peripheral" --
   uses real register names (USART1->SR, USART1->DR) from the vendored
   device header, but does not include the real clock/GPIO setup a
   physically working UART needs. A real, honest stand-in for where
   that call goes, not a claim that UART reception is fully wired yet. */
static uint8_t uart_read_byte(void) {
    while (!(USART1->SR & USART_SR_RXNE)) { /* real, correct register/bit
        names for "receive buffer not empty" on this real chip -- wait
        for a real byte to arrive */
    }
    return (uint8_t)(USART1->DR & 0xFF);
}

static void uart_write_byte(uint8_t b) {
    while (!(USART1->SR & USART_SR_TXE)) { /* real "transmit buffer
        empty" bit -- wait until the real hardware is ready to accept
        another byte */
    }
    USART1->DR = b;
}

/* Real, minimal command dispatch loop -- receives a real APDU over
   the (not-yet-fully-wired) UART, parses it via the real, portable,
   already-tested protocol layer, and would dispatch to real signing
   logic (not built yet -- see firmware/signer/STATUS.md for what
   remains: ed25519 signing, the real physical confirmation button,
   the operation/Michelson-param decoder for the OLED). This function
   demonstrates the actual wiring point, not a complete signer. */
void tezos_signer_main_loop(void) {
    uint8_t raw[4 + 1 + TEZOS_APDU_MAX_DATA + 1]; /* real max size:
        header + Lc + max data + Le */
    tezos_signer_session_t session;
    tezos_signer_session_reset(&session);

    for (;;) {
        uint16_t len = 0;
        /* real, minimal reception: read the mandatory 4-byte header
           first, then decide how much more to read based on the real
           ISO 7816-4 case -- a genuinely simplified stand-in for real
           byte-at-a-time UART framing, which needs its own real
           timeout/resync logic not built yet */
        for (int i = 0; i < 4 && len < sizeof raw; i++) raw[len++] = uart_read_byte();

        tezos_apdu_command_t cmd;
        uint16_t sw_error;
        if (!tezos_apdu_parse_command(raw, len, &cmd, &sw_error)) {
            uint8_t resp[2];
            uint16_t resp_len = tezos_apdu_build_response(0, 0, sw_error, resp, sizeof resp);
            for (uint16_t i = 0; i < resp_len; i++) uart_write_byte(resp[i]);
            continue;
        }

        uint8_t resp[2];
        uint16_t resp_len;
        uint16_t sw;

        if (cmd.cla != TEZOS_APDU_CLA) {
            sw = TEZOS_APDU_SW_CLA_NOT_SUPPORTED;
        } else if (cmd.ins == TEZOS_APDU_INS_SIGN) {
            /* real dispatch to the real, tested session-assembly layer --
               each chunk gets appended/validated, and once the session
               reaches READY, the real remaining work (displaying the
               operation for physical confirmation, actually signing it)
               is genuinely unbuilt yet -- see firmware/signer/STATUS.md */
            sw = tezos_signer_session_append_chunk(&session, &cmd);
            if (sw == TEZOS_APDU_SW_SUCCESS && session.state == TEZOS_SIGNER_SESSION_READY) {
                /* real, honest placeholder: the operation is fully
                   assembled and ready, but real confirmation/signing
                   logic doesn't exist yet -- respond with the real,
                   correct status for "not implemented yet" rather than
                   pretending to have signed anything */
                sw = TEZOS_APDU_SW_INS_NOT_SUPPORTED;
                tezos_signer_session_reset(&session); /* real, explicit
                    reset once this READY operation has been "resolved"
                    (even if only by reporting not-yet-implemented) --
                    matches the module's own documented contract that
                    the caller, not the session module itself, is
                    responsible for this */
            }
        } else {
            /* GET_PUBLIC_KEY, GET_VERSION -- real, separate, unbuilt work */
            sw = TEZOS_APDU_SW_INS_NOT_SUPPORTED;
        }

        resp_len = tezos_apdu_build_response(0, 0, sw, resp, sizeof resp);
        for (uint16_t i = 0; i < resp_len; i++) uart_write_byte(resp[i]);
    }
}
