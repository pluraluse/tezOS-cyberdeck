# Concepts — real ideas raised, not yet scoped for a build cycle

This file exists so ideas discussed while waiting on external results
(Shadownet testing, spec decisions) don't get lost between sessions.
Nothing here is committed to being built; each entry states what's
actually been thought through versus what's still just an idea.

---

## 1-bit Art — real scope clarified, not yet started

**Real, clarified scope**: not just a from-scratch drawing app — real
image *editing* software too. Draw original images, but also load and
edit/mark up existing photos, not just blank-canvas creation.

This directly validates reusing Gallery's already-proven pipeline
rather than building new decode logic: Camera's real downsample/dither
functions and `tezos_image_decode.h` already turn an arbitrary real
image into pixel data this renderer can draw — loading a real photo as
a starting point or reference layer before drawing over it is a real,
natural extension of code that already exists and is already tested,
not new decode work. **Priority, per direct instruction**: lower than
`.tez` name resolution (below) — worth sequencing after that, not
before.

## Snake — real, built

**Status: done.** `src/apps/snake/`, reachable from Settings → About
("PLAY SNAKE"), not given a main-app-grid slot — the open placement
question got decided: still discoverable and documented, just not
competing for grid space with Wallet/Explorer/Messenger. See
`docs/build-notes/snake-build-notes.md` for the full writeup, including
two real test-design bugs (not product bugs) caught while writing the
tests, and one genuine compiler warning (an undersized score buffer,
not a known false-positive class) fixed properly. Real seedable PRNG,
real high-score persistence, real 180-reversal prevention, 18+ checks
under AddressSanitizer.

## Address book — real, built

**Status: done.** `src/core/tezos_address_book.h`/`.c`, wired into
Wallet's Contacts page (previously three hardcoded fake entries, now a
real, persisted list). See `docs/build-notes/address-book-build-notes.md`
for the full writeup. Real validation on add (via `tezos_forge_address`),
real persistence (verified with a genuine two-process test), real
delete-and-shift. Selecting a saved contact in Wallet jumps straight to
amount entry with the address pre-filled — the actual point of an
address book, not a side effect. **Not yet done**: Messenger's and
Explorer's own address-entry screens don't offer "pick from saved" yet
— the module is shared/reusable, but only Wallet actually uses it so
far. Still connects naturally to the `.tez` name-resolution idea from
earlier (both are "turn a name into an address," just one is
self-service, one is a real on-chain lookup) — that idea remains
unbuilt.

## `.tez` name resolution — real, built, wired into Wallet Send

**Status: done.** See `docs/build-notes/tez-domain-resolution-build-notes.md`
for the full writeup. Real, direct priority call honored: built ahead
of 1-bit Art. Real, honest scope cuts: no proxy-resolution step (a
real, user-configurable Settings value instead), no full Unicode
normalization, no expiry checking. Wired into Wallet's Send flow; the
address book (above) remains a real, natural pairing not yet unified
with this — both are "turn a name into an address," one self-service,
one a real on-chain lookup.

## App menu icons — real rendering pipeline built; all 9 app icons designed; wired into a real grid home screen

**Status: technical foundation, full icon set, and real grid home
screen all done.** See `docs/build-notes/app-icons-build-notes.md` and
`docs/build-notes/grid-home-screen-build-notes.md`. Direction settled:
true multi-color, mascot-style icons (ICQ/AIM era), not the existing
single-color MEK-Dings font — a real correction made along the way,
since MEK-Dings turned out to already exist as real infrastructure but
didn't stylistically fit once the actual glyphs were looked at. A real
shared rendering function (`tezos_draw_icon_bitmap`, plus a real
integer-scaled variant for the home screen) exists, extracted from
what had been duplicated blit code in Gallery and Discover, plus a
real authoring tool (`tools/rasterize_icons.py`) for hand-designing
icons as plain, editable text. All 9 real icons designed and now
rendered in a real, first-class 3×3 grid home screen. Concepts
confirmed agreed-upon before a second execution pass — Wallet,
Discover, and Beacon Wallet all show real, clear improvement.
Explorer's second attempt is an honest, disclosed tradeoff, not a
clean win (bolder, but less recognizable as a magnifying glass) — a
third pass, or the radar-sweep alternative, is still worth
considering.

