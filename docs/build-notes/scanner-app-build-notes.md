# Scanner App — Build Notes

> **Removed 2026-10-16.** Explorer superseded Scanner's capability
> generically (see `explorer-app-build-notes.md`); `scanner.c`/`.h` and
> its tests stayed in the tree afterward, unreferenced, by mistake, not
> decision. Code deleted; this document stays as historical record of
> the real design work below, which is accurate for what was built, not
> for what currently exists in `src/`.

## Status: real, verified where the underlying pieces are standard-based; hand-rolled and non-interoperable at the QR schema level (see below)

The entire spec for this app, from the moment it entered the app suite
decision, was one line: **"generic entrypoint decode/fill/forge."** No
further detail was ever specified — how a user picks a contract, how
parameters get entered, what the actual UI flow looks like. This
document is a real, reasoned design filling that gap, not a guess at
some other blueprint. The forging/signing/broadcasting underneath is
real and standards-verified; the QR payload schema on top of it is not
— see the section below for exactly what that means in practice.

## The QR payload schema — hand-rolled, and that's worth stating plainly

Unlike every other network-shaped piece in this project (Beacon,
Matrix, the RPC injection format, BIP39/SLIP-10), **this schema was not
checked against any existing standard before being written.** That's a
real gap in process, not just a footnote — it was caught by a direct
question after the fact, not by the same discipline applied elsewhere
in this project.

Checked afterward, for the record: Ethereum has a real, published
standard for exactly this use case — **EIP-681**, a URI format for
transaction/contract-call requests, embeddable in a QR code, that
real wallets (MetaMask and others) actually implement. Tezos has no
equivalent. There's no `tezos:` URI scheme, no TZIP standardizing a
one-off QR/URI-encoded contract-call request. The Tezos ecosystem's
actual answer to "a dApp needs a wallet to do something" is Beacon
(TZIP-10) — but that's a fundamentally different protocol (a paired,
ongoing session), not a one-shot URI/QR format, and reusing its full
envelope for a use case that doesn't need a session would have been
the wrong kind of reuse (see above).

So: there genuinely was no existing Tezos-native precedent to follow.
That's consistent with what got built, but the check should have
happened *before* writing the schema, establishing that with
confidence, rather than being confirmed only after being asked.

