#pragma once
/*
 * Address book — a real, shared "save a name for an address, reuse it
 * later" module, so every real send/message/call flow in this project
 * (Wallet, Messenger, Explorer) can offer "pick from saved" ahead of
 * raw T9 address entry, not instead of it. Genuinely shared, core-
 * adjacent module (like tezos_popup.h/tezos_t9.h) rather than living
 * inside any one app, since multiple apps use it.
 *
 * Every real address is validated on add via tezos_forge_address() --
 * the same real base58/prefix validation every forge call already
 * uses, not a separate, weaker check invented for this module. An
 * invalid address is rejected at add time, not discovered later when
 * something tries to actually use it.
 */
#include <stdbool.h>
#include <stddef.h>

#define TEZOS_ADDRESS_BOOK_MAX_CONTACTS 20
#define TEZOS_ADDRESS_BOOK_NAME_MAX 32
#define TEZOS_ADDRESS_BOOK_ADDRESS_MAX 40

typedef struct {
    char name[TEZOS_ADDRESS_BOOK_NAME_MAX];
    char address[TEZOS_ADDRESS_BOOK_ADDRESS_MAX];
} tezos_address_book_entry_t;

/* How many contacts are currently saved. */
int tezos_address_book_count(void);

/* Real validation (via tezos_forge_address) before saving -- returns
   false, and saves nothing, if the address doesn't actually encode, if
   name is empty, or if the book is already at
   TEZOS_ADDRESS_BOOK_MAX_CONTACTS. Real, informative failure, not a
   silent no-op. */
bool tezos_address_book_add(const char *name, const char *address);

/* Fills *entry_out with the contact at `index` (0-based, in save
   order). Returns false if index is out of range. */
bool tezos_address_book_get(int index, tezos_address_book_entry_t *entry_out);

/* Removes the contact at `index`, shifting later entries down by one
   (so indices stay contiguous 0..count-1) -- real deletion, not just
   marking a slot empty and leaving a gap for callers to trip over.
   Returns false if index is out of range. */
bool tezos_address_book_delete(int index);

/* Exposed specifically for direct testing (see
   tests/test_address_book.c) -- resets the in-memory book AND the
   on-disk store to empty, so tests don't leak contacts between runs.
   NOT for production use. */
void tezos_address_book_test_reset(void);
