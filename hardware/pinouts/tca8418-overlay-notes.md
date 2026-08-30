# TCA8418 keypad — device-tree overlay notes

Mainline Linux has a real driver for this chip
(`drivers/input/keyboard/tca8418_keypad.c`, present since kernel 4.11),
configured via a standard device-tree binding. Once loaded correctly, the
keypad shows up as an ordinary evdev keyboard device — `src/shim-linux/`
just reads standard `EV_KEY` events, no custom I2C code needed.

## Binding reference

- `compatible = "ti,tca8418";`
- `reg` — I2C address (default 0x34, per TCA8418 datasheet — confirm no
  clash with the PiSugar battery HAT on the same I2C bus first, per
  `docs/build-notes/hardware-roadmap-build-notes.md`)
- `interrupts` — the TCA8418's IRQ line, trigger on falling edge
- `keypad,num-rows` / `keypad,num-columns` — matches your physical matrix
- `linux,keymap` — this is the important part: maps each (row, column)
  position to a **standard Linux keycode**, encoded as
  `MATRIX_KEY(row, col, keycode)` per entry

## Keymap this project's shim assumes

`src/shim-linux/linux_shim.c`'s `translate_key()` expects these Linux
keycodes — map your physical keys to exactly these in the overlay's
`linux,keymap`, or update both together if you change one:

| Physical key | Linux keycode |
|---|---|
| Up | `KEY_UP` |
| Down | `KEY_DOWN` |
| Left softkey (Select) | `KEY_ENTER` |
| Right softkey (Back/Menu) | `KEY_ESC` |
| Numeric 0–9 | `KEY_0`–`KEY_9` |
| `*` | `KEY_KPASTERISK` |
| `#` | `KEY_BACKSLASH` (placeholder — pick whatever unused keycode fits your keymap convention; `#` has no dedicated standard keycode) |

## Practical steps on the Pi

1. Write a `.dts` overlay fragment with the binding above, matching your
   actual TCA8418 wiring (I2C address, IRQ GPIO, row/col count).
2. Compile it: `dtc -@ -I dts -O dtb -o tca8418.dtbo tca8418-overlay.dts`
3. Copy to `/boot/firmware/overlays/`, add `dtoverlay=tca8418` to
   `/boot/firmware/config.txt`, reboot.
4. Verify: `cat /proc/bus/input/devices` should show a TCA8418 entry with
   an `event` handler — that's the path to pass to `open_input_device()`
   in `linux_shim.c` (currently a placeholder path; find the real one
   with the command above and update it).

This file intentionally doesn't include a copy-pasteable full `.dts` —
the exact I2C address, IRQ GPIO, and row/column count depend on your
actual wiring, which isn't finalized in `hardware/pinouts/` yet.