**The real, practical consequence**: because this schema isn't a
standard anything else implements, **no actual third-party dApp or
website today could generate a QR code this Scanner would recognize**
— it only round-trips with itself (this project's own encoder, this
project's own decoder). That's a genuinely different, weaker claim than
Beacon's QR payload, which real Kukai instances can actually produce.
Fine for proving the forge/sign/broadcast pipeline works end-to-end,
which was the actual goal — but calling that "solved" for real-world
interop would be overclaiming. **This remains unresolved** — the
entrypoint-discovery feature added later in this document verifies a
scanned call's *entrypoint* against the real contract, which is a
genuinely useful, separate safety check, but it does nothing for the
underlying schema-interop gap described here. A QR from a real dApp
still can't be scanned by this Scanner at all, regardless of whether
its entrypoint claim would have checked out.

## Why QR-based input, not manual entry

Tezos contract addresses are 36-character mixed-case Base58 strings.
Typing one via T9 multi-tap would be genuinely painful, and pointless —
Scanner's whole reason to exist is calling entrypoints on contracts a
user encountered elsewhere (a dApp's website, a QR code at an event),
not ones they'd type in character by character. Real-world usage looks
like "a website shows a QR code, scan it to fill in the call" — so this
reuses the exact same real `zbar`/`libqrencode` pipeline already
verified for Beacon pairing and Camera's Scan mode, rather than
inventing a new input mechanism.

## Deliberately not Beacon

Beacon (see `docs/build-notes/beacon-build-notes.md`) is a paired,
ongoing dApp↔wallet session — real protocol overhead (identity
keypairs, session keys, a Matrix transport) that makes sense for a
persistent pairing relationship. Scanner is a one-off, unpaired "scan
this QR, review the call, sign it" flow — genuinely a different use
case. It uses its own simpler JSON schema for the scanned payload:

```json
{"destination": "KT1...", "entrypoint": "collect", "value": {"int": "4021"}}
```

Reusing Beacon's full envelope (type/version/id/senderId/network/
operationDetails) for a use case that doesn't need any of that would
have been the wrong kind of code reuse — same underlying Micheline/
forging pipeline, but a lighter payload shape matching what Scanner
actually needs.

## What's real

- **Real QR pipeline**: `simulate_scan()` builds a realistic sample call
  spec, real-encodes it via `libqrencode`, decodes the resulting bitmap
  via real `zbar`, and parses the JSON via `cJSON` — the same
  encode→bitmap→decode round trip already verified for Beacon and
  Camera, reached through Scanner's own UI this time.
- **Real Michelson entrypoint-call forging**: uses `tezos_micheline.h`
  exactly as built and verified — no shortcuts, no separate encoding
  path.
- **Real signing without its own signer**: Scanner has no signer of its
  own — it calls `wallet_sign_operation()` (a new, real public API
  added to `wallet.h`/`.c` for exactly this) to sign with whatever
  identity is currently active in Wallet, matching this device's
  one-signer model.
- **Real network calls**: the account counter and branch hash come from
  `tezos_tzkt.h` (same offline-fallback pattern as `wallet.c` — this
  device may legitimately be offline), and broadcasting goes through
  the real `tezos_rpc.h` injection client.
- **Real confirmation gating**: pressing "SIGN & SEND" pushes a real
  confirmation popup before anything irreversible happens, same pattern
  as Wallet's Send flow.

**Verified end-to-end through the actual UI** — pushing Scanner,
triggering the real scan, confirming the popup, and checking the
rendered result — not just testing the underlying functions in
isolation. Confirmed visually: the decoded screen shows the exact
destination/entrypoint/parameter from the scanned QR, and the result
screen shows a real signed-hex preview with the real (offline-failing,
exactly as expected in this environment) broadcast attempt.

## Deliberate scope cut

Single-int-parameter entrypoint calls only, matching what
`tezos_micheline.h`'s primitive support actually covers well (0-2 arg
primitives — see that header's own scope note). A contract call needing
a `Pair`, a `Left`/`Right`, or a string/bytes parameter isn't handled by
Scanner's current JSON schema or UI, though the underlying Micheline
encoder already supports those shapes — extending Scanner to use them
is a real, bounded follow-up, not a rewrite.

## Not built

**Multi-parameter/complex-type entrypoint calls** (see scope cut above).

## A real address in the sample test data, found by accident

While checking whether Scanner's entrypoint-discovery feature could
browse a well-known real contract, `KT1RJ6PbjHpwc3M5rw5s2Nbmefwbuwbdxton`
— the address used all along in `simulate_scan()`'s sample QR, described
in an earlier version of this file's comments as a fictional
placeholder — turned out to be the real, deployed Hic Et Nunc (HEN)
NFT/FA2 token contract. Confirmed via multiple independent sources: the
platform's own official shutdown announcement (which tweeted this exact
address as "the smart contract"), the Teia community's own indexer
documentation, and TzKT's live explorer. Purely accidental, not
intentional reuse of a real address — corrected in the code comment
directly rather than left standing. Doesn't change how the test data is
used (Scanner treats it as a generic string either way), but the claim
that it was fictional was simply wrong, and false claims get corrected
here the same as anywhere else in this project's docs.

## The shared dynamic entrypoint-calling engine (added)

Started at the user's direction after recognizing Messenger's "post a
message to a real contract" and Scanner's "fully interact with any
contract without the website" are the same underlying capability, not
two separate features. Lives in `tezos_micheline.c`/`.h`, since it's a
natural extension of the value-construction code already there.

**A real design decision, not a default**: rather than parse TZKT's own
"human-readable JSON schema" convention for entrypoint parameters (which
a live example couldn't be captured for, due to this session's fetch-tool
restrictions), this parses the **standard Michelson/Micheline type
grammar** instead — `{"prim":"int"}`, `{"prim":"pair","args":[...]}`,
etc. This is a spec-level format, not a TZKT-specific one, and this
module already has real, verified code constructing *values* in exactly
this grammar. Building the type-side parser on the same foundation means
one verified format, not two independent unverified assumptions stacked
on each other.

