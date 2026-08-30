#pragma once
/*
 * Tezos RPC client — specifically operation injection. This is the
 * missing final link: the device can forge and sign a real operation
 * (src/chain/tezos_forge.c, tezos_signer.c), but until now nothing
 * actually submitted it to the chain. TZKT (tezos_tzkt.h) is a
 * read-only indexer — injection needs an actual Tezos RPC *node*, a
 * different kind of service entirely.
 *
 * Endpoint and request/response shape verified against the real Tezos
 * RPC documentation and a real-world forge/sign/inject example
 * (POST /injection/operation, body is a bare JSON *string* containing
 * the hex-encoded signed operation — not a JSON object with a field —
 * confirmed by an actual working curl example, not guessed).
 *
 * HONEST LIMITATION, same shape as every other network-touching piece
 * in this project: request construction and response parsing are real
 * code, verified against the real spec/example. A live injection
 * cannot be tested from this environment — that needs a real,
 * *funded* testnet account (an actual transaction, not a read-only
 * query) and unrestricted network access, i.e. real hardware.
 *
 * No default node URL is hardcoded here deliberately — public RPC
 * providers change, go offline, or get deprecated, and guessing one
 * would be worse than requiring the caller to supply it explicitly
 * (Settings' eventual "RPC node" field, or a sensible default chosen
 * and kept up to date at the UI layer, not buried in this header).
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Injects a signed operation (forged bytes + the 64-byte signature
   already appended -- exactly what tezos_mock_signer_sign produces in
   place). `node_base_url` e.g. "https://some-rpc-node.example".
   On success, writes the Base58Check-encoded operation hash to
   `op_hash_out`. Returns false on transport failure OR on the node
   rejecting the operation (check op_hash_out[0] == '\0' plus the
   return value to distinguish "never reached the node" from "node
   said no" if that distinction matters to the caller -- both are
   currently folded into a single false, since either way nothing got
   injected). */
bool tezos_rpc_inject_operation(const char *node_base_url,
                                const uint8_t *signed_bytes, size_t signed_len,
                                char *op_hash_out, size_t op_hash_cap);
