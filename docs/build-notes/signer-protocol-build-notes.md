# Signer Wire Protocol — Build Notes

## Status: real, tested, and cross-compiled for the real target chip

The chip decision landed on the **STM32F412** — deliberately the more
expensive option over the F401, accepted specifically for its real
hardware TRNG (see the earlier signer-chip research: F401/F411 lack
one, F410/F412 have one). This is the first real work built against
that decision.

## The wire protocol — real ISO/IEC 7816-4 APDU framing, not invented

Built against the real, published spec, not from scratch: a Command
APDU has a mandatory 4-byte header (CLA/INS/P1/P2) and an optional
body (Lc + Data + Le); a Response APDU has optional data and a
mandatory 2-byte SW1/SW2 status trailer. This is the same real
precedent production hardware wallets (Ledger and others) build their
own signing protocols on top of — a genuinely well-specified framing
already solved once, not something worth re-solving badly here.

**Real, deliberate v1 scope**: short-form Lc/Le only (1-byte length
fields, 255 bytes max per APDU). Real Tezos operations — especially
ones with Michelson parameters — routinely exceed 255 bytes, so
multi-APDU chaining is real and built in from the start via the
`INS_SIGN` instruction's P1 byte (`INIT`/`CONTINUE`/`LAST`), the same
real pattern production hardware wallets use for exactly this problem.
Extended-length APDUs (3-byte Lc/Le, up to 65535 bytes) were
deliberately not built — chaining already solves the real size
problem without that added complexity, and a command whose Lc byte is
`0x00` (which would signal extended-length in the full spec) is
correctly rejected as out of this project's real v1 scope, not
silently misparsed.

## Deliberately, entirely portable — chip-independent from the start

`firmware/signer/protocol/tezos_signer_apdu.h`/`.c` contains zero
platform-specific code — no STM32 registers, no UART/SPI calls,
nothing. This is the same discipline as everything else in this
project: build the part that's genuinely testable in software, prove
it correct there, then link it against real hardware unchanged.

**Verified real ISO 7816-4 case-handling**: all four real short-form
cases (no data/no Le; no data/Le present; data/no Le; data/Le
present) are distinguished correctly by the real total length together
with the real Lc byte's value — not guessed at. 22 checks, covering
all four cases, the real "Le byte 0x00 means 256" convention, the
real 255-byte maximum, real malformed-input rejection (too short, a
length that matches no real case, an extended-length-style `Lc=0x00`),
and real response building — including a genuinely important safety
check: a response that wouldn't fit the output buffer is reported as a
failure (returns 0), not silently truncated, since a truncated
signature silently reported as complete would be a real, dangerous
bug for a signer specifically. All under AddressSanitizer, zero
errors.

## Real, cross-compiled for the actual STM32F412 target

This is genuinely new territory for this project — everything else
has run as x86 software in this sandbox. This time:

1. **A real ARM cross-compiler** (`gcc-arm-none-eabi`) was installed
   and confirmed working.
2. **Real STM32F412-specific CMSIS headers** were vendored from
   STMicroelectronics' own official `cmsis_device_f4` GitHub repo
   (Apache 2.0, checked before vendoring) — `stm32f412zx.h`,
   `stm32f4xx.h`, `system_stm32f4xx.h` — plus the real ARM CMSIS Core
   headers they depend on, from ARM's own official `CMSIS_6` repo
   (also Apache 2.0), all under `third_party/cmsis_stm32f4/`.
3. **`firmware/signer/stm32f412/main.c`** — a real, minimal firmware
   skeleton that includes the real vendored device header and links
   the portable protocol layer against it, with a real (if minimal)
   UART receive/dispatch loop using real register names and real bit
   definitions from the vendored header (`USART1->SR`, `USART_SR_RXNE`,
   `USART1->DR`), not placeholder/invented names.