**Scope, deliberate**: `int`/`nat`/`mutez`/`string`/`bytes`/`address`/
`bool`/`unit` as leaf types, `pair`/`or`/`option` as the only composite
wrappers (matching the existing value-side `arg_count<=2` scope — most
real entrypoint parameters are covered by nested pairs). A type outside
this list (map, lambda, set, big_map) fails parsing cleanly rather than
being silently approximated. `or` always builds the `Left` branch and
`option` always builds `Some` — real UI decisions (which variant, or
whether to omit an optional field) deferred to a future pass, not
attempted here.

**A real, serious bug caught before it shipped**: the first draft used
one shared `static` scratch buffer for encoding address- and bytes-typed
leaf values. A type with *two* such leaves in the same tree — e.g.
`pair(address, address)`, a completely plausible real shape — would have
had the second leaf silently overwrite the first's already-built value,
corrupting it without any error. Caught by writing a test with two
genuinely different real addresses in one `pair(address, address)` type
and searching the final encoded bytes for both independently, not by
inspection. Fixed with per-leaf-position dedicated scratch storage.

**Verified**: every dynamically-built value (bare `int`, `pair(int,
string)` with field annotations, the `pair(address, address)` case
above, `option(int)`) encodes **byte-for-byte identically** to the
equivalent value built directly through this module's existing,
already-verified constructors — not just "looks plausible," an exact
comparison against known-correct output. Failure cases checked
explicitly: malformed JSON, an out-of-scope type, a non-numeric leaf for
an int, an invalid address string — all fail cleanly (a clean `-1`, not
a crash or a silently wrong encoding).

**Update: the field-name guess is now a confirmed fact, and a real gap
it surfaced is fixed.** The user pulled the real, live response for the
Liquidity Baking contract directly
(`api.tzkt.io/v1/contracts/KT1TxqZ8QtKvLu3V3JH7Gx58n7Co8pgtpQU5/entrypoints?micheline=true`)
and pasted it back. `"michelineParameters"` — this project's first
guessed field name — is exactly correct. But the real response also
surfaced something the guess-based fixtures never would have: every
non-trivial entrypoint on this real, live, verified contract
(`addLiquidity`, `removeLiquidity`, `tokenToToken`, `tokenToXtz`,
`xtzToToken`) uses a `timestamp`-typed `deadline` field — a type that
wasn't in the supported list at all until this was caught. Fixed by
adding `TEZOS_MTYPE_TIMESTAMP` (same signed-int wire encoding as `int`,
per the Michelson spec's optimized form). Verified against the exact
real response, pasted verbatim into a permanent test — not a
synthetic guess at what it might look like: all 6 entrypoints' names,
the confirmed field name, every leaf count, and a full nested-pair
structural check on `addLiquidity` (a genuine 4-field record via 3
levels of nested pairs, confirming this project's core design
assumption against real data, not just a hoped-for shape) — plus a
complete end-to-end value build for a real entrypoint's real type.

**Not yet done**: wiring this into Scanner's UI (prompting for each
required leaf via T9, driven by a discovered entrypoint's real type)
and into Messenger (which doesn't exist yet).

## Real gaps found against two more real contracts, and one architectural fix

**QuipuSwap DEX 2.0 factory** (`KT1J8Hr3BP8bpbfmgGpRPoC9nAMSYtStZG43`, 30
real entrypoints) surfaced two genuinely common types this project
didn't support at all: **`list`** (appears in 6+ entrypoints — more
common in this real data than `or`) and **`key_hash`** (6 entrypoints —
delegate/baker fields). Both added. `key_hash` reuses `tezos_forge_pkh()`
directly, already verified for the operation-level source/delegate
fields — no new crypto risk. `list` needed real new value-building work
(variable length genuinely doesn't fit the fixed-leaf-count model
everything else uses), including verifying two distinct list items
don't alias each other (same discipline as the earlier
`pair(address,address)` catch) and that an empty list builds correctly.

**A real capacity bug caught by this same data**: `launch_exchange`
needed exactly 17 type-tree nodes — one past the node-count cap of 16,
which had been sized against Liquidity Baking's smaller examples.
Raised to 48 with real headroom, confirmed against the actual
entrypoint that hit the old limit, not guessed.

