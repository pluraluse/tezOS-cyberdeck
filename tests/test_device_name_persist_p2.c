/* Device name persistence, PART 2 of 2 -- must run immediately after
   test_device_name_persist_p1 (which creates the file this reads).
   A completely separate process from p1, sharing only the disk file --
   this is what actually proves "survives a restart", not just that a
   save function returned true. */
#include "../src/apps/settings/settings.h"
#include <stdio.h>
#include <string.h>
static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)
int main(void) {
    /* fresh process -- only the file on disk is shared with part 1 */
    settings_entry_screen();
    printf("  recovered device name: %s\n", settings_get_device_name());
    CHECK(strcmp(settings_get_device_name(), "cyber") == 0,
          "device name recovered in a SEPARATE PROCESS matches what part 1 saved -- real persistence");
    remove("/tmp/tezos_cyberdeck_settings.txt");
    printf("\nPART2 %s (%d failures)\n", fails==0?"PASSED":"FAILED", fails);
    return fails == 0 ? 0 : 1;
}