4. **Compiled cleanly** with `arm-none-eabi-gcc -mcpu=cortex-m4
   -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -DSTM32F412Zx`, at both
   `-O0` and `-O2`, zero warnings. Confirmed via `objdump` that the
   resulting object file is genuinely `elf32-littlearm`,
   `architecture: armv7e-m` — real Cortex-M4 machine code, not
   accidentally the host architecture.

## Update: real operation-assembly session layer, and a real logic bug it caught

`firmware/signer/protocol/tezos_signer_session.h`/`.c` — reassembles a
real forged operation across one or more `INS_SIGN` chunks into a
complete buffer, before anything can be displayed or signed. Same
discipline as the wire protocol itself: entirely portable, zero
platform-specific code, testable independent of the crypto backend or
chip.

**Resolves a real ambiguity left open by the wire protocol's own P1
values**: a small operation that fits in a single APDU sends
`P1=LAST` directly from idle (simultaneously the first and only
chunk); a large one sends `INIT`, zero or more `CONTINUE`, then
`LAST`. Both are real cases — simple transfers are small, operations
with Michelson parameters routinely aren't.

**A real logic bug, caught by this module's own test suite, not a
false start**: the first version always reset the session on `LAST`,
regardless of current state. That's correct when `LAST` arrives
directly from idle (a genuine single-chunk operation), but wrong when
it arrives after a real, legitimately-started `INIT`/`CONTINUE`
sequence — resetting there silently discarded every previously
accumulated byte right before the operation was supposed to complete,
with no error reported. Fixed by distinguishing the two real cases:
`LAST` only resets if the session *isn't* already `RECEIVING`;
otherwise it appends to what's already there. A second, related fix
followed from the same insight: once a real `INIT` has started a
sequence, a later `LAST` must be treated as completing *that*
sequence, not as a fresh, unrelated single-chunk operation — the
protocol has no way to distinguish "the host gave up on this sequence"
from "this is genuinely the final chunk," so silently discarding
`INIT`'s bytes would be strictly worse (data vanishing with no error)
than requiring an explicit fresh `INIT` to actually restart.

**A real, precise ISO 7816-4 status word was added** for out-of-
sequence chunks: `0x6A86`, "Incorrect parameters P1-P2" — the real,
standard code for exactly this, not a stretch of an existing one.

**Verified**: 31 checks, covering both real completion paths
(single-chunk and multi-chunk), the corrected `LAST`-after-`INIT`
behavior specifically, out-of-sequence rejection from both `IDLE` and
stale `READY`, the "fresh `INIT` always discards stale bytes cleanly"
safety property, and real buffer-overflow rejection. All under
AddressSanitizer, zero errors. Cross-compiled cleanly for the real
STM32F412/Cortex-M4 target at both `-O0` and `-O2`, same as the wire
protocol layer.

**Wired into `main.c`'s dispatch loop for real**: `INS_SIGN` chunks
now genuinely flow through this session layer. Once a session reaches
`READY`, the honest, current behavior is to respond
`INS_NOT_SUPPORTED` and reset — the operation is fully assembled, but
real confirmation-display and signing logic don't exist yet, and
that's reported accurately rather than pretended away.

## Update: real hardware RNG safety layer, built against a live, ongoing incident

A real, current event drove this: the August 2026 Coldcard hardware
wallet incident. That was not a logic bug producing wrong output — it
was a firmware path that, when the real hardware entropy source
wasn't behaving as expected, **silently fell back** to a weak,
predictable software-seeded value and kept operating normally, with
no error and no warning, generating catastrophically weak keys that
were later brute-forced at scale. The chip decision (F412 over the
cheaper F401/F411, specifically for its real hardware TRNG) was the
right first instinct, but it isn't sufficient on its own — the
firmware has to actively verify that RNG is genuinely working before
ever trusting it.