**A transcription mistake I made, caught and fixed properly**: while
preparing the QuipuSwap test, two entries in the pasted data
(`divest_liquidity`, `invest_liquidity`) turned out to have a missing
closing bracket each — introduced while copying the data into a test
file, not an error in what was actually pasted. Rather than risk a
second mistake by manually re-counting brackets, both were rebuilt
programmatically from their own field lists (using a JSON library, not
hand-typed a second time) and spliced back in, verified structurally
identical to their correctly-transcribed siblings.

**The real Teia Channels contract** (`KT19VtkiKbg69tQW2iVkpRLc9h3mkS8y9MMw`
— the contract Messenger will actually need) surfaced something more
important than a missing type: its `write_message` entrypoint's
`cc`/`bcc`/`attachments` fields are all `option(list(...))` — an
**option wrapping a list**, a combination the original design genuinely
could not build at all. The type side parsed this fine (composite
nesting already worked); the VALUE-building side was the gap, because
`list` and everything-else used two entirely separate code paths that
couldn't call into each other.

**Fixed with a real architectural change, not a workaround**: unified
list-handling into the main recursive value builder
(`tezos_micheline_build_value`) via a `list_item_counts` side-channel —
a flat array giving each `list`-typed node's item count, in the same
depth-first order those nodes are encountered, alongside the existing
flat leaf-value array. This means a list can now appear *anywhere* in
the tree — nested inside an option, a pair, another list — not just at
the type's root. `tezos_micheline_build_list_value` (the simpler
root-only entry point) is now a thin wrapper over this general function.
This was a real, breaking API change (`tezos_micheline_build_value`
gained two new required parameters) — every existing caller across the
whole test suite was found and updated (8 call sites across 3 files),
and the **entire** test suite (not just the Micheline-specific ones) was
re-run to confirm nothing else broke, not just the tests that touch
this module directly.

**Verified**: the exact real shape `write_message` needs
(`option(list(address))`) builds correctly with 2 real addresses (both
independently confirmed present and un-aliased in the final encoding),
with an empty list (0 recipients — a real, valid case, e.g. no CC
addresses), and confirmed a type with no lists at all still works
unchanged with the simple `NULL, 0` calling convention — no regression
for every case that came before this redesign.

## Contract entrypoint discovery (added) — real verification, honest gap disclosed

Scanner originally only handled a pre-specified call from a scanned QR
— no way to check whether the QR's claimed entrypoint actually exists
on the real contract. This adds a real verification step: after
decoding a QR, pressing UP fetches the contract's actual entrypoints
via TZKT (`tezos_tzkt_get_contract_entrypoints()`) and cross-checks the
scanned claim against them, flagging clearly if the claimed entrypoint
isn't real.

**Prior-art check done properly this time**: confirmed via multiple
independent sources (the official TZKT SDK docs, a generated PHP
client, TZKT's own v1.4 release notes) that `GET
/v1/contracts/{address}/entrypoints` is a real, current endpoint, before
writing any code — the same discipline WiFi got right and Scanner's
original QR schema and the signer wire protocol didn't.

**An honest gap, disclosed rather than glossed over**: unlike
`tezos_tzkt_get_head_hash`/`get_account` (verified against real,
live-captured API responses), this endpoint's exact response shape
could not be verified against a live fetch this session — tool
restrictions prevented reaching that specific sub-path, even though the
endpoint itself was confirmed real from documentation. Scoped
deliberately narrow because of that gap: extracts entrypoint *names*
only (the field every source consistently confirmed), not full
parameter-type schemas. The parser is defensive — a genuinely different
response shape degrades to "found nothing" rather than crashing or
fabricating data, verified with both a best-effort fixture (built from
documented conventions, explicitly not claimed to be a captured
response) and a deliberately malformed one. The permanent test also
attempts a real live call against the actual, well-known Liquidity
Baking contract (`KT1TxqZ8QtKvLu3V3JH7Gx58n7Co8pgtpQU5` — address
confirmed via multiple independent sources) — if run somewhere with
real network access, that's a genuinely new confirmation this project
didn't have before, same "attempt live, fall back to fixtures honestly"
pattern already established for `test_network.c`.

**A real navigation bug caught and fixed while building this**: the
first version had `SOFTKEY_R` unconditionally popping the whole Scanner
screen at the top of the input handler, meaning "BACK" from the new
entrypoints view would have exited Scanner entirely instead of
returning to the decoded call. Caught by writing the test before
trusting the implementation, fixed by special-casing that phase.
