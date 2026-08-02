# Wallet App — Build Notes

The first real app screen built on the core engine (beyond Idle), and
the first to exercise a real backend: operation forging and signing,
both genuinely implemented and rigorously verified — not mocked logic
behind a real-looking UI.

## What's real here

**`src/chain/tezos_base58.c`** — Base58Check encode/decode for all
Tezos address/key formats (tz1/tz2/tz3/KT1/edpk/edsk-seed/edsig/sig/
block-hash). Prefix byte values and the checksum scheme verified
directly against Taquito's reference implementation (`@taquito/utils`),
not reconstructed from memory. **Verified against a real golden vector**:
`tz1gvF4cD2dDtqitL3ZTraggSR1Mju2BKFEM` decodes to exactly
`e96b9f8b19af9c7ffa0c0480e1977b295850961f`, matching Taquito's own
docstring example byte-for-byte, with full round-trip (decode → encode
→ same string) confirmed for both a tz1 and a KT1 address.

**`src/chain/tezos_forge.c`** — Operation forging: zarith (variable-length
integer) encoding, address/PKH encoding, and the reveal/transaction/
delegation operation schemas (exact field order, exact tag bytes —
`0x6b`/`0x6c`/`0x6d`/`0x6e`). Every encoding rule here was checked
against Taquito's `@taquito/local-forging` source
(`schema/operation.ts`, `constants.ts`, `codec.ts`) line by line — tag
values, the `boolEncoder` convention (`0xff`/`0x00`, not `0x01`/`0x00`),
the optional-`delegate` field encoding, the parameters field's
no-entrypoint-call encoding. **Verified two ways**: against Taquito's
own `kindMapping` constants directly, and against an independently
written Python reference implementation of the same spec — the C and
Python outputs match byte-for-byte for a full transaction (content and
full operation group, branch included).

**`src/chain/tezos_signer.c`** — The mock software signer: derives an
ed25519 keypair from a seed, signs forged operation bytes with the
correct watermark+hash convention (prepend `0x03`, blake2b-256 hash,
ed25519-sign the hash — confirmed against Taquito's `provider.ts`
watermark call site and `ed-key.ts`'s sign implementation), and can
self-verify a signature against its own public key.
**Verified against Taquito's own test vector, exactly**: seed
`edsk4TjJWEszkHKono7XMnepVqwi37FrpbVt1KCsifJeAGimxheShG` derives public
key `edpkuhmrbunxumoiVdQuxBZUPMmwkPt7yLtY5Qnua3VJVTLWr3vXXa` and address
`tz1b9kV41KV9N3sp69ycLdSoZ2Ak8jXwtNPv` — both matched exactly — and
signing hex message `1234` with watermark `[3]` produces
`sigpKAnfQGzG4Rk5pV7z9mx2TL9veQCHD7qN4PhsUZMj1BqsumBoApBS9Ue616vKVymxrzfZE2L4h27zzxRUVy6BNPRMpufb`,
matching Taquito's own test suite output exactly.

**`src/apps/wallet/wallet.c`** — Real screens (home + Send confirm)
built on the core engine, genuinely exercising the pipeline above:
pressing SELECT on Send forges a real transaction, signs it with the
mock signer, and self-verifies the result — confirmed via integration
test (rendered frame checked for the exact cyan "SIGNED"/"self-verify:
OK" pixels that only appear on the success path, not just visual
inspection).

## What's still a placeholder, marked explicitly in the code

- **The mock signer itself.** `DEV_TEST_SEED` in `wallet.c` is Taquito's
  own public test vector, reused deliberately so it's unambiguous this
  isn't a real fund-holding key. This stands in for the real STM32/Pico
  signer hardware, which doesn't exist yet.
- **Balance display** — hardcoded, no network layer exists to query TZKT.
- **Branch and counter** — `DEV_PLACEHOLDER_BRANCH` and a hardcoded
  counter of `"1"` in `wallet.c`'s `perform_send()`. Real values need a
  network call to fetch the current head block hash and the account's
  actual counter from a node/indexer.
- **Recipient and amount are hardcoded**, not user-entered — blocked on
  the T9 text-entry widget, which doesn't exist yet (see
  `docs/architecture/CORE-ARCHITECTURE.md`'s still-open list).
- **No broadcast.** The signed operation is never sent to
  `/injection/operation` — no network layer exists yet.
- **No entrypoint/parameters support in `tezos_forge_transaction`** —
  plain tez transfers only. Contract calls (needed by Scanner, HEN
  minting, Messenger's `post_message`) need the parameters field's
  Michelson-value encoding added, not yet implemented.

## Why build it in this order

Verifying the crypto/encoding layer against real, precise reference
vectors *before* wiring it into a UI meant any mistake would show up as
a clean test failure with an exact expected-vs-actual diff, rather than
a subtle bug discovered later when something real is on the line. Same
methodology as the render engine (proven in software before real DRM
hardware) and the shim (compile-checked against real libdrm headers
before real hardware) — verify what's checkable now, mark plainly what
still needs real hardware/network/a widget that doesn't exist yet.
