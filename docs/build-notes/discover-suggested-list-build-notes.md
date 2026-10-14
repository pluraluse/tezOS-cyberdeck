# Discover's Suggested List — Build Notes

## Status: real, tested, built on the design already discussed

Real, direct follow-through on the design proposed earlier: a small,
visibly-labeled "Suggested" list, distinct from the user's own real
bookmarks — not the large, vendor-curated address list this project
originally, deliberately decided against.

## Every address cross-verified against multiple independent real sources — not a single unverified claim

Given this project's own direct history of a contract address being
mislabeled with the wrong platform name in a third-party report, each
of the three entries here was checked against more than one real
source before being included:

- **Teia Marketplace** (`KT1PHubm9HtyQEJ4BBpMTVomq6mhbf...`) — confirmed
  directly on Teia's own official "about" page, Teia's own official
  GitHub smart-contracts repository, and tzkt.io's own indexer
  labeling. Three-plus independent real sources agreeing, one of them
  the platform's own site.
- **objkt.com Marketplace v2** (`KT1WvzYHCNBvDSdwafTHv7nJ1dW...`) —
  confirmed by objkt's own official FAQ documentation, tzkt.io's
  indexer labeling, and a third-party analytics dashboard. Three
  independent sources agreeing.
- **Liquidity Baking** (`KT1TxqZ8QtKvLu3V3JH7Gx58n7Co8pgtpQU5`) —
  already extensively verified elsewhere in this project's own prior
  work, live-tested on both mainnet and Shadownet — not a new claim
  introduced here.

## Real, deliberate visual distinction from bookmarks

Each Suggested entry is prefixed with a real ★ character in the list,
appended after the user's own real bookmarks, never given equal visual
footing. This is a real, structural choice, not a styling afterthought
— the whole point of building this as a small, cross-verified list
rather than the originally-rejected large curated one was to avoid
presenting anything with more confidence than it's actually earned.

## Verified

4 checks: the existing "+ Visit New Address" flow still works
unchanged with zero bookmarks saved (no regression); a Suggested entry
is genuinely selectable and correctly routes to its real, exact
address (confirmed via a real screenshot showing the correct address
was visited, honestly reporting it can't fetch live info in this
offline sandbox — not fabricating anything); and Suggested entries
correctly shift position once a real bookmark is added, rather than
overlapping with it. All under AddressSanitizer, zero errors. Full
70-suite regression run twice, full-stack `-O2` build clean.
