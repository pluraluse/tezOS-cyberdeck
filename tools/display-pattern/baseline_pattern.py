#!/usr/bin/env python3
"""
baseline_pattern.py — reference test pattern for the ili9481 fbtft framebuffer.

Writes directly to /dev/fb1 (320x480, RGB565). Does the R/B channel swap
itself at pixel-write time instead of relying on the driver's `bgr`
devicetree override — see docs/build-notes/display-bringup-build-notes.md
for why: `bgr=0` was deployed and confirmed in config.txt and dmesg, but
colors still showed the swapped pattern on a later boot (same physical
viewing angle, confirmed) — not explained, most likely a marginal
breadboard connection, not chased further per the Owner's direction.
Swapping in software makes correctness a software guarantee regardless of
what the flag or a loose wire do.

Draws:
  - four quadrants (red/green/blue/white, pre-swap colors as named — swap
    happens inside px()) for channel-order sanity checking
  - a 40x40 black square at buffer origin (0,0) as a REFERENCE marker, not
    a claimed "top-left" or "top-right" — which physical corner that lands
    on is a mounting/enclosure decision, not settled by this script
  - a 4px white border around the full frame, to catch partial-coverage
    bugs (seen once already, at rotate=90/270)
"""
import struct

W, H = 320, 480


def rb_swap(c):
    r = (c >> 11) & 0x1F
    g = (c >> 5) & 0x3F
    b = c & 0x1F
    return (b << 11) | (g << 5) | r


RED, GREEN, BLUE, WHITE, BLACK = 0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000


def main():
    buf = bytearray(W * H * 2)

    def px(x, y, c):
        if 0 <= x < W and 0 <= y < H:
            off = (y * W + x) * 2
            buf[off:off + 2] = struct.pack('>H', rb_swap(c))

    for y in range(H):
        for x in range(W):
            if y < H // 2 and x < W // 2:
                c = RED
            elif y < H // 2 and x >= W // 2:
                c = GREEN
            elif y >= H // 2 and x < W // 2:
                c = BLUE
            else:
                c = WHITE
            px(x, y, c)

    for y in range(40):
        for x in range(40):
            px(x, y, BLACK)

    for x in range(W):
        for t in range(4):
            px(x, t, WHITE)
            px(x, H - 1 - t, WHITE)
    for y in range(H):
        for t in range(4):
            px(t, y, WHITE)
            px(W - 1 - t, y, WHITE)

    with open('/dev/fb1', 'wb') as f:
        f.write(bytes(buf))
    print('BASELINE_WRITTEN', W, H)


if __name__ == '__main__':
    main()
