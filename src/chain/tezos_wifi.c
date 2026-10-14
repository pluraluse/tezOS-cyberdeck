#define _POSIX_C_SOURCE 200809L
#include "tezos_wifi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* nmcli terse mode escapes literal colons within a field value with a
   backslash (`\:`) so they aren't mistaken for the field separator --
   a real, documented nmcli convention, not an edge case to ignore.
   This splits on unescaped colons only, unescaping `\:` -> `:` and
   `\\` -> `\` in each resulting field. */
static void split_terse_line(const char *line, char fields_out[][64], int max_fields, int *count_out) {
    int field = 0;
    size_t pos = 0;
    const char *p = line;

    fields_out[0][0] = '\0';
    while (*p && field < max_fields) {
        if (*p == '\\' && (*(p+1) == ':' || *(p+1) == '\\')) {
            if (pos < 63) fields_out[field][pos++] = *(p+1);
            p += 2;
            continue;
        }
        if (*p == ':') {
            fields_out[field][pos] = '\0';
            field++;
            pos = 0;
            if (field < max_fields) fields_out[field][0] = '\0';
            p++;
            continue;
        }
        if (pos < 63) fields_out[field][pos++] = *p;
        p++;
    }
    fields_out[field][pos] = '\0';
    *count_out = field + 1;
}

bool tezos_wifi_scan(tezos_wifi_network_t *results_out, int max_results, int *count_out) {
    *count_out = 0;
    FILE *fp = popen("nmcli -t -f SSID,SIGNAL,SECURITY device wifi list 2>/dev/null", "r");
    if (!fp) return false;

    char line[256];
    while (fgets(line, sizeof line, fp) && *count_out < max_results) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        if (line[0] == '\0') continue;

        char fields[3][64];
        int field_count = 0;
        split_terse_line(line, fields, 3, &field_count);
        if (field_count < 3) continue; /* malformed/truncated line -- skip, don't crash */

        tezos_wifi_network_t *net = &results_out[*count_out];
        strncpy(net->ssid, fields[0], sizeof net->ssid - 1);
        net->ssid[sizeof net->ssid - 1] = '\0';
        net->signal_percent = atoi(fields[1]);
        strncpy(net->security, fields[2], sizeof net->security - 1);
        net->security[sizeof net->security - 1] = '\0';
        (*count_out)++;
    }
    pclose(fp);
    return true;
}

bool tezos_wifi_connect(const char *ssid, const char *password) {
    /* fork()+execvp(), NOT popen()/system() -- see tezos_wifi.h's
       security note. ssid/password come from user T9 input; passing
       them through a shell would be a real injection risk. */
    pid_t pid = fork();
    if (pid < 0) return false;

    if (pid == 0) {
        /* child */
        char *argv[] = {
            (char *)"nmcli", (char *)"device", (char *)"wifi", (char *)"connect",
            (char *)ssid, (char *)"password", (char *)password, NULL
        };
        execvp("nmcli", argv);
        _exit(127); /* execvp only returns on failure */
    }

    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool tezos_wifi_current_connection(char *ssid_out, size_t ssid_cap) {
    ssid_out[0] = '\0';
    FILE *fp = popen("nmcli -t -f active,ssid device wifi list 2>/dev/null", "r");
    if (!fp) return false;

    char line[256];
    bool found = false;
    while (fgets(line, sizeof line, fp)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';

        char fields[2][64];
        int field_count = 0;
        split_terse_line(line, fields, 2, &field_count);
        if (field_count < 2) continue;

        if (strcmp(fields[0], "yes") == 0) {
            strncpy(ssid_out, fields[1], ssid_cap - 1);
            ssid_out[ssid_cap - 1] = '\0';
            found = true;
            break;
        }
    }
    pclose(fp);
    (void)found; /* not connected is a valid, successful outcome -- empty ssid_out, true return */
    return true;
}
