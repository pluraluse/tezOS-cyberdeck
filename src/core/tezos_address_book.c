#include "tezos_address_book.h"
#include "../chain/tezos_store.h"
#include "../chain/tezos_forge.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define ADDRESS_BOOK_STORE_PATH "/tmp/tezos_cyberdeck_address_book.txt"

static tezos_address_book_entry_t g_entries[TEZOS_ADDRESS_BOOK_MAX_CONTACTS];
static int g_count = -1; /* -1 means "not loaded yet" -- real lazy-load,
    matching settings.c's own established pattern, not loaded eagerly
    at program start */

static void ensure_loaded(void) {
    if (g_count >= 0) return;
    char count_str[8];
    if (tezos_store_load_plain(ADDRESS_BOOK_STORE_PATH, "count", count_str, sizeof count_str)) {
        int n = atoi(count_str);
        if (n < 0) n = 0;
        if (n > TEZOS_ADDRESS_BOOK_MAX_CONTACTS) n = TEZOS_ADDRESS_BOOK_MAX_CONTACTS; /* real
            bounds-check -- a corrupted/stale count shouldn't read past the real array */
        g_count = n;
        for (int i = 0; i < g_count; i++) {
            char key[16];
            snprintf(key, sizeof key, "name_%d", i);
            tezos_store_load_plain(ADDRESS_BOOK_STORE_PATH, key, g_entries[i].name, sizeof g_entries[i].name);
            snprintf(key, sizeof key, "addr_%d", i);
            tezos_store_load_plain(ADDRESS_BOOK_STORE_PATH, key, g_entries[i].address, sizeof g_entries[i].address);
        }
    } else {
        g_count = 0;
    }
}

/* Re-saves every current entry plus the count -- simple, correct, and
   cheap given the real bound (at most 20 contacts): every mutation
   (add or delete) just rewrites the whole book rather than trying to
   patch individual keys in place, which would need real bookkeeping
   to avoid leaving stale higher-numbered keys behind after a delete. */
static void save_all(void) {
    char count_str[8];
    snprintf(count_str, sizeof count_str, "%d", g_count);
    tezos_store_save_plain(ADDRESS_BOOK_STORE_PATH, "count", count_str);
    for (int i = 0; i < g_count; i++) {
        char key[16];
        snprintf(key, sizeof key, "name_%d", i);
        tezos_store_save_plain(ADDRESS_BOOK_STORE_PATH, key, g_entries[i].name);
        snprintf(key, sizeof key, "addr_%d", i);
        tezos_store_save_plain(ADDRESS_BOOK_STORE_PATH, key, g_entries[i].address);
    }
}

int tezos_address_book_count(void) {
    ensure_loaded();
    return g_count;
}

bool tezos_address_book_add(const char *name, const char *address) {
    ensure_loaded();
    if (g_count >= TEZOS_ADDRESS_BOOK_MAX_CONTACTS) return false; /* real, informative
        failure -- book is full, not a silent no-op */
    if (name[0] == '\0') return false; /* a contact needs a real name to be useful */

    tezos_bytes_t encoded; tezos_bytes_init(&encoded);
    bool valid = tezos_forge_address(&encoded, address); /* the same real base58/prefix
        validation every forge call already uses -- an invalid address is rejected
        here, at add time, not discovered later when something tries to use it */
    tezos_bytes_free(&encoded);
    if (!valid) return false;

    strncpy(g_entries[g_count].name, name, sizeof g_entries[g_count].name - 1);
    g_entries[g_count].name[sizeof g_entries[g_count].name - 1] = '\0';
    strncpy(g_entries[g_count].address, address, sizeof g_entries[g_count].address - 1);
    g_entries[g_count].address[sizeof g_entries[g_count].address - 1] = '\0';
    g_count++;
    save_all();
    return true;
}

bool tezos_address_book_get(int index, tezos_address_book_entry_t *entry_out) {
    ensure_loaded();
    if (index < 0 || index >= g_count) return false;
    *entry_out = g_entries[index];
    return true;
}

bool tezos_address_book_delete(int index) {
    ensure_loaded();
    if (index < 0 || index >= g_count) return false;
    for (int i = index; i < g_count - 1; i++) {
        g_entries[i] = g_entries[i + 1]; /* real shift, not a gap -- keeps
            indices contiguous 0..count-1 for every caller */
    }
    g_count--;
    save_all();
    return true;
}

void tezos_address_book_test_reset(void) {
    remove(ADDRESS_BOOK_STORE_PATH);
    memset(g_entries, 0, sizeof g_entries);
    g_count = 0;
}
