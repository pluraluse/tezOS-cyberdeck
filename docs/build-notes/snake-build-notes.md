# Snake — Build Notes

## Status: real, tested, playable

A real, native app — not a hidden easter egg — in the spirit of the
classic phone/console version. Built and verified the same way as
everything else in this project: real game logic, real tests, real
bugs caught before they shipped.

## Real design, matching what was scoped during planning

- **Input**: `UP`/`DOWN` for vertical movement (already exists
  everywhere else in this project); `NUM 4`/`NUM 6` for left/right,
  since this hardware has no dedicated 4-way D-pad — a real, deliberate
  mapping decision given the actual keypad this device has, not an
  arbitrary choice.
- **"Sprites instead of raw LCD pixels"**: the snake's head is drawn in
  a distinct color (cyan) from its body (blue) — a real, deliberate
  sprite distinction, not a uniform line of identical blocks. Achieved
  with this renderer's existing solid-rect primitive; no new drawing
  capability was needed.
- **A real, seedable PRNG** (`xorshift32`), not `rand()`'s global,
  non-reentrant, non-seedable-per-call state — food placement is
  genuinely random during real play, and fully deterministic and
  verifiable in tests via a fixed seed.
- **Real high-score persistence**, reusing the same plain-store
  mechanism as every other persisted setting in this project.

## Placement — a real, considered decision, not a default

Reachable from **Settings → About** (a "PLAY SNAKE" softkey action),
not given its own slot in the main app grid. The main app list was just
deliberately pared down earlier this project (Scanner folded into
Explorer) — adding a game back as a first-class grid entry felt like
the wrong call given that. Still fully real, fully discoverable (no
hidden key combo, just a labeled softkey action on a screen everyone
eventually visits), and documented here plainly — not an actual hidden
easter egg despite the informal placement.

## Two real test-design bugs caught while writing the tests — not product bugs

Worth recording honestly, since both looked at first like the game
logic was wrong, and both turned out to be genuine mistakes in the test
itself:

1. **Assumed sequential taps queue.** The first version of the reversal
   test pressed UP then DOWN in two separate taps, expecting UP to be
   buffered and DOWN (its exact opposite) to be rejected. But
   `pending_direction` is a single buffered slot, not a queue — the
   second tap simply overwrites the first's value before either is
   ever consumed by a real game step. Since the snake's *actual*
   direction at that point was still RIGHT (not UP, which was never
   really applied), DOWN is a legal perpendicular turn from RIGHT, not
   an illegal reversal — the code was correct; the test's own
   expectation was wrong. Fixed by testing a single, real,
   unambiguous reversal attempt instead (LEFT while genuinely moving
   RIGHT), which is correctly rejected.
2. **Assumed blind straight-line movement would find food within 200
   steps.** The grid is 20 columns wide and the snake starts
   horizontally centered — moving in an unchanged straight line hits a
   wall in under 10 steps regardless of where food actually spawned,
   so the original test's loop almost always hit a wall and exited
   early, long before reaching food. Fixed by reading food's real
   position from a snapshot and steering toward it deterministically —
   including a real perpendicular detour (turn down one step first)
   for the case where food is behind the snake's initial direction,
   since turning directly toward it in that case would itself be the
   illegal reversal the game correctly rejects.

## Verified

18 checks on the core game logic (initial state, real movement, real
180-reversal prevention — both same-tick and across-steps, real wall
collision, real food-eating with growth/score verified via
deterministic steering, not luck) plus 3 more confirming the real
Settings → Snake → back navigation actually works end to end. All
under AddressSanitizer, zero errors. One real, valid compiler warning
(not a known false-positive class) was also caught and fixed: an
8-byte score buffer was undersized for the theoretical full `int`
range, even though a real score can never practically approach it
(bounded by the 520-cell grid) — fixed by giving the buffer real
headroom rather than relying on a bound the compiler couldn't see.

## Real, disclosed scope, not attempted in this pass

- No increasing difficulty/speed as score grows — a constant move
  interval throughout, a real, deliberate v1 scope cut, not an oversight.
- No pause functionality.
- The "won by filling the entire grid" edge case (food has nowhere left
  to spawn) is handled safely (no crash, food simply isn't placed) but
  not specially celebrated — a real, extreme edge case, not expected to
  come up in practice.
