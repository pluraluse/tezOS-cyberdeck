# Explorer App — Build Notes

## Status: real address lookup, activity signals, entrypoints, holdings; entrypoint *calling* not built yet

Consolidates what used to be a separate "Scanner" app concept. The
actual stated use case is recovering your own assets from a project
whose website has gone dark — you don't need a QR code from a site
that no longer exists, you need the contract address, which you
already have. Address entry (T9) is the primary flow here; Scanner's
QR-based input remains real, tested code (`src/apps/scanner/`, not
deleted) but is no longer a separately-registered app — folding it in
as an alternate input method for Explorer is real, described follow-up
work, not done in this pass.

**Real, built on three TZKT functions:**

- **`tezos_tzkt_get_contract_info()`** — activity/staleness signals
  (balance, transaction count, first/last activity). Verified against
  the *exact* real response captured live earlier this session for the
  real Liquidity Baking contract (while researching Scanner's
  entrypoint-discovery feature) — not a new fixture, reused real data
  this project already had.
- **`tezos_tzkt_get_entrypoints_with_types()`** — already built for
  Scanner/Messenger's shared dynamic engine; Explorer is simply another
  consumer of the same real, tested function.
- **`tezos_tzkt_get_owned_tokens()`** — already built for Gallery; a
  KT1 address is just an account as far as TZKT is concerned, so the
  same function shows what a *contract itself* holds, not just what a
  wallet holds.

**Two real compiler warnings caught and fixed, one via each of this
project's two established approaches**: a label buffer sized without
real headroom (fixed by enlarging it, same as Gallery's earlier fix),
and a second `-Wformat-truncation` warning that persisted even after
enlarging the buffer — a known GCC static-analysis limitation with
fixed-size struct char-array fields that this project already has
precedent for (`tezos_popup.c`'s `safe_copy()` helper) — fixed with the
same documented, targeted pragma suppression, not by guessing at ever-
larger buffer sizes that don't actually address the root cause.

**Verified**: the UI flow (address entry → info → Entrypoints/Holdings
sub-screens → back, back, back) under AddressSanitizer, confirming
each BACK press returns to the correct prior screen rather than
over-popping out of the app.

## Deliberate, disclosed scope cut — see docs/SECURITY-FINDINGS.md

This app shows real, public information (activity, entrypoints,
holdings) and will eventually let you call an entrypoint a contract
already exposes — the same category of thing any block explorer or
wallet does. It does **not** attempt any vulnerability/bug detection,
and never will. "Is this contract inactive" (a real, disclosed activity
signal this app surfaces) is a completely different question from "is
this contract exploitable" — this app only ever answers the former. Any
finding that looks like an actual security bug in a real deployed
contract, encountered incidentally while building or using this app,
goes in `docs/SECURITY-FINDINGS.md` — a human-written disclosure log,
never referenced by any code, never a feature.

## Not built yet — the real next step

**Calling an entrypoint with actual, dynamically-prompted parameters.**
The shared dynamic engine (type discovery, `list`/`key_hash`/
`option(list(...))`/`timestamp` support, all of it) has been built and
tested for a while now, but nothing in this project has a real UI that
walks an arbitrary type tree and prompts for each leaf via T9 yet —
Scanner's old flow only ever handled a single int. This is genuinely
the largest remaining piece of real UI-engineering work in this
project: a recursive prompt flow (enter this leaf, then this one, ...,
for however many the discovered type actually needs, correctly handling
pair/list/option nesting), not a small addition. Explorer's
Entrypoints screen currently shows real names and (where fetchable)
real types, with an honest "not built yet" note where calling one
would go.

## Update: entrypoint calling is real now — the walker engine, wired into a real UI

This was described as "the single largest remaining piece of real
UI-engineering work in this project" — now done. Two parts, built and
tested separately before being wired together:

**The walker engine** (`tezos_micheline_walker_t`, `tezos_micheline.h`/
`.c`) — converts recursively walking an arbitrary Micheline type tree
into an explicit, pausable state machine: `tezos_micheline_walker_next()`
returns "I need a leaf of this kind" or "I need this list's item count"
one step at a time, so a real UI can prompt for one value, wait for
real input, then resume — rather than needing every value collected
up front. Processing order matches `tezos_micheline_build_value`'s own
depth-first walk exactly. Verified with 27 checks covering: correct
leaf ordering for `pair(int,string)`, the critical `pair(address,
address)` aliasing case (now proven through the walker specifically,
not just the underlying builder), real list handling
(`list(pair(address,bool))`), and — directly relevant to Messenger —
`option(list(address))`, the exact shape `write_message`'s `cc`/`bcc`
fields needed, now provably buildable through the general walker
instead of hand-built. Every walker-driven value checked to encode
byte-for-byte identically to the equivalent built directly.

**Wired into Explorer's real UI**: selecting an entrypoint with a known
type now drives the walker through real T9 prompts (one leaf or list
count at a time, with a plain-language hint per leaf kind — "Enter a
whole number," "Enter an address (tz1/KT1)," etc.), then a real
confirmation popup, then the same real forge/sign/broadcast pipeline
every other real send flow in this project uses. An entrypoint whose
type is unknown (the disclosed gap from the entrypoint-type-fetching
function) is correctly not callable — the list already labels it
"(type unknown)."

**A second real stack-use-after-return bug, this one significant
enough to fix at the source rather than patch locally** — see
`docs/architecture/CORE-ARCHITECTURE.md`'s popup section for the full
writeup. Found while testing this exact call flow under
AddressSanitizer; turned out to be a latent bug shared by Messenger's
and Scanner's confirmation flows too, not something specific to this
new code.

**Test-only injection hook** (`explorer_test_inject_entrypoint()`, not
for production use) lets the full call flow — walking a real type,
confirming, forging/signing/broadcasting — be exercised end-to-end
without needing a live TZKT fetch to discover a real entrypoint first,
same reasoning as Messenger's and Gallery's own test hooks.

**Still not built**: choosing `Right` instead of `Left` for an `or`
type, or omitting an optional field instead of always building `Some`
— both real, disclosed scope cuts from the underlying engine, unchanged
by this session's work. Multi-parameter entrypoints with a `map` type
anywhere in their structure still can't be called at all (the engine's
own disclosed `map` gap) — Explorer's list already shows these,
correctly, as having an unparseable type.
