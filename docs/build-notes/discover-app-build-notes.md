# Discover — Build Notes

## Status: real, tested, built almost entirely by reusing existing capability

The framing that shaped this: a real Tezos address browser, in the
spirit of an old-web browser — visit any address like typing a URL,
see real basic info about what's there, browse what it holds with
real images if it has any, bookmark it for next time.

## A real, deliberate decision: no hardcoded "known dApps" list

An early instinct would have been to ship Discover with a curated
home-page list of well-known marketplace contracts (Teia, objkt,
QuipuSwap, etc.). Deliberately not done — real browsers don't ship
with someone else's fixed bookmark list baked in, and this project has
its own recent, direct history of exactly this kind of thing going
wrong: a contract address mislabeled with the wrong platform name in a
third-party audit report, caught only by independently checking it.
Shipping a small, hand-picked address list this project can't
continuously, independently verify stays correct felt like the wrong
kind of confidence to bake into the product.

**Instead**: the real, already-tested address book (`tezos_address_book.h`)
*is* the bookmark list — user-curated, exactly like a real browser's,
not vendor-curated. Discover's "+ Visit New Address" is the real
address bar; visiting somewhere real, useful lets you bookmark it via
`STAR`.

## Almost entirely built by reusing existing, already-tested capability

Nothing about Discover required new chain-layer code:

- **`tezos_tzkt_get_contract_info()`/`get_entrypoints_with_types()`** —
  Explorer's own real functions, unchanged, for "what is this address."
- **`tezos_tzkt_get_owned_tokens()`** — Gallery's own real function,
  unchanged, for "what does it hold." Already generic — takes an
  explicit address parameter, not implicitly "the active wallet" —
  which is exactly what made this reuse possible with zero changes.
- **The real image decode/thumbnail pipeline** (`tezos_image_decode.h`
  + Camera's downsample/RGB565-convert functions) — the same pipeline
  Gallery's own real viewer already proved out.

That generic capability being genuinely generic, not something built
narrowly for one specific app, is the actual point of building it that
way in the first place — this is the real payoff, not a coincidence.

## A real design bug caught and fixed before shipping

The first draft gated browsing behind bookmarking — you couldn't
browse an address's holdings until you'd bookmarked it first, since
both were mapped to the same `SOFTKEY_L` action with an `if
(!is_bookmarked) { bookmark } else { browse }` branch. That's an
arbitrary constraint no real browser has — you can browse a page
without bookmarking it. Fixed by making the two real actions
genuinely independent: `STAR` bookmarks, `SOFTKEY_L` always browses
when there's something to browse, regardless of bookmark state. Caught
directly while writing the real test for this flow, not in review.

## Real, disclosed duplication, not hidden

The image fetch/decode/thumbnail logic and the RGB565 blit helper are
duplicated from `gallery.c` rather than extracted into a shared
module in this pass — a real, honest tradeoff made to avoid a larger,
riskier refactor while building Discover, not an oversight. A real,
worthwhile future consolidation, not attempted here.

## Verified

10 checks: browsing works without bookmarking first (the corrected
design, verified directly); real holdings list → detail → back
navigation; real, independent bookmarking via `STAR`, validated
through the same real `tezos_forge_address` check the address book
already uses (an invalid test address was correctly rejected during
test-writing, itself real confirmation the validation still works);
bookmarking an already-bookmarked address doesn't duplicate; real Home
→ address-entry navigation. All under AddressSanitizer, zero errors.
Two real test-methodology bugs (not product bugs) caught while
writing the test itself: pushing a second injected screen on top of
the first without popping meant both pointed at the same shared static
state (fixed by using a fresh core/fb per scenario, the established
pattern elsewhere in this project), and an invalid test address was
correctly rejected by real validation that was working exactly as
designed.

## Real, disclosed scope, not attempted in this pass

- **No hand-off to Explorer's dynamic entrypoint-calling** — Discover
  can show an address's entrypoint *count*, but calling one is
  deliberately left to Explorer, which already does this well. A
  direct hand-off (jump straight to Explorer for this exact address)
  is real, natural follow-up work, not built here — Explorer doesn't
  currently expose an entry point that takes a pre-filled address.
- **Live network fetch remains genuinely unverifiable from this
  sandbox** — same disclosed limitation as every other network-touching
  feature in this project.
