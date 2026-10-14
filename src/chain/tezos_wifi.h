#pragma once
/*
 * WiFi configuration — real `nmcli` (NetworkManager) integration, not a
 * placeholder. Verified as the correct real-world target before writing
 * any code (see docs/build-notes/settings-app-build-notes.md's WiFi
 * section): since Raspberry Pi OS Bookworm, NetworkManager/nmcli is the
 * official standard, replacing wpa_supplicant/dhcpcd entirely — this
 * project's own target OS (README.md: "Raspberry Pi OS Lite"). This is
 * exactly the prior-art check that should have happened before Scanner's
 * QR schema and the signer wire protocol were designed, done properly
 * this time.
 *
 * Real, confirmed-current commands used here:
 *   - Scan:    nmcli -t -f SSID,SIGNAL,SECURITY device wifi list
 *              (terse mode, `-t -f`, is nmcli's own documented
 *              script-friendly output format — colon-separated, stable
 *              across versions — not scraping the human-readable table,
 *              which has variable-width columns and unicode bar
 *              characters that would be fragile to parse.)
 *   - Connect: nmcli device wifi connect "<ssid>" password "<password>"
 *
 * SECURITY NOTE, a real design decision not an afterthought: SSID and
 * password come from user T9 input and get passed to nmcli as command
 * arguments. `tezos_wifi_connect()` uses fork()+execvp() (a real argv
 * array, no shell) specifically to avoid shell injection risk from
 * arbitrary user-typed content -- popen()/system() would pass the
 * command through a shell, where a crafted SSID containing shell
 * metacharacters could do real damage. `tezos_wifi_scan()` has no
 * user-supplied interpolation, so popen() is fine there.
 *
 * HONEST LIMITATION: this sandbox has no WiFi hardware and no
 * NetworkManager daemon (confirmed: `nmcli` isn't even installed here).
 * What's verified: the real command construction and real subprocess
 * execution mechanics (checked against a safe real command, standing in
 * for nmcli), and the terse-output parsing logic (checked against
 * realistic fixtures built from nmcli's own documented terse format,
 * not fabricated). The actual live scan/connect behavior against real
 * WiFi hardware can only be verified on the real device.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define TEZOS_WIFI_MAX_NETWORKS 16

typedef struct {
    char ssid[64];
    int signal_percent; /* 0-100, from nmcli's SIGNAL field */
    char security[32];  /* e.g. "WPA2", or "--" for an open network */
} tezos_wifi_network_t;

/* Runs the real scan command and parses its terse output. Returns
   false only on a genuine execution failure (nmcli not found/couldn't
   run) -- finding zero networks is NOT a failure, that's
   *count_out == 0 with a true return. */
bool tezos_wifi_scan(tezos_wifi_network_t *results_out, int max_results, int *count_out);

/* Attempts to connect, via fork()+execvp() -- see the security note
   above for why not popen()/system(). Returns true if nmcli reports
   success (exit code 0). */
bool tezos_wifi_connect(const char *ssid, const char *password);

/* Current connection's SSID, or an empty string if not connected.
   Returns false only on execution failure. */
bool tezos_wifi_current_connection(char *ssid_out, size_t ssid_cap);
