# Camera App — Build Notes

Hardware: Pi camera module (CSI, ~1080p native color sensor), driving a 320x480
color SPI/GPIO display (ILI9486 + XPT2046 touch, DRM tinydrm path).

The Camera app has **two structurally separate modes**. This separation is not
just a UI toggle — it should be two genuinely different code paths from the
sensor onward, so a Capture-mode setting or bug can never degrade Scan mode.

---

## 1. Scan Mode (Beacon / QR pairing)

- Always full native sensor resolution and quality.
- Feeds directly into `zbar` for QR decode.
- Never touches the downsample/dither pipeline below, under any circumstance.
- Used for: Beacon (TZIP-10) dApp pairing, and any other QR-based flows added later.

## 2. Capture Mode (art / Game Boy Camera-style)

Two **independent** axes: Resolution and Color Depth. 4 x 4 = 16 combinations,
all sharing one pipeline.

### Resolution presets (user picks one; no separate "logical" vs "saved" image —
what's shown IS what's saved)

| Preset    | Screen fit behavior                          |
|-----------|-----------------------------------------------|
| 320x480   | Exact 1:1 fill. No scroll, no letterbox.       |
| 320x240   | Fits width exactly. Vertical letterbox (shorter than screen). |
| 800x480   | Fits height exactly. Horizontal scroll only.   |
| 640x480   | Modest overflow both axes. Scroll both directions. |

Note: every preset needs scrolling on **at most one axis** except 640x480 —
worth keeping scroll logic single-axis where possible.

### Color depth modes

| Mode              | Method                                                        |
|-------------------|----------------------------------------------------------------|
| 1-bit dithered    | Ordered Bayer dither (4x4 or 8x8 matrix) — Game Boy Camera look |
| 8-bit grayscale   | Weighted luminance conversion, smooth gradient, **no dither**   |
| 8-bit color       | 3-3-2 RGB bit truncation (256 colors) + Bayer dither for intermediate tones |
| 16-bit color      | Native RGB565 passthrough — matches display's native depth, no quantization needed |

### Pipeline (single path, no intermediate steps)

1. Capture native sensor frame.
2. **Box-average downsample** directly to the chosen target resolution.
   - NOT bilinear/bicubic (produces blur, defeats the pixel-art look).
   - NOT nearest-neighbor/point-sampling (produces noisy aliasing).
   - Box-averaging: average each block of source pixels into one output pixel.
3. Apply the chosen color-depth reduction (dither/quantize/passthrough as above).
4. Result bitmap = what's displayed = what's saved = what feeds minting.
   No separate "visible window" vs "backing image" — avoids ambiguity about
   what actually gets saved after scrolling.

### Expected quality variance (this is fine, not a bug)

Smaller presets (e.g. 320x240) average over larger source pixel blocks →
smoother, cleaner result. Larger presets (800x480, 640x480) average over
smaller blocks → closer to raw sensor noise/detail. This is just downsampling
math being honest about how much source information went into each output
pixel. The four presets should feel meaningfully different, not just "same
photo, different size."

### Minting integration

Output feeds directly into the same minting pipeline as the 1-bit art
(drawing) app — same file format, same "mint this" path into HEN v2-style
collection minting. Camera and drawing app are two input methods into one
minting flow, not two separate systems.

---

## Open dependency

This app's UI (mode-select while framing a shot, scroll controls, save/mint
handoff) depends on the tezOS visual/interaction design system, which is not
yet defined — see below.
