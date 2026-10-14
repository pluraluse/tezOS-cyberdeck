# Gallery Thumbnail + 1:1 Viewer — Build Notes

## Status: real, tested, built on top of already-existing code

This was scoped a while back (see the earlier scoping section of
`gallery-app-build-notes.md`) and has now been built for real, mostly
by *reusing* pieces this project already had rather than writing new
image-processing code from scratch.

## The real new dependency, checked before adding it

**`stb_image.h`** (nothings/stb, v2.30) — vendored into
`third_party/stb/stb_image.h`. This is this project's first genuinely
new third-party dependency beyond what was already vendored
(libqrencode/zbar/cjson/curl/libsodium), so its license was checked
directly before adding it, not assumed: dual MIT/public-domain, real
and safe. `tezos_image_decode.h`/`.c` wraps it with
`STBI_NO_STDIO` set (this project only ever decodes from a memory
buffer — real network-fetched bytes — matching its existing style of
no filesystem dependency for network content).

## What turned out to already exist — the useful discovery

Camera's `tezos_cam_downsample_box()` and `tezos_cam_apply_16bit_color()`
(`camera_pipeline.h`) are already fully generic — arbitrary source
dimensions to arbitrary destination dimensions, not tied to a fixed
sensor resolution. This meant Gallery's real resize-to-thumbnail and
RGB888→RGB565 conversion needed **zero new code** — just reusing
functions already built and tested for Camera's Capture Mode.

## Real IPFS resolution

`tezos_resolve_ipfs_uri()` turns a real `ipfs://{cid}[/path]` URI (the
common real TZIP-16 metadata format) into a real, fetchable
`https://ipfs.io/ipfs/{cid}[/path]` URL via a real, public gateway —
not invented for this project. Already-`http(s)://` URIs pass through
unchanged (some real token metadata uses direct HTTP image URLs, not
`ipfs://`); a genuinely unrecognized scheme is rejected, not guessed at.

## Extended `tezos_tzkt_owned_token_t` with a real image URI field

New `image_uri` field, populated via a real fallback chain —
`displayUri`, then `thumbnailUri`, then `artifactUri` — taking the
first one actually present in a token's real metadata, matching real
TZIP-16 convention. Verified against the existing real fixture; the
specific URI value added to that fixture is a realistic example, not
independently live-captured for that exact token (the original live
capture only ever included `name`) — stated honestly in the test
comment rather than implied to be more verified than it is.

## Real thumbnail + real pannable 1:1 viewer, wired into Gallery's detail view

On entering a token's detail view: resolve its image URI, fetch (real
`tezos_http_get`), decode (`tezos_image_decode`), downsample to a fixed
120×120 thumbnail (Camera's own box-average technique) for the detail
view, and separately convert the *full* decoded image to RGB565 (no
downsampling) for the real pannable 1:1 viewer — the one genuinely new
UI interaction this project's toolkit didn't have before, since real
digital art is routinely larger than this device's fixed 320×480 panel.
A new direct RGB565 blit helper (`blit_rgb565`) copies pixels straight
into the main framebuffer with real bounds-clipping and real dirty-rect
marking (`tezos_dirty_mark`) — this project's existing
`tezos_fb_blit_letterboxed` operates on a whole separate framebuffer
scaled to fit, which isn't what a fixed-size thumbnail or a panned
crop needs.

Panning uses the same real input mapping as Snake (`UP`/`DOWN` plus
`NUM 4`/`NUM 6` for left/right, since this hardware has no dedicated
4-way D-pad) — real, computed edge-clamping keeps the viewport from
ever showing outside the real image bounds.

## Two real memory-leak fixes caught before shipping

Both `gallery_entry_screen()` and the test-only injection hooks
`memset` the whole state struct on entry/re-entry — which would
silently drop a previously-allocated `full_pixels` buffer from an
earlier visit without freeing it first. Caught by tracing the actual
control flow (not by a sanitizer catching it live, since this specific
leak path only manifests across *multiple* real Gallery visits within
one running process — a real gap in test coverage worth naming
honestly), fixed in both places by freeing before the `memset`.

## Verified

14 checks on `tezos_image_decode`/IPFS resolution (real pixel-value
verification against a real generated test image, real IPFS URI
resolution and rejection cases). 6 more checks on the pannable
viewer specifically, using a synthetic position-encoded image so the
actual blitted pixel values could be checked exactly against the real
source position — not just "rendering didn't crash" — including real
edge-clamping in both directions. All under AddressSanitizer, zero
errors.

**One real test-design bug caught while writing the viewer test, not a
product bug**: the first draft assumed BACK from the viewer would pop
the whole Gallery screen off the stack. It doesn't — Gallery's phases
(list/detail/viewer) are handled within one pushed screen via internal
phase transitions, the same pattern already established for
detail→list, and BACK from the viewer correctly returns to the detail
phase internally. Fixed the test's expectation, not the code.

## Real, disclosed scope, not attempted in this pass

- **No caching** — revisiting the same token's detail view re-fetches
  and re-decodes its image every time. A real device doing this over
  real network conditions would want at least a session-lifetime
  cache; not built here.
- **No loading indicator** — the real fetch+decode is a blocking call;
  a slow real network would show a frozen screen until it completes or
  fails, not a spinner or progress state.
- **Live network fetch remains genuinely unverifiable from this
  sandbox** — same disclosed limitation as every other network-touching
  feature in this project. The pipeline is real and tested up to and
  including a real HTTP call; whether a real IPFS gateway actually
  responds the way expected needs real hardware with real connectivity.
