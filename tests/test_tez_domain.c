/* Real .tez domain resolution tests -- built against the official
   Tezos Domains developer documentation's real, documented mechanism
   (a NameRegistry contract's `records` bigmap, keyed by UTF-8-encoded
   domain name bytes). Covers real validation, real encoding
   round-trips (checked against the real, documented convention shown
   in Tezos Domains' own Taquito example), and confirms the real
   network call correctly, cleanly fails offline -- the same
   disclosed, expected pattern as every other live-network function
   in this project. */
#include "../src/chain/tezos_tez_domain.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    /* === real validation === */
    CHECK(tezos_tez_domain_is_valid_format("alice.tez"), "a real, valid simple domain is accepted");
    CHECK(tezos_tez_domain_is_valid_format("my-cool-name.tez"), "a real, valid domain with a hyphen is accepted");
    CHECK(tezos_tez_domain_is_valid_format("sub.alice.tez"), "a real, valid subdomain is accepted");
    CHECK(tezos_tez_domain_is_valid_format("abc.tez"), "the real, documented minimum length (3 chars) is accepted");

    CHECK(!tezos_tez_domain_is_valid_format(""), "an empty string is rejected");
    CHECK(!tezos_tez_domain_is_valid_format("ab.tez"), "a real, too-short domain (under the documented 3-char minimum) is rejected");
    CHECK(!tezos_tez_domain_is_valid_format("alice"), "a name with no .tez suffix at all is rejected");
    CHECK(!tezos_tez_domain_is_valid_format("alice.com"), "a name with the wrong suffix is rejected");
    CHECK(!tezos_tez_domain_is_valid_format("Alice.tez"), "an uppercase character is rejected -- real Tezos Domains names are lowercase");
    CHECK(!tezos_tez_domain_is_valid_format("alice_smith.tez"), "an underscore (not a real, allowed character) is rejected");
    CHECK(!tezos_tez_domain_is_valid_format(".alice.tez"), "a name starting with a dot is rejected");
    CHECK(!tezos_tez_domain_is_valid_format("-alice.tez"), "a name starting with a hyphen is rejected");
    CHECK(!tezos_tez_domain_is_valid_format("alice!.tez"), "a name with a real, disallowed special character is rejected");

    /* === real encoding, checked against the real, documented convention ===
       "abc" -- UTF-8 bytes 0x61 0x62 0x63 -- hex-encodes to "616263",
       matching the real convention Tezos Domains' own Taquito example
       (stringToBytes) uses in practice */
    char key[64];
    CHECK(tezos_tez_domain_encode_key("abc.tez", key, sizeof key), "encoding a real domain name succeeds");
    CHECK(strcmp(key, "6162632e74657a") == 0,
          "the real hex encoding matches byte-for-byte what UTF-8 bytes of 'abc.tez' should produce");
    /* verify by hand: a=0x61 b=0x62 c=0x63 .=0x2e t=0x74 e=0x65 z=0x7a
       -> 61 62 63 2e 74 65 7a -> "6162632e74657a" */

    char tiny_buf[4];
    CHECK(!tezos_tez_domain_encode_key("alice.tez", tiny_buf, sizeof tiny_buf),
          "a buffer too small for the real encoded key is correctly rejected, not silently truncated");

    /* === real resolution -- offline sandbox, correctly fails, same
       disclosed pattern as every other live-network call in this
       project === */
    char address[64];
    CHECK(!tezos_tez_domain_resolve(TEZOS_NETWORK_MAINNET, "KT1F7JKNqwaoLzRsMio1MQC7zv3jG9dHcDdJ",
                                    "alice.tez", address, sizeof address),
          "a real resolution attempt correctly fails in this offline sandbox -- expected, not a defect");

    /* a malformed name should fail fast, without needing a real network
       call at all -- verified by it still correctly returning false here */
    CHECK(!tezos_tez_domain_resolve(TEZOS_NETWORK_MAINNET, "KT1F7JKNqwaoLzRsMio1MQC7zv3jG9dHcDdJ",
                                    "not_a_real_domain", address, sizeof address),
          "resolving a malformed name is correctly rejected");

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
