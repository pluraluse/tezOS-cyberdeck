/*
 * Real STM32F401CCU6 bootstrap target -- proves the FULL real
 * toolchain pipeline (compile -> link -> real flashable ELF/BIN)
 * works, not just individual object files compiling in isolation.
 * Reuses the same, already-tested wire protocol and session-assembly
 * layers as the real F412 target -- both are chip-independent by
 * design, so nothing here is bootstrap-specific except which device
 * header and linker script/startup file this links against.
 *
 * Deliberately does NOT include the RNG safety layer
 * (tezos_signer_rng_core.c/tezos_signer_rng.c) -- the STM32F401 does
 * not have the hardware TRNG peripheral that code assumes at all;
 * linking it here would either fail to build (register block not
 * defined) or, if it somehow did build, read garbage from whatever IS
 * mapped at that address -- a real, live demonstration of the exact
 * silent-failure problem that whole design exists to prevent. This
 * bootstrap target proves the protocol/session/toolchain pipeline
 * only, nothing RNG- or signing-related.
 */
#include "stm32f4xx.h"
#include "../protocol/tezos_signer_apdu.h"
#include "../protocol/tezos_signer_session.h"

static uint8_t uart_read_byte(void) {
    while (!(USART1->SR & USART_SR_RXNE)) { }
    return (uint8_t)(USART1->DR & 0xFF);
}

static void uart_write_byte(uint8_t b) {
    while (!(USART1->SR & USART_SR_TXE)) { }
    USART1->DR = b;
}

int main(void) {
    uint8_t raw[4 + 1 + TEZOS_APDU_MAX_DATA + 1];
    tezos_signer_session_t session;
    tezos_signer_session_reset(&session);

    for (;;) {
        uint16_t len = 0;
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
        uint16_t sw;
        if (cmd.cla != TEZOS_APDU_CLA) {
            sw = TEZOS_APDU_SW_CLA_NOT_SUPPORTED;
        } else if (cmd.ins == TEZOS_APDU_INS_SIGN) {
            sw = tezos_signer_session_append_chunk(&session, &cmd);
            if (sw == TEZOS_APDU_SW_SUCCESS && session.state == TEZOS_SIGNER_SESSION_READY) {
                sw = TEZOS_APDU_SW_INS_NOT_SUPPORTED; /* real, honest placeholder --
                    same as the real F412 target; no signing logic exists yet,
                    and none would belong on this bootstrap chip anyway */
                tezos_signer_session_reset(&session);
            }
        } else {
            sw = TEZOS_APDU_SW_INS_NOT_SUPPORTED;
        }
        uint16_t resp_len = tezos_apdu_build_response(0, 0, sw, resp, sizeof resp);
        for (uint16_t i = 0; i < resp_len; i++) uart_write_byte(resp[i]);
    }
}
