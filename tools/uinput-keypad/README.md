# Synthetic keypad (uinput)

Exercises real keypad navigation in `linux_shim.c` without the TCA8418
hardware wired yet (M2). Used to verify Wallet and Settings actually
render correctly on real hardware — see
`docs/build-notes/linux-shim-fbdev-rewrite-build-notes.md`.

## Build (on the Pi)

```bash
gcc -O2 -Wall -Wextra -std=c11 -o uinput_keypad uinput_keypad.c
```

## Run

Needs `/dev/uinput` access (root, or add the user to whatever group owns
it on your image — it was `root:root 0600` on the confirmed bring-up
image, so `sudo` was used).

```bash
sudo ./uinput_keypad
```

It creates a virtual keyboard named `tca8418-keypad-sim`, prints the
resulting event number from `/proc/bus/input/devices`, then reads key
codes (one Linux `KEY_*` numeric code per line) from stdin and sends a
press+release for each.

## Wiring it to `linux_shim.c`

`main()`'s keypad path is a hardcoded placeholder:
`/dev/input/by-id/tca8418-keypad-event-kbd`. Point it at the synthetic
device (adjust the event number to whatever `/proc/bus/input/devices`
reports for `tca8418-keypad-sim`):

```bash
sudo ln -sf /dev/input/eventN /dev/input/by-id/tca8418-keypad-event-kbd
sudo chmod 666 /dev/input/eventN
```

Then start/restart `tezos_shim` — it only opens input devices once at
startup, no hot-plug support, so the symlink has to exist before launch.

## Useful key codes (from `<linux/input-event-codes.h>`)

| Code | Key | `translate_key()` maps to |
|---|---|---|
| 103 | `KEY_UP` | `TEZOS_INPUT_UP` |
| 108 | `KEY_DOWN` | `TEZOS_INPUT_DOWN` |
| 28 | `KEY_ENTER` | `TEZOS_INPUT_SOFTKEY_L` (select) |
| 1 | `KEY_ESC` | `TEZOS_INPUT_SOFTKEY_R` (back) |
| 55 | `KEY_KPASTERISK` | `TEZOS_INPUT_STAR` |
| 43 | `KEY_BACKSLASH` | `TEZOS_INPUT_HASH` |
| 2–10 | `KEY_1`..`KEY_9` | `TEZOS_INPUT_NUM`, value 1–9 (direct grid jump) |
| 11 | `KEY_0` | `TEZOS_INPUT_NUM`, value 0 |

## Feeding it keys interactively from a script

A named pipe keeps the device alive between individual key sends instead
of the program exiting after stdin closes:

```bash
mkfifo /tmp/keypad_fifo
sudo bash -c 'exec < /tmp/keypad_fifo; ./uinput_keypad' &
exec 3>/tmp/keypad_fifo   # holds the fifo open; closing this ends the session
echo 28 > /tmp/keypad_fifo   # send one ENTER keypress
```
