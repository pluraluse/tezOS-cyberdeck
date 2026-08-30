#pragma once
/*
 * Beacon pairing payload — the actual content encoded into (or scanned
 * from) a pairing QR code. Verified against the real beacon-sdk source:
 *   - Peer info fields: packages/beacon-types/src/types/PeerInfo.ts
 *   - Encoding pipeline: JSON.stringify(peerInfo) -> bs58check.encode()
 *     (packages/beacon-core/src/Serializer.ts) -> wrapped as
 *     "tezos://?type=tzip10&data={code}"
 *     (packages/beacon-ui/src/utils/get-tzip10-link.ts,
 *     packages/beacon-ui/src/components/qr/index.tsx)
 *
 * This is the piece the camera's Scan mode needs to decode a QR code
 * Kukai displays (scanning direction), and what this device would
 * encode if it ever displays its own pairing QR (encoding direction,
 * e.g. for acting as a Beacon signer for other dApps).
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char id[64];
    char name[64];
    char public_key[80];  /* hex-encoded Ed25519 public key, per PeerInfo */
    char version[16];
    char relay_server[128];
    char icon[256];       /* optional, may be empty */
    char app_url[256];    /* optional, may be empty */
} tezos_beacon_peer_info_t;

/* Builds the full "tezos://?type=tzip10&data=..." URI string for this
   peer's own info -- what this device would show if displaying its own
   pairing QR. */
bool tezos_beacon_build_pairing_uri(const tezos_beacon_peer_info_t *info,
                                    char *uri_out, size_t uri_cap);

/* Parses a scanned "tezos://?type=tzip10&data=..." URI (or a bare
   bs58check-encoded data string, in case the QR only contained that)
   back into peer info -- what Camera's Scan mode needs after zbar hands
   back the decoded QR text. */
bool tezos_beacon_parse_pairing_uri(const char *uri, tezos_beacon_peer_info_t *out);
