# Messenger App — Build Notes

## Two sources, two confidence levels — read this first

1. **Reverse-engineered from `teia-ui`'s live frontend source** (see
   `hardware-roadmap-build-notes.md`'s earlier research): contract
   `KT19ooSLPFxJQ5mx3kR4Qo2UY4KJDcdMdng9`, constants `CHANNELS_V2_CONTRACT`,
   `CHANNEL_FEE = 100000`, `CHANNEL_MESSAGE_FEE = 25000`. This is what the
   actual production Teia app calls today — highest confidence for "what
   to actually build against."
2. **Authoritative SmartPy source + formal docs**, from an **open, unmerged,
   single-author, zero-review PR** (teia-community/teia-smart-contracts
   #18, branch `teia-messaging-network-token`, opened by `josim`):
   much richer detail (full entrypoint list, storage layout, Merkle proof
   algorithm, error codes, governance model) — but describes a suite that
   has visibly evolved within the PR's own commit history (fees, ban
   list, message history, and an `update_channel` entrypoint were all
   added over the branch's lifetime). **Don't assume every detail here
   matches the deployed contract's live entrypoints/storage — confirm
   against TZKT before writing real forge/sign code.**

## Channels (`channels.py`) — what this app actually integrates with

Three roles per channel: **Creator** (immutable, set at `create_channel`),
**Admin** (added by creator via `update_channel_admins`), **User** (a
Merkle-proof member in `allowlist` mode). Three access modes:
- `unrestricted` — anyone posts
- `allowlist` — creator/admin bypass; everyone else needs a valid proof
  against `channel.merkle_root`
- `closed` — creator/admin only; this is the DM primitive (2-person DM =
  `closed` channel, no merkle root, peer added as the single admin)

**Building a Merkle proof** (needed for any allowlist channel our device
posts into): leaf = `blake2b(pack(address))`, no per-channel salt. Build
a binary tree bottom-up with `blake2b(left || right)`, publish the
address list at `merkle_uri` (IPFS), submit `proof` as a list of
`{direction, sibling}` steps with `post_message`. A single-address tree's
proof is just `Some([])`. Full algorithm in the PR's README — port this
logic directly rather than re-deriving it.

Key entrypoints: `create_channel`, `configure_channel`, `update_channel`,
`update_channel_admins`, `hide_channel`, `post_message`, `delete_message`.
Deletion rule: `closed` → sender only; `unrestricted`/`allowlist` →
sender or creator. Content stored on-chain as `bytes` (≤32KB) in the
`messages` big_map directly — this is a change worth double-checking
against the live contract, since our earlier `teia-ui` research found
content pointed at IPFS rather than stored inline; the PR's storage
layout shows `content: bytes` directly in the `message` record. **This
discrepancy is exactly the kind of thing to verify against TZKT before
building** — if it's genuinely inline now, that changes whether our
device needs an IPFS-pinning step to post a message at all.

## Token Gated Chat (`token_gate.py`) — solves our builders'-channel problem

This is new information, and it directly replaces a design we'd shelved.
Recap of the problem: we wanted a "Cyberdeck builders" channel gated by
a proof-of-build token, but Channels' only gating mechanism is a
Merkle-allowlist — meaning gating an *evolving* membership (new builders
joining over time) requires **manually recomputing and republishing the
address list and Merkle root** every time someone new qualifies. We'd
flagged this as a real maintenance burden with no live-membership option.

Token Gated Chat removes that problem entirely:
- **Every FA2 token automatically gets a chat room** — no `create_channel`
  step at all. Room key = `(fa2_contract_address, token_id)`.
- Gating is **live**, checked via the FA2 `balance_of` callback on every
  post — not a snapshot. If someone acquires the proof-of-build token,
  they can post immediately; if they transfer it away, they lose access
  immediately. No Merkle tree, no republishing, no admin maintenance step.

**This should replace the Merkle-allowlist plan for the builders'
channel.** Mint the proof-of-build token (reusing the HEN v2-style
minting pipeline already scoped for Camera/Art), and the corresponding
Token Gated Chat room exists automatically with live, self-maintaining
membership. Simpler architecture, no periodic upkeep task for anyone to
forget.

**Not yet confirmed**: whether this contract is deployed to mainnet at
all — the PR's README deployment table only lists Poll Comments
addresses, not Token Gated Chat's. Check TZKT / ask the contact who
pointed us here before assuming it's live and usable today.

## Poll Comments (`poll_comments.py`) — confirmed live, bonus integration surface

Deployed and documented with real addresses:
- **Mainnet**: `KT1HjHTTVa7hc9C2NGQh6H3m2gNKRDZPtg86`
- **Shadownet**: `KT1G7UK1hvM7yVbGoL6h8uKLmcYGNKGqas1j`

Comments on off-chain teia.art polls, gated by holding a single
configured Teia FA2 token (checked live via `balance_of`, same pattern as
Token Gated Chat). Full edit history (`comment_history` big_map, every
edit archives the prior version, queryable forever via
`get_comment_version`). Contract-wide ban list, governance-gated. Not
core to Messenger, but worth keeping in mind for Discover/Explorer if
either ever wants to surface poll discussion.

## Token Comments (`token_comments.py`) — also worth noting for Discover/Gallery

Same model as Poll Comments, but the gated resource is an *arbitrary*
`(fa2_address, token_id)` pair rather than one configured Teia token —
meaning any NFT could have a comment thread. Worth considering for
Gallery/Discover later (comment on an owned or browsed NFT) — not
scoped now, just flagging the surface exists.

## Action items before writing real Messenger code

1. **Pull the live contract's actual storage/entrypoints from TZKT**
   (`KT19ooSLPFxJQ5mx3kR4Qo2UY4KJDcdMdng9`) and diff against this PR's
   README — especially the inline-content-vs-IPFS-pointer question above.
2. **Confirm whether Token Gated Chat is deployed anywhere** before
   committing the builders'-channel design to it.
3. Port the Merkle proof construction algorithm faithfully if Channels'
   allowlist mode is still needed for anything else (it remains the
   right tool for channels with genuinely static/curated membership,
   just not for the builders' channel anymore).
