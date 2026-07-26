# Funding & Legitimacy Strategy

Position this project as legitimate Tezos infrastructure, not a novelty
device, from the first outward-facing document onward. That framing has
to be earned in the right order — funding and platform trust follow
proof, not the reverse.

## The core credibility asset: audit access

There is an existing informal relationship with a respected Tezos baker
who would be willing to assist in auditing the device. This is a real
asset — bakers are validators with technical standing in the community,
and an audit endorsement from one carries weight a founder's own claims
of security can't.

**Timing matters here**: don't formalize or announce this relationship
until there's an actual signer implementation to audit (post-M8 in
ROADMAP.md). An audit commitment against vaporware helps no one and can
read as borrowed credibility rather than earned. Once the signer protocol
and firmware exist and are open-sourced, that's the point to bring this
relationship into the open — ideally as a real audit with a public
writeup, not just a mention.

## Funding avenues

### Tezos Foundation Ecosystem Grants
Fits this project directly under at least two of the Foundation's stated
categories: **Developer Experience** (tools/tutorials/infrastructure to
make Tezos easier to build on) and **End-User Applications** (new kinds
of applications addressing problems traditional software struggles with).

Two things this grant does beyond money:
- The Foundation's **Business Development vertical** exists specifically
  to forge strategic partnerships and engage the community — a credible
  grant application is a warm door to platform introductions (Teia,
  objkt, DeFi protocols), not just a check.
- A Foundation grant itself is a legitimacy signal other platforms and
  contributors will recognize, independent of the funding amount.

Apply once M9-M10 (core app suite + Messenger) are working — a grant
application needs a working reference to point to, not just a roadmap.

### Tezos Commons small grants
Regional program, lower friction, smaller amounts (historically up to
~$10,000) — useful for funding a specific discrete milestone (e.g. the
audit itself, or the first custom PCB run) without the larger Foundation
process. Can run in parallel with, or ahead of, the main Foundation
application.

## Sequencing — legitimacy has to be earned in order

1. Working device + reference app integrations (Discover, Messenger) —
   this must exist before any outward funding/partnership push starts.
2. Signer fully open-sourced, audit engaged (see above).
3. Tezos Commons small grant application (lower friction, can fund the
   audit or PCB run directly).
4. Tezos Foundation Ecosystem Grant application — Developer Experience
   and/or End-User Applications category.
5. Direct community engagement in parallel with all of the above: Tezos
   Commons community calls (Tuesday Tezday, Artz Fridays), TezDev,
   developer Discord/Slack. Reputation in this ecosystem travels through
   the people already running the platforms, not through a formal BD
   pitch deck.

## Platform onboarding — what "legitimacy" unlocks

Once funded/audited/credible, the actual ask to a platform is: build a
verified partner app in the curated repo (see BOM.md's app-repo model),
using the published app manifest spec and Scanner's forge/sign pipeline.
Lead with smaller, values-aligned platforms first (Teia's DAO structure
is a natural early adopter given the existing Messenger integration)
before approaching larger names — credibility compounds, it doesn't start
at the top.
