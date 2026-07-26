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
| Profiles | See below — PIN-protected personalization layers, NOT separate wallets. Add / Switch / Remove. |
| Lock / Passcode | Device-level PIN gating screen access. Independent of signing — signing is always gated by the physically separate signer's own button confirmation, regardless of profile or lock state. |
| Wi-Fi | SSID scan/list, T9 PSK entry, saved-network list. |
| Storage | microSD space management (this is the only storage tier on this hardware — Pi boots from and stores everything on the card). Total/free space, breakdown by category (Camera captures, Gallery cache, app data, logs), clear-cache action. |
| Services | Network selection (Mainnet vs. Ghostnet/testnet) — needed for development and for anyone trying the device without risking real tez. RPC/TZKT endpoint override. IPFS pinning-service credentials. |
| About / Reset | Firmware version, signer firmware version, factory reset. |

## Profile architecture (the "users" decision)

**One wallet, one tz address, one signer key — always.** Profiles do not
create separate identities. A profile is a personalization/view-state layer:

- Favorited NFTs (Gallery)
- Saved watch-addresses (Explorer)
- Saved contract addresses (Scanner)
- Read/unread state (Messenger)
- Default Camera resolution/depth preset
- Theme/sound/UI preferences

All profiles see the **same** underlying chain data (same balance, same
owned tokens, same channels) since it's the same address. Profiles only
differ in what's been personally saved/favorited/configured on top.

**Security note, worth keeping explicit in UI copy later**: the profile PIN
gates access to the personalization layer only. It has no bearing on
signing. Never present it in a way that implies it protects funds — that's
the airgapped signer's job, via its own physical button confirmation,
independent of which profile is active or whether the device is "unlocked."

**Implementation**: small local keyed storage per profile (not on-chain, not
shared). Lock/idle screen = PIN-entry gate that loads the matching profile's
saved state. Reasonable default cap on profile count (small-group/family
scale, not open-ended).

### Profile switching — discreet by design

The lock/idle screen **never shows a profile picker/list**. It always just
prompts for a PIN, blind. Whichever PIN matches determines which profile
silently loads. This is deliberate: a visible list of profiles is itself an
information leak — it tells an onlooker how many profiles exist and, from
any labels/icons, likely which is the primary/admin one. With blind PIN
entry, an onlooker sees someone type a PIN and get into "their device" —
no visible branching, no hierarchy tells.

- Fast switching = re-enter PIN from idle/lock, not a menu-diving flow.
- No profile names/labels ever surfaced during the switch flow itself
  (labels can exist internally in Settings for the owner's own reference,
  but never shown to an onlooker during the actual unlock).
- **Not built by default, but worth knowing exists**: duress/decoy PINs
  (a specific PIN that loads a deliberately unremarkable decoy profile) are
  a real, heavier security pattern used in some devices. This is a bigger
  feature with its own tradeoffs — revisit only if "discreet" ever needs to
  become "safe under coercion" specifically, not part of v1 profile design.

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
- Active wallet's tz1 address (fine to show here — this is the owner
  viewing their own device, not an onlooker during a profile switch)