## In-device browser (for Discover's "open a browser to view" fallback) — real research done, deferred

**Status: real research complete, explicitly deferred, not started.**
Real answer identified for when this gets picked up: **WPE WebKit +
Cog**, not Chromium — WPE is a real, production WebKit port built
specifically for embedded/low-power devices (no GTK/Cocoa dependency,
used at real scale in set-top boxes, automotive, digital signage), and
Cog is a real, minimal, single-window kiosk browser built on top of
it, explicitly designed as a UI-less "web application container...
for embedded devices in kiosk mode." Real, direct precedent for this
exact class of hardware: the Buildroot WPE project lists Pi **Zero**
specifically as a supported target, not just Pi 3/4.

**Real, honest limit, not solved by this choice**: even the lightest
realistic engine is still a full web engine, and real dApp sites are
almost always JS-heavy React/Vue SPAs. WPE makes running a browser on
this hardware *possible*, not *fast* — "loads and is usable, if not
snappy" is the real bar, not smooth desktop-class browsing. **Real,
unresolved integration question**: this project's own rendering goes
straight to DRM/KMS; Cog's graphics stack is normally Wayland/EGL —
how the two coexist (full-screen handoff vs. something more
integrated) is real, unbuilt design work, not a solved detail.

**Explicitly a later problem, by direct instruction** — page-load
performance on real hardware is a real constraint worth confirming
hands-on before investing further design time here.

## Dedicated marketplace apps vs. one unified Discover

Real, undecided tradeoff between two real approaches:

- **One Discover app** (the currently-scoped plan) — reuses the token-
  browsing/holdings/entrypoint-calling capability that Gallery/Explorer
  already provide, since a lot of "browse a marketplace" reduces to
  the same underlying real capability. Less total code, but real risk
  of not fitting every platform's actual shape well.
- **Dedicated apps per platform** (Mederu, Teia, objkt each separately)
  — more honest to each platform's real, distinct contract/entrypoint
  shape (a "buy" on one marketplace is a genuinely different entrypoint
  call than "buy" on another), at the cost of a separate research-then-
  build cycle per platform.

**Leaning**: build Discover generically first, since Gallery/Explorer
already provide most of the underlying real capability; only peel off
a dedicated app for a specific platform if the generic approach turns
out not to fit it in practice. Not a final decision.

## Tezos DeFi (QuipuSwap, Plenty, Crunchy)

Confirmed real, current, substantial ecosystem (multiple independent
sources, this session) — QuipuSwap (the largest Tezos DEX, already
directly relevant: its real DEX 2.0 factory contract was the actual
subject of earlier entrypoint-discovery research and testing this
session), Plenty (DEX + cross-chain bridge, its bridge contract also
already confirmed real earlier this session), Crunchy (lending/DeFi
services). Real, useful connection worth stating plainly: **basic DeFi
interaction (a swap, a stake) might already be mostly reachable through
Explorer's real entrypoint-calling engine as built**, without a
dedicated DeFi app at all — the engine doesn't care whether an
entrypoint belongs to a marketplace or a DEX. Whether a dedicated
DeFi-specific app (with swap-rate display, pool info, a purpose-built UI
rather than generic "here's a raw entrypoint call") is worth building
on top of that generic capability is a real, separate, undecided
question — not urgent given the generic path already exists.

## "Mini apps" (Farcaster Frames/Mini-App-style third-party experiences) — decided: not happening on this project

**Real, definitive decision, not a deferral**: this is not being built
as part of tezos-cyberdeck. Possibly a genuinely separate, future
Tezos-ecosystem effort someday, but not inherently tied to this
project — kept in this file as a real record of the idea and the
reasoning that ruled it out, not as a live backlog item.

Raised because Mederu (a platform this project already interacts with)
has publicly stated Farcaster integration is on its own roadmap. The
underlying question: should tezOS itself support running lightweight,
third-party, dynamically-provided experiences — the way a Farcaster
client hosts Mini Apps/Frames — rather than every app being native,
compiled-in C code fully trusted at build time, which is what this
entire project has been until now.

This was a fundamentally different category of feature from everything
else in this document, for one specific, serious reason: **this device
also holds a signer.** Any framework for running code that wasn't
written by this project's own developers, on the same device that can
sign real transactions, is a real security boundary question — not a
UI feature. The real open questions that would have needed real
answers before this could be responsibly scoped, for the record:

