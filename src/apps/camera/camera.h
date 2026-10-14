#pragma once
/*
 * Camera app — Scan Mode AND Capture Mode, both real now.
 * docs/build-notes/camera-app-build-notes.md has the full spec for
 * both. Structurally separate code paths from the sensor onward, per
 * that doc's requirement — a Capture-mode bug can never reach or
 * affect Scan mode.
 *
 * Entry point pushes a real Scan/Capture mode-select screen (previously
 * this jumped straight into Scan mode with no choice).
 *
 * HONEST LIMITATION, same for both modes: there is no real CSI camera
 * driver here — V4L2 capture and image buffer management from actual
 * sensor hardware is a separate, genuinely hardware-dependent task that
 * can't be built or tested without a real Pi + camera module.
 *
 * Scan Mode's job ends at "scan and decode a real pairing QR" — it
 * hands the decoded peer info to the separate Beacon Wallet app
 * (src/apps/beacon_wallet/) from there, rather than handling the
 * connection/session/signing itself. See beacon_wallet.h and
 * docs/CONCEPTS.md's platform-philosophy section for why that's a
 * separate app, not more of Camera's own internals.
 *
 * Capture Mode (camera_pipeline.h/.c) implements the real Game Boy
 * Camera-style pipeline — box-average downsampling (not nearest-
 * neighbor/bilinear — verified this actually matters with a checkerboard
 * test case) and 4 color-depth modes (1-bit ordered Bayer dither, 8-bit
 * grayscale, 8-bit 3-3-2 color+dither, 16-bit RGB565 passthrough) — using
 * a "simulate capture" trigger against a synthetic gradient+shape test
 * frame in place of a live sensor feed. Bayer dither matrices are
 * generated programmatically (recursive tiling construction, verified
 * against the well-known 4x4 reference), not hand-transcribed.
 *
 * Both stand-in triggers follow the same pattern already used for the
 * mock signer (stands in for the STM32) and the TZKT/HTTP client (its
 * live network calls can't be tested from this sandbox either) —
 * swapping either for a real V4L2 frame source is future work gated on
 * real camera hardware, not a rewrite of either screen's logic.
 */
#include "../../core/tezos_core.h"

tezos_screen_t *camera_entry_screen(void);
