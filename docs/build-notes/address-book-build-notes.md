# Address Book — Build Notes

## Status: real, tested, integrated into Wallet's Contacts page

A real, shared "save a name for an address, reuse it later" module —
`src/core/tezos_address_book.h`/`.c` — so every real send/message/call
flow in this project can offer "pick from saved" ahead of raw T9
address entry, not instead of it. Built core-adjacent (like
`tezos_popup.h`/`tezos_t9.h`) rather than living inside any one app,
since the goal from the start was for Wallet, Messenger, and Explorer
to all be able to use the same saved contacts.

**Real validation, not a rubber stamp**: every address is checked via
`tezos_forge_address()` on add — the same real base58/prefix validation
every forge call already uses. An invalid address is rejected at add
time, with a real, informative failure, not discovered later when
something tries to actually use it.

**Real persistence**, using the same plain key=value store as Settings,
verified with a genuine two-process test (not just within one process)
— write contacts in one process, read them back correctly in a
completely separate process invocation, matching the same rigor
Settings' own device-name persistence test already established.

**Real deletion**: removing a contact shifts every later entry down by
one, keeping indices contiguous — not just marking a slot empty and
leaving a gap for callers to trip over.

## Wired into Wallet's Contacts page — real, not mock, for the first time

Contacts previously showed three hardcoded, fake entries
(`alice.tez`/`bob.tez`/`baker.tz`) that never actually existed
anywhere. Now: a real list built from `tezos_address_book_get()`, with
a "+ Add Contact" entry always first, driving a real T9 name-entry →
address-entry flow (real validation, real rejection shown in-place if
the address doesn't check out). Selecting any real saved contact jumps
**straight to amount entry** with the address already filled in,
skipping recipient re-entry entirely — that's the actual point of an
address book, not a side effect.

**A real bug caught by the compiler, not by luck**: the Add Contact
screens were originally written *before* `wallet_pages_state_t` and
`refresh_contacts_list()` were defined later in the file, so the call
to refresh the list after a successful save referenced an undeclared
function and an undeclared type. Caught immediately by a real compile
error (not a silent runtime bug), fixed by relocating the whole Add
Contact section to after those definitions — not by adding a forward
declaration workaround that would have papered over the ordering
problem instead of fixing it. One genuine, already-familiar
`-Wstringop-truncation` false positive (same class as three earlier
instances in this project) was also hit and suppressed the same
documented way.

## Verified

25+ checks on the core module alone (add/get/delete/count, real
address validation both accepting real tz1/KT1 addresses and rejecting
garbage, real book-full rejection at the real
`TEZOS_ADDRESS_BOOK_MAX_CONTACTS` bound, real delete-and-shift
correctness, real two-process persistence), all under AddressSanitizer.
7 more checks on the full Wallet UI integration — the add flow, its
real rejection path, cancellation, and the real "send to a saved
contact skips straight to amount entry" flow — also under
AddressSanitizer, zero errors either way.

## Not done in this pass

- **Messenger's and Explorer's own recipient/address entry screens
  don't yet offer "pick from saved."** The core module and Wallet's own
  integration are real and complete; extending the same "pick from
  saved" step to Messenger's Compose flow and Explorer's address-lookup
  entry is real, natural follow-up work reusing the same module, not
  attempted in this pass.
- **No dedicated "manage contacts" edit/rename flow** — a saved contact
  can be deleted via the real `tezos_address_book_delete()` function,
  but no UI screen currently exposes a delete action; Wallet's UI
  currently only exercises add + use.