- What would a "mini app" even *be* on this device — a restricted
  scripting language, a declarative UI description format, something
  else? Native C is not a safe answer here.
- Where do mini-apps come from — bundled/vetted at build time (safer,
  but then "dynamically provided" isn't really true), or fetched over
  the network at runtime (real, serious trust and supply-chain
  questions on a device with a wallet)?
- What can a mini-app actually touch — read-only display? Can it ever
  request a real signature, and if so, through what gate (the existing
  popup-confirmation pattern, or something stricter given the
  reduced-trust context)?
- Given this device's whole architecture assumes an airgapped signer
  as the actual trust boundary, does a compromised or malicious
  mini-app even matter as much as it would on a hot-wallet device — or
  does it matter *more*, since the Linux side already handles real key
  material for the software-signer path documented elsewhere?

**Status: closed, not happening here.** Not scoped, not started, and
shouldn't be

is. If this gets picked up seriously, it deserves its own dedicated
research-and-design pass before any code, the same discipline every
real feature in this project has gotten — arguably more, given the
security stakes are categorically different from a Snake game or an
address book.

## Platform philosophy — build the open platform, not every integration

A real clarification of intent, worth keeping distinct from any single
feature: the goal isn't for this project to deep-integrate with every
Tezos platform (Kukai, Teia, objkt, Mederu, QuipuSwap, ...) forever.
It's to build a genuinely open, general platform — the shared dynamic
entrypoint engine, a real Beacon-compatible wallet, an open TZKT
client, documented app patterns — good enough that those teams might
want to build (or take over) their own piece of it. Explicitly stated
hope: Kukai's own team might someday own the actual Kukai-compatible
signing experience on this device, rather than this project maintaining
a reverse-engineered approximation of it indefinitely. This reframes
"build a dedicated app for platform X" as a fallback for when the
generic path doesn't fit, not the default strategy — consistent with,
and an extension of, the earlier Discover/Explorer-generic-first
reasoning above.

**Concretely, this means the "act like Kukai" idea (see the dedicated
build notes once started) is not "run Kukai" or "replace Kukai" — it's
tezOS becoming a real, standalone Beacon-compatible wallet, using the
same open protocol Kukai/Temple/every other Beacon wallet already
speaks.** Once built, this device can pair with *any* Beacon dApp, not
just act as a Kukai substitute — a real software-only signing path,
specifically for builders who complete the device without the STM32
signer, parallel to (not a compromised version of) the hardware-signer
path. **Where this actually landed, after going back and forth on it
directly**: Kukai has been removed from this project's production
entirely — no Kukai name or branding anywhere in the shipped app. The
app is kept protocol-generic ("Beacon Wallet"). Whether/how to
represent a specific real wallet by name on this device is a real,
open question, deliberately not decided here, needing a clearer shared
understanding first. What building this against Kukai's real,
published Beacon behavior *did* confirm, independent of the naming
question: the architecture genuinely works and generalizes to any
Beacon-compatible wallet (Kukai, Temple, others), not something
Kukai-specific. See
`docs/build-notes/beacon-wallet-app-build-notes.md`'s naming section
for the full back-and-forth.

## Supporting a mix of hardware, and a future Android build

Two real, stated goals worth keeping visible: (1) the software should
genuinely support builders assembling this on varied hardware
(different signer approaches, possibly different boards/displays down
the line) rather than assuming one fixed BOM is the only real path —
some of this is already true by construction (the logical-canvas
letterboxing already handles display-resolution differences; the
shim-linux layer already isolates platform-specific I/O from the
portable `tezos_core` engine), worth treating as a real constraint to
preserve, not just an accident. (2) A longer-term, explicitly-a-later-
cycle idea: an Android build that runs the software-signer/Beacon-
wallet experience directly on a phone, with no physical device
required at all — the most accessible possible tier of adoption.
**Status: concept only, not scoped.** The core rendering/logic
(`tezos_core.h` and friends) was already built with a platform shim
boundary in mind (`linux_shim.c` isolating DRM/evdev specifics from the
portable engine), which is a real, if partial, head start — but an
actual Android port is a genuinely different UI toolkit and app
lifecycle, not a small follow-on, and deserves its own real scoping
pass when it's actually taken up.
