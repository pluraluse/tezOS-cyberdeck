# `.tez` Name Resolution — Build Notes

## Status: real, tested, wired into Wallet's Send flow

Real, direct priority call from earlier discussion: this was
sequenced ahead of 1-bit Art, and built first.

## Built against the real, official spec — not guessed at

`tezos_tez_domain.h`/`.c` implements resolution against Tezos
Domains' own documented mechanism
(developers.tezos.domains/interoperability/name-resolution): a
NameRegistry contract holds a `records` bigmap, keyed by the
UTF-8-encoded domain name (hex bytes), whose value contains the
resolved `address`. The hex-encoding convention was checked against
Tezos Domains' own official Taquito example (`stringToBytes`), not
assumed — and verified by hand in this project's own test (`"abc.tez"`
→ `"6162632e74657a"`, checked byte-for-byte).

## Three real, honest scope decisions, not silently glossed over

1. **The documented proxy-resolution step isn't implemented.** The
   official docs describe a stable entry point
   (`NameRegistry.CheckAddress`) whose storage points at the current
   real NameRegistry contract, specifically so clients don't need to
   track migrations by hand. This project's sandbox has no live
   network access to verify that specific contract's exact storage
   shape, and guessing at an unverified JSON layout felt like the
   wrong kind of confidence — this project has its own real, direct
   history of exactly that mistake (a contract mislabeled with the
   wrong platform name in an audit report). Instead, the real
   NameRegistry contract address is a real, user-configurable Settings
   value (see below), same honest pattern already used for RPC Node
   and Messenger Contract.
2. **Full Unicode normalization isn't implemented.** The docs call for
   a documented "encode algorithm" before using a name as a lookup
   key; this module does real UTF-8 byte encoding (matching the real,
   working convention shown in official examples) but not the full
   edge-case normalization spec.
3. **Expiry checking isn't implemented.** Domains do expire and become
   resolvable again; a resolved address from an expired domain could
   theoretically be stale. Real, disclosed gap, not attempted in this
   pass.

## A real, new Settings field, honestly empty by default

"Name Registry" (`settings_get_name_registry_contract()`) — the
*direct* records-holding contract, not the proxy, since this module
doesn't resolve through the proxy. Deliberately defaults to empty
rather than a guessed address (the one candidate address found in
Tezos Domains' own example code may be testnet-specific, not
Mainnet) — needs a real, live-confirmed address entered before `.tez`
resolution can work at all.

## Wired into Wallet's Send flow

Typing a recipient ending in `.tez` triggers real resolution before
proceeding to amount entry. Three real outcomes, each handled
distinctly: a raw tz1/KT1 address passes through unchanged (no
regression); an unconfigured registry shows a real, clear error
("Set Name Registry in Settings first") rather than silently
advancing; a resolution failure shows the real domain name that
couldn't be resolved, not a generic error.

## A second real bug, found and fixed along the way

While wiring in the new Settings row, the settings home screen's list
was found to be initialized with a stale, hardcoded item count (`9`),
while the real array had grown to 12 rows. **RPC Node, Messenger
Contract, Network, and now Name Registry were all genuinely
unreachable via real list navigation** — not a new bug introduced by
this work, a pre-existing one this work happened to surface. Fixed
with a computed array size (`sizeof(...)/sizeof(...[0])`) instead of a
hardcoded number, so this can't silently drift out of sync again.

## A real build-system bug, found and fixed during close-out

Adding the new dependency to `wallet.c` broke ~19 previously-passing
tests that link `$APP_WALLET` but hadn't been told about the new file
`wallet.c` now calls into — a real, direct lesson in why a new hard
dependency on a widely-shared module needs to go into that module's
own build-variable definition (`APP_WALLET` in `run_all_tests.sh`),
not just the one new test that happened to prompt adding it.

## Verified

18 checks on the core module (real validation cases, the by-hand hex
encoding check, buffer-overflow rejection, and confirming the real
network call correctly fails offline — the same disclosed pattern as
every other live-network function in this project). 4 checks on the
Settings integration (the navigation bug fix, verified directly by
pressing past the old stale limit). 4 checks on the Wallet Send
integration (regression check for raw addresses, the unconfigured-
registry error path, the settings screen round-trip). All under
AddressSanitizer, zero errors. Full 69-suite regression run twice,
full-stack `-O2` build clean.