**Built directly against Coldcard's own real fix, not reinvented**:
`github.com/Coldcard/firmware` PRs #692 and #693 document the actual,
correct STM32 RNG error-handling sequence, including a real, subtle
nuance a first attempted fix got wrong — treating a transient *clock*
error (`CECS`/`CEIS`) as fatal the same way a *seed* error
(`SECS`/`SEIS`) is. That's also wrong: a clock error doesn't
necessarily invalidate data already latched into the data register
(the real Linux `stm32-rng` driver takes the same position), while a
seed error does, and is sticky until software explicitly clears it.
This project's implementation follows the real, documented seed-error
recovery specifically: clear `SEIS`, discard 12 words to flush the
pipeline (the real, documented count, not invented here), then
reconfirm `SEIS` is still clear before trusting further output.

**The one, load-bearing rule this is built to enforce structurally**:
every caller of `tezos_signer_rng_read_word()` gets either a genuinely
verified random word, or `false` — and the only correct response to
`false`, anywhere in this codebase, is `tezos_signer_panic_halt()`.
There is no third path, and no silent fallback to anything softer,
anywhere in this code.

**`tezos_signer_panic_halt()`** — disables interrupts
(`__disable_irq()`) and spins forever. Recovering requires a real
physical power cycle. This is the deliberate, correct response to
anything meaning the device can't currently guarantee safe operation
— continuing to run in a possibly-degraded state, even "gracefully,"
is the exact failure mode this whole design exists to make impossible.

**Split into a portable core and a chip-specific wrapper, the same
pattern as the wire protocol and session layers** —
`firmware/signer/protocol/tezos_signer_rng_core.c` contains the actual
recovery logic, taking the register struct as a parameter rather than
reaching for real hardware directly. This is what makes it testable in
software at all: a real, in-memory struct can simulate `DRDY`/`SEIS`
behavior byte-for-byte — the exact same technique Coldcard's own
engineers used to verify their fix (their PR includes its own
`rng-testbench.c` doing precisely this). The genuinely untestable
parts — real register addresses, the `__disable_irq()` intrinsic that
doesn't exist on any non-ARM target — are isolated to
`firmware/signer/stm32f412/tezos_signer_rng.c`, a thin wrapper that
just points the tested logic at the real hardware.

**Verified**: 8 checks — the nominal case, a real transient seed error
genuinely recovered via the documented sequence (confirmed by checking
the actual register state afterward, not just the return value), and
two real failure modes (a dead RNG that never asserts, and a
persistent seed error that the documented recovery itself can't fix)
both correctly reported as failure rather than hung or silently
accepted. One test-coverage gap disclosed honestly rather than faked:
`SEIS` reasserting specifically right after a successful flush isn't
covered, since simulating that with a plain static mock (rather than a
callback-based one) wasn't straightforward — a real, distinct
limitation from the two failure modes that *are* covered. Both the
portable core and the chip-specific wrapper cross-compile cleanly for
the real STM32F412/Cortex-M4 target.

## Real, honest scope — this is not a flashable firmware image

This compiles to a real object file, proving the protocol code is
valid for the real target. It does **not** link into something that
could be flashed to real hardware. That needs, and does not yet have:

- **A real linker script** — the actual memory map (flash/RAM sizes,
  addresses) for this specific chip.
- **A real startup file** — the vector table and reset handler.
- **Real UART/GPIO clock and pin configuration** — `main.c`'s
  `uart_read_byte()`/`uart_write_byte()` use real register names, but
  nothing here configures the actual clock tree or GPIO alternate
  functions a physically working UART needs; that's real, separate,
  unbuilt work.
- **Real signing logic** — ed25519 signing, the real physical
  confirmation button, and the operation/Michelson-param decoder for
  the OLED are all still unbuilt (per `firmware/signer/STATUS.md`).
  `main.c`'s dispatch loop now genuinely assembles a complete operation
  through the real session layer above, but once one reaches `READY`,
  it responds `INS_NOT_SUPPORTED` and resets — an honest placeholder
  for where confirmation and signing go, not a completed dispatch.

## What this actually unblocks

The wire protocol was the piece explicitly flagged as worth building
*before* chip-specific firmware, precisely because it's portable and
doesn't need to be redone once real signing/GPIO/UART logic gets built
on top of it. That's now done and proven against the real target, not
just in the abstract.
