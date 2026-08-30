# Signer Wire Protocol

The protocol the main board (Pi) speaks to the signer (STM32F401 or Pi
Pico) over the physical UART link. This spec exists so real signer
firmware can be written against something precise and unambiguous —
"the Pi and signer agree on bytes," not "the Pi and signer both guess."

**Security note, stated plainly**: this protocol has no cryptographic
protection of its own (no encryption, no MAC beyond a basic corruption
checksum) — and that's fine. The wire is a short, physically airgapped
UART link, not a network. The actual trust boundary is the signer's own
physical confirmation step before it ever signs anything (see
`docs/build-notes/settings-app-build-notes.md`'s security note) — the
protocol's job is reliable framing, not authentication.

## Framing

```
[0x01 START] [LEN: u16 BE] [TYPE: u8] [PAYLOAD: LEN-1 bytes] [CHECKSUM: u8]
```

- `START` (`0x01`) lets a receiver resync after noise/garbage on the wire.
- `LEN` covers `TYPE` + `PAYLOAD` (not `START`, `LEN` itself, or `CHECKSUM`).
- `CHECKSUM` = XOR of every byte from `TYPE` through the end of `PAYLOAD`.
  Catches transmission corruption; not a security mechanism (see above).

## Message types

| Type | Direction | Name | Payload |
|---|---|---|---|
| `0x01` | Pi → Signer | `PING` | (empty) |
| `0x81` | Signer → Pi | `PONG` | firmware version string, length-prefixed (1 byte len + bytes) |
| `0x02` | Pi → Signer | `GET_ADDRESS` | `account_index: u32 BE` |
| `0x82` | Signer → Pi | `ADDRESS_RESPONSE` | 36-byte `tz1...` address string |
| `0x03` | Pi → Signer | `SIGN_REQUEST` | `account_index: u32 BE` + `forged_len: u16 BE` + forged operation bytes |
| `0x83` | Signer → Pi | `SIGN_RESPONSE` | raw 64-byte signature |
| `0xFF` | Signer → Pi | `ERROR_RESPONSE` | `error_code: u8` + message string (1 byte len + bytes) |

## Error codes (`ERROR_RESPONSE` payload)

| Code | Meaning |
|---|---|
| `0x01` | Malformed request (bad framing/checksum/length) |
| `0x02` | Unknown message type |
| `0x03` | User rejected the signing request at the physical confirmation step |
| `0x04` | Internal signer error |

## What "SIGN_REQUEST" actually means on real hardware

On real signer hardware, receiving a `SIGN_REQUEST` triggers the
physical confirmation step — however that's implemented (a button, a
small display showing the decoded operation, whatever the actual signer
board design ends up being). The signer does **not** respond until the
user has confirmed or rejected. This means, on real hardware, `PING`/
`GET_ADDRESS` should return quickly, but `SIGN_REQUEST` may take
arbitrarily long (human-timescale) before a response arrives — the Pi
side's serial read should account for this and not assume a short
timeout, the way it reasonably could for the other two message types.

**The software simulation (`tezos_signer_protocol.c`) auto-confirms
every `SIGN_REQUEST` immediately** — there's no physical button to wait
for in software. This is explicitly a simulation gap, not something to
carry forward into real firmware: real firmware must implement an actual
wait-for-confirmation step, and the "user rejected" error path
(`0x03`) has to be real handling, not a case that can never actually
occur.

## Account derivation happens on the signer, never on the Pi

Per the profile architecture decision: the signer holds the one seed
(from the recovery phrase) and derives the requested account's keypair
on demand for each `GET_ADDRESS`/`SIGN_REQUEST` — it never sends the
seed, or any other account's key material, to the Pi. The Pi only ever
receives the specific derived address or signature it asked for. The
software simulation matches this: `tezos_signer_protocol_process()`
holds the mnemonic/passphrase (standing in for what the real signer's
secure element would hold) and derives fresh per request, never caching
or exposing the seed itself across the simulated wire.

## What's verified here vs. what's still open

**Verified**: message framing encode/decode round-trips correctly for
all message types, checksum validation correctly rejects corrupted
frames, and a full simulated `GET_ADDRESS` → `SIGN_REQUEST` → verify
round trip produces a real, correctly-signed operation — reusing the
same forge/sign/verify pipeline already verified against Taquito's
golden vectors, just reached through the protocol layer instead of
calling the signer functions directly. See
`docs/build-notes/wallet-app-build-notes.md`.

**Not yet done, and genuinely can't be from this environment**: real
UART transport (this is all in-memory byte buffers, not an actual serial
port), and — the big one — actual STM32/Pico firmware implementing this
spec, including the real physical confirmation step. Both need real
hardware in the loop.
