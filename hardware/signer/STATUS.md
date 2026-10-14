# Status: chip decided, hardware reference not started

Hardware reference for the signer board — **STM32F412** (decided, over
the cheaper F401/F411, specifically for its real hardware TRNG; see
`firmware/signer/STATUS.md` and `docs/build-notes/signer-protocol-build-notes.md`),
OLED, and mechanical inline disconnect switch. An STM32F401 (Black
Pill) is being used separately to bootstrap the firmware toolchain
while real F412 hardware is on order — see
`docs/build-notes/stm32f401-bootstrap-build-notes.md` — but F412
remains the real, decided production target for this hardware
reference.

See ROADMAP.md and BOM.md at the project root for sequencing and scope.
