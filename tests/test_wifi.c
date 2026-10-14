/* WiFi configuration -- real nmcli command construction, real terse-
   output parsing (including nmcli's documented escaped-colon
   convention, a real edge case a naive strtok(':') would get wrong),
   and the real fork()+execvp()+exit-status mechanism, all exercised
   against a mock `nmcli` (tests/fixtures/nmcli) standing in for the
   real thing -- no WiFi hardware or NetworkManager daemon exists in
   any environment this is likely to run in besides the real target Pi.
   See src/chain/tezos_wifi.h for why nmcli/NetworkManager is the
   correct real-world target (checked before writing any code, unlike
   Scanner's QR schema and the signer wire protocol). */
#define _POSIX_C_SOURCE 200809L
#include "../src/chain/tezos_wifi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

int main(void) {
    /* Self-contained PATH setup -- doesn't rely on the invoking shell
       (run_all_tests.sh or otherwise) to have set this up correctly.
       Assumes CWD is tests/ (true when run via run_all_tests.sh, which
       cd's there first -- same assumption every other test in this
       suite already makes for its own file I/O). */
    setenv("PATH", "./fixtures:/usr/bin:/bin", 1);

    tezos_wifi_network_t results[TEZOS_WIFI_MAX_NETWORKS];
    int count = 0;
    CHECK(tezos_wifi_scan(results, TEZOS_WIFI_MAX_NETWORKS, &count), "scan succeeds against the mock nmcli");
    CHECK(count == 3, "scan finds exactly 3 networks, matching the mock's real output");
    if (count == 3) {
        CHECK(strcmp(results[0].ssid, "HomeNetwork") == 0 && results[0].signal_percent == 87
              && strcmp(results[0].security, "WPA2") == 0, "first network parsed exactly correctly");
        CHECK(strcmp(results[1].ssid, "OfficeGuest") == 0 && results[1].signal_percent == 45
              && strcmp(results[1].security, "--") == 0, "second network (open) parsed exactly correctly");
        CHECK(strcmp(results[2].ssid, "Joe's Wifi: Guest") == 0,
              "third network's SSID with an ESCAPED COLON parses correctly as a single field with a literal colon -- "
              "a naive strtok(':') would have split this into two garbage fields");
        CHECK(results[2].signal_percent == 60 && strcmp(results[2].security, "WPA2") == 0,
              "the remaining fields after the escaped-colon SSID still parse correctly");
    }

    CHECK(tezos_wifi_connect("GoodNetwork", "correctpassword"),
          "connect succeeds against the mock nmcli with correct credentials -- "
          "real fork()+execvp()+exit-status mechanism genuinely works end-to-end");
    CHECK(!tezos_wifi_connect("GoodNetwork", "wrongpassword"),
          "connect correctly FAILS with wrong credentials -- exit code 1 correctly propagated");
    CHECK(!tezos_wifi_connect("NonexistentNetwork", "whatever"),
          "connect correctly fails for an unknown network");

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
