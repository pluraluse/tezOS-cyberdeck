# Status: wire protocol real and tested; signing logic not started

Chip decision: **STM32F412** (over the cheaper F401/F411, specifically
for its real hardware TRNG — see the signer-chip research in this
project's history).

**Real bootstrap milestone**: an STM32F401CCU6 (Black Pill board, on
hand now, real F412 hardware still on order) was used to prove the
*full* toolchain pipeline for the first time — real linker script,
real startup file, a real link producing a genuinely valid, flashable
`.bin` image (verified via `file`, `objdump`, and reading the actual
disassembly by hand, not just "the linker didn't error"). Reuses the
same wire protocol and session layers unchanged. Deliberately does NOT
include the RNG safety layer — F401 lacks that hardware peripheral
entirely. See `docs/build-notes/stm32f401-bootstrap-build-notes.md`.
Never actually flashed to real hardware yet.

**Done, real, tested**: the real, portable ISO 7816-4 APDU wire protocol
(`protocol/tezos_signer_apdu.h`/`.c`) — 22 checks. The real
operation-assembly session layer (`protocol/tezos_signer_session.h`/
`.c`) — 31 checks, including a real logic bug this module's own tests
caught. **A real hardware RNG safety layer**
(`protocol/tezos_signer_rng_core.c` + `stm32f412/tezos_signer_rng.c`),
built directly against the real, ongoing August 2026 Coldcard incident
and Coldcard's own real firmware fix — every caller gets a genuinely
verified random word or a signal to halt, never a silent fallback. 8
checks, using the same register-mocking technique Coldcard's own
engineers used to verify their fix. All three modules cross-compile
cleanly for the real STM32F412/Cortex-M4 target, and `main.c`
genuinely dispatches `INS_SIGN` through the session layer. See
`docs/build-notes/signer-protocol-build-notes.md` for the full
writeup.

**Not started**: ed25519 signing, the real physical confirmation
button, the operation/Michelson-param decoder for the OLED, real
UART/GPIO clock configuration, the real linker script and startup file
a flashable firmware image needs.

See ROADMAP.md and BOM.md at the project root for sequencing and scope.
