# STM32F401 Bootstrap — Build Notes

## Status: real, linked, produces a real flashable image — never flashed yet

**Chip**: STM32F401CCU6 (WeAct "Black Pill" board — 256KB flash, 64KB
RAM), used specifically to bootstrap the toolchain and protocol work
*before* real STM32F412 hardware arrives, since F401 lacks the
hardware TRNG the real signer design depends on. **Not the production
signer target** — see the "what this deliberately does not include"
section below.

## The real, first-ever milestone this represents

Every piece of signer firmware built before this compiled to
individual object files, proving the code was valid for the real
target — but nothing had ever been linked into an actual, complete,
flashable image. This is that: a real linker script
(`linker.ld`, memory map matching this exact chip's real 256KB
flash / 64KB RAM), a real startup file (`startup_stm32f401xc.s`, the
actual vector table and reset handler — copies `.data` from flash to
RAM, zeroes `.bss`, calls `main()`), and a real link that succeeds
end to end, producing a real `.bin` image via `arm-none-eabi-objcopy`.

**Independently verified, not just "the linker didn't error"**:
- `file` confirms a real `ELF 32-bit LSB executable, ARM, EABI5`, not
  just a relocatable object file.
- The `.isr_vector` section sits at VMA `0x08000000` — the real,
  correct STM32 flash base address this chip actually boots from.
- `objdump -d` disassembly was read by hand: `uart_read_byte()`
  correctly compiles to poll the real USART1 peripheral base address
  (`0x40011000`, the real, correct STM32F401 address) and check the
  real RXNE status bit — genuinely correct machine code, not just a
  file that happens to exist.
- `build.sh` re-run from a clean state produces byte-identical
  `text`/`bss` sizes — a real, reproducible build, not a one-off that
  happened to work once.

## What this reuses, unchanged, from the real F412 work

The wire protocol (`tezos_signer_apdu.*`) and operation-assembly
session layer (`tezos_signer_session.*`) — both were already
chip-independent by design, so nothing about them changed to target
this different chip. That's the actual point of building them portable
in the first place: this bootstrap exercise is proof the design choice
paid off, not a coincidence.

## What this deliberately does NOT include, and why that matters

**The RNG safety layer never touches this chip, on purpose.** The
STM32F401 does not have the hardware TRNG peripheral the RNG driver
depends on — the register block simply isn't present. Linking that
code here would either fail outright (undefined register symbols) or,
worse, if it somehow built anyway, silently read whatever garbage
happens to be mapped at that address and report it as a "real" random
word. That would be a live, self-inflicted demonstration of the exact
failure mode the whole RNG safety design exists to prevent (see
`docs/build-notes/signer-protocol-build-notes.md`'s Coldcard section).
`main.c` here contains no reference to it at all, and `build.sh`
doesn't compile or link `tezos_signer_rng_core.c`/`tezos_signer_rng.c`
— not because it was forgotten, but because it categorically doesn't
belong here.

**This chip's board revision may not match the final wiring plan.**
"CCU6" confirms the flash/RAM (256KB/64KB), but real, documented board
variants exist with different pinouts (e.g. a 40-pin "black" board vs.
a 44-pin "green" board, both using the same STM32F401CCU6 chip) — the
UART pins `main.c` assumes (real `USART1` register access) haven't
been confirmed against the actual, physical board in hand. Worth
checking before wiring anything for real.

## Real, honest scope — still not something to flash yet

- **Never actually flashed to real hardware** — this is a real,
  verified image, not a real, confirmed *running* one. The real DFU
  flashing command is documented in `build.sh`'s own output, using
  this exact board's real, common bootloader-entry procedure (hold
  BOOT0 during reset/power-on), but hasn't been tried.
- **No clock configuration** — this runs on whatever the reset default
  clock happens to be (the internal HSI oscillator, un-configured); no
  `SystemInit()`/PLL setup exists yet, so real UART baud rate
  timing would need this to be addressed before real communication
  could work.
- **Only the core Cortex-M4 exception vectors are populated** in the
  startup file — real STM32F401 peripheral interrupt vectors
  (`USART1_IRQn`, etc.) aren't listed. Since this bootstrap runs
  entirely polled with interrupts never enabled, that's a real,
  acceptable simplification for this pass specifically, not a claim
  that interrupt-driven operation would work correctly as shipped.
