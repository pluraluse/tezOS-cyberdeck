# Gallery App — Build Notes

## Status: real owned-token list + detail view; no thumbnail/3D preview

Built on `tezos_tzkt_get_owned_tokens()` (`tezos_tzkt.h`/`.c`) — the
same real, live-verified `/v1/tokens/balances` endpoint already used for
Messenger's Inbox, generalized here to every contract an address holds
anything on, rather than filtered to one specific contract. Field names
(`contract.alias`/`contract.address`/`tokenId`/`metadata.name`/
`balance`) are confirmed against the same real, live-captured response
from earlier this session (a real address's actual token holdings
across dozens of real contracts) — not guessed from documentation.

Correctly excludes zero-balance historical entries, same reasoning as
Messenger's inbox function: the live capture showed real examples of
tokens once held and since transferred away, which a naive
"list every row ever seen" approach would wrongly include.

**Verified**: the parsing logic against a real fixture drawn directly
from that live capture (one zero-balance PLENTY entry correctly
excluded, one real hic et nunc OBJKT — including its real name,
"Genesis 002" — correctly extracted). The real UI flow (list → detail →
back) tested under AddressSanitizer, using a test-only injection hook
(`gallery_test_inject_tokens()`, not for production use) to verify
navigation with actual token content present, since this sandbox has no
way to reach live network data. Confirmed visually that selecting the
second of two injected tokens shows the second token's detail, not an
off-by-one artifact of the first.

## Deliberate scope cut

This app's original `STATUS.md` explicitly flagged "GLB 3D viewing" as
"an explicit afterthought, not a core feature" — not attempted here,
and for a concrete reason beyond just scope: this device's display
pipeline has no 3D rendering capability at all (a fixed 320×480 RGB565
framebuffer with 2D drawing primitives), so this would be new,
substantial rendering infrastructure, not a small addition.

Also not attempted: fetching/decoding thumbnail images. Real token
metadata often includes a `thumbnailUri` (usually pointing at IPFS),
but decoding an arbitrary remote image format into this device's
framebuffer is real, separate work (format detection, decode,
dither-to-panel-depth) — this app's list and detail views show real
text data (name, contract, token ID, balance), not a placeholder
waiting for images; images are a distinct, unbuilt feature, not a
degraded version of an intended image view.

## A real, structural bug in the install script caught and fixed along the way

While confirming Gallery would actually build correctly for a real
install, `scripts/install-tezos-service.sh` was found to be missing
**both** `../apps/messenger/*.c` and `../apps/gallery/*.c` — the exact
"hand-listed app directory drifts stale" problem this project's own
history already documents happening twice before with a different file
list. Fixed properly this time, not just patched: replaced the
hand-listed app directories with `find ../apps -name '*.c'`, which
(unlike a raw glob) doesn't error or leave a literal unmatched pattern
on the command line for app directories that are still stub-only (no
`.c` files yet) — structurally immune to this exact drift for every
future app, the same way `CORE_SOURCES` already was for `src/core/`.
Verified this actually finds and correctly compiles all 8 real app
source files (`wallet.c`, `settings.c`, `import_recovery.c`,
`gallery.c`, `scanner.c`, `camera.c`, `camera_pipeline.c`,
`messenger.c`) before considering this fixed.

## Update: this is now built — see gallery-viewer-build-notes.md

The scoping below was written before this was built, and is kept for
historical record — the real plan largely held up (IPFS resolution,
`stb_image.h`, reusing Camera's downscale/dither, a new pannable
viewport), though the specific open questions below were resolved
differently in a few places (no caching was built yet; `displayUri` was
confirmed as the right first field to check, with `thumbnailUri`/
`artifactUri` as real fallbacks). See
`docs/build-notes/gallery-viewer-build-notes.md` for the full, real
writeup of what actually got built.

## Scoped for a future cycle: real thumbnail + 1:1 pixel viewer (historical)

Not started — this is a scope, not an implementation, written up so the
next cycle has a real plan rather than starting from nothing. Checked
against real prior art before writing anything down, same discipline as
everything else in this project.

**What's needed, in dependency order:**

1. **Resolve the image reference.** Real token metadata (already
   fetched in part by `tezos_tzkt_get_owned_tokens()`, though not yet
   parsed for this) typically has `displayUri`/`artifactUri`/
   `thumbnailUri` fields, usually `ipfs://{cid}` URIs. These need
   translating to an HTTP(S) URL via a gateway
   (`https://ipfs.io/ipfs/{cid}` or a dedicated one) before they're
   fetchable — `ipfs://` itself isn't a scheme `tezos_http.c` (or any
   plain HTTP client) can fetch directly.
2. **Fetch the raw bytes.** Reuses `tezos_http.c` directly — no new
   networking code needed, just a GET against the resolved gateway URL.
3. **Decode.** `stb_image.h` (nothings/stb, public domain / MIT dual
   license, real and current) is a strong fit: single-header, decodes
   PNG/JPEG/BMP/GIF/TGA, and — importantly for this project's style —
   `stbi_load_from_memory()` decodes directly from a byte buffer, no
   filesystem dependency, no external library linking beyond libc. This
   would be the project's first genuinely new third-party dependency
   beyond what's already vendored (libqrencode/zbar/cjson/curl/
   libsodium) — worth flagging as a real, deliberate addition, not
   slipped in quietly.
4. **Downscale for the list/thumbnail view.** Real precedent already
   exists in this codebase: Camera's box-average downsampling
   (`camera_pipeline.c`, already proven different from naive
   nearest-neighbor via a real checkerboard test) is the right
   *technique*, though it was built for a fixed sensor resolution, not
   arbitrary decoded image dimensions — needs generalizing, not
   reinventing.
5. **RGB565 + dithering for the panel.** Same real precedent: Camera's
   4 color-depth modes (1-bit dither, 8-bit gray, 8-bit color 3-3-2,
   16-bit RGB565 passthrough) are the right starting point for how
   arbitrary decoded pixels should reach this panel's actual bit depth.
6. **The 1:1 pixel view is the genuinely new part.** This is a fixed
   320×480 panel; most real digital art (OBJKTs especially) is far
   larger, so "1:1" means panning a viewport across the full decoded
   image, not just displaying it — real scroll/pan input handling on
   the D-pad, which doesn't exist anywhere in this project's UI toolkit
   yet (every existing screen is either static or list-scrollable, not
   pannable across a 2D surface).

**Real open questions to resolve before starting, not during:**

- Which metadata field to actually use (`displayUri` vs `artifactUri`
  vs `thumbnailUri`) — they can differ significantly in resolution and
  file size; real tokens vary in which fields are even populated.
- Which IPFS gateway, and what happens when it's slow/down — real
  gateways have real rate limits and reliability issues; this needs a
  fallback strategy, not an assumption that the first choice always
  works.
- Memory/performance budget on a Pi Zero 2 W specifically — decoding
  and holding a large source image (some real OBJKTs are multiple
  megapixels) alongside this project's existing footprint needs a real
  bound, not "however much it takes."
- Caching — a naive implementation would re-fetch and re-decode the
  same image every time its detail view is opened; a real device
  doing this over real network conditions needs at least a
  session-lifetime cache, ideally something that survives a restart.

**Sequencing**: this depends on nothing else currently planned (Gallery
itself is done), but it's a genuinely larger, standalone effort — a new
vendored dependency, a generalized version of an existing technique,
and one entirely new UI interaction pattern (panning) — not a small
follow-up to this session's work.
