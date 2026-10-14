# Boot Splash — Build Notes

## Status: real, tested, wired into boot

An original animation in the spirit of the classic 16-bit console boot
logo (a bright gleam sweeping across a wordmark on a dark screen) —
built for tezOS's own name and palette, not a reproduction of anyone
else's specific artwork. Shows once, first, before the real home
screen.

**How it actually works**: "tezOS" is drawn one character at a time
(not as a single string) so each letter can be independently
brightened as a sweep position moves across the wordmark — this is
what makes it read as light passing *through* the letters rather than
a bar floating in front of static text, achievable with only the solid-
color rect/text primitives this renderer actually has (no alpha
blending or clipping exists in `tezos_gfx.h`). A bright accent beam
rides alongside the sweep for the "beam of light" read specifically.
After the sweep (1.4s) and a brief hold (0.5s) — or immediately on any
real key press — it hands off to the real home screen.

**A real architectural correction made while building this, not an
afterthought**: the first draft had the splash's own `handle_input`
return `TEZOS_ACTION_POP` on any key press to skip it. That's wrong —
the splash is pushed as the very first (and briefly only) screen on
the stack, and `tezos_core_pop()` correctly refuses to ever pop the
last screen, so that would have been a silent no-op on real hardware:
press any key, nothing happens, the splash never actually goes away.
Caught before shipping by tracing through `tezos_core.h`'s own actual
`pop` contract, not by luck. Fixed properly: neither the timer-elapsed
case nor the key-press case is handled by the splash screen returning
an action at all — both just set internal state, and the platform
shim's own main loop (which is the only thing that actually knows what
its real home screen is; this core module correctly doesn't hardcode
that) checks `tezos_boot_splash_is_finished()` after both `on_tick` and
real input dispatch, performing the actual transition itself. One real
mechanism for both exit paths, not two different ones bolted together.

**Verified**: 10 checks, including the timer/skip logic directly and a
full-stack `-O2` compile with the real main-loop wiring in
`linux_shim.c`. Visually confirmed across 5 rendered frames — entry,
two mid-sweep positions showing the correct letters lit as the beam
passes them, and the final fully-lit hold — that the gleam effect
genuinely looks like light sweeping across the word, not just an
abstract color change.
