# Settings App + Input Model — Build Notes

## Input model

- **Softkeys (2) + Up/Down (2)**, separate from the 12-key T9 numeric matrix.
- All 4 wired into spare GPIO pins on the TCA8418 keypad decoder (I2C) —
  zero additional Pi GPIO cost, same interrupt line as the numeric matrix.
- Navigation grammar: Up/Down scrolls lists/menus. Left softkey = contextual
  action ("Select"/"Options"). Right softkey = "Back"/"Menu".
- **Decided: dedicated discrete tactile switches for Up/Down** (not overloaded
  onto numeric keys 2/8 — context-dependent key meaning is more confusing on
  new hardware than on a phone a user already knows intimately).

## Settings app scope

| Section | Contents |
|---|---|
| Device Name | Free-text string (T9 entry). Shown in Messenger profile, Beacon pairing handshake, etc. |
| Profiles | See below — PIN-protected, each mapping to a separate HD-derived account/address from one seed. Add / Switch / Remove. |
| Lock / Passcode | Device-level PIN gating screen access. Independent of signing — signing is always gated by the physically separate signer's own button confirmation, regardless of profile or lock state. |
| Wi-Fi | SSID scan/list, T9 PSK entry, saved-network list. |
| Storage | microSD space management (this is the only storage tier on this hardware — Pi boots from and stores everything on the card). Total/free space, breakdown by category (Camera captures, Gallery cache, app data, logs), clear-cache action. |
| Services | Network selection (Mainnet vs. Ghostnet/testnet) — needed for development and for anyone trying the device without risking real tez. RPC/TZKT endpoint override. IPFS pinning-service credentials. |
| About / Reset | Firmware version, signer firmware version, factory reset. |

## Profile architecture (the "users" decision — revised)

**One seed, multiple derived accounts — each profile is a genuinely
separate address, not a skin on one shared address.** Corrected from an
earlier version of this doc that had all profiles sharing one address —
that doesn't work: it defeats the point of separate alts and undermines
the discreet-switching design below, since anyone who ever saw one
profile's balance would see all of them.

- The signer generates **one seed** during initial device setup, backed up
  as a single recovery phrase. It never leaves the signer.
- Each profile/"bank" (0/1/2/3...) is a **child key derived from that seed
  at a different account index** — HD (hierarchical deterministic)
  derivation. Genuinely distinct keypair, genuinely distinct tz address,
  genuinely separate on-chain balance and NFT holdings per profile.
- **Curve note**: Tezos's default curve is ed25519 (tz1). Its HD derivation
  standard (SLIP-0010) only supports *hardened* derivation — every path
  segment needs a hardened index, unlike secp256k1's more flexible scheme.
  Use a proper SLIP-0010 ed25519 derivation library, not a naive BIP32 port.
- **Path convention — open decision**: match the derivation path Ledger/
  Temple/Kukai already use (something like `m/44'/1729'/account'/0'` —
  1729 is Tezos's registered coin type) if the same seed phrase should be
  importable into other wallets, versus a custom path if recovery is meant
  to stay within this device's own software. Not yet decided — pick this
  before the signer's derivation logic is implemented, since it's harder
  to change after seeds exist in the wild.
- One local, per-profile personalization layer still exists on top (see
  list below), it's just no longer the *only* thing that differs between
  profiles:
  - Favorited NFTs (Gallery)
  - Saved watch-addresses (Explorer)
  - Saved contract addresses (Scanner)
  - Read/unread state (Messenger)
  - Default Camera resolution/depth preset
  - Theme/sound/UI preferences

**What the signer actually does**: holds the one seed, derives the
requested account's keypair on demand, and never exposes the seed or other
accounts' keys to the main board — the main board only ever receives the
specific derived address (for display/queries) or signature (for that one
account) it asked for.

**Security note, worth keeping explicit in UI copy later**: selecting a
profile now has a real functional role (it picks which account/address is
active), but it still doesn't bypass signing security — every operation
still requires the airgapped signer's own physical button confirmation,
for whichever account is active. The profile PIN selects an identity; it
never substitutes for that confirmation.

**Recovery consideration**: one seed phrase recovers every bank's keys.
The PIN → account-index mapping itself is local device state, not
on-chain — needs an explicit recovery flow (simplest: re-enter "this PIN
is account index N" manually on a recovered/new device).

**Implementation**: small local keyed storage per profile for the
personalization layer (not on-chain, not shared). Lock/idle screen =
PIN-entry gate that both selects the active account/address *and* loads
the matching profile's saved personalization state. Reasonable default
cap on profile count (small-group/family scale, not open-ended).

### Profile switching — discreet by design

The lock/idle screen **never shows a profile picker/list**. It always just
prompts for a PIN, blind. Whichever PIN matches determines which profile
(and now, which distinct address/account) silently loads. This matters
more now than it did under the old shared-address model: a visible list
would enumerate not just personalization profiles but genuinely separate
accounts/addresses — a real information leak, not just a cosmetic one.
With blind PIN entry, an onlooker sees someone type a PIN and get into
"their device" — no visible branching, no hierarchy tells, no account
enumeration.

- Fast switching = re-enter PIN from idle/lock, not a menu-diving flow.
- No profile names/labels ever surfaced during the switch flow itself
  (labels can exist internally in Settings for the owner's own reference,
  but never shown to an onlooker during the actual unlock).
- **Not built by default, but worth knowing exists**: duress/decoy PINs
  (a specific PIN that loads a deliberately unremarkable decoy
  profile/account) are a real, heavier security pattern used in some
  devices. This is a bigger feature with its own tradeoffs — revisit only
  if "discreet" ever needs to become "safe under coercion" specifically,
  not part of v1 profile design.

## About screen — full device specifics

- tezOS version + build/commit hash
- Signer firmware version (tracked separately — different board)
- Pi model/revision, serial number
- Camera module ID, display panel model
- microSD total/free space
- Battery charge % (via PiSugar fuel gauge)
- WiFi SSID, IP address
- Signer link status (connected/disconnected)
- Device uptime
- Active profile's derived tz1 address (fine to show here — this is the
  owner viewing their own device, not an onlooker during a profile switch)
