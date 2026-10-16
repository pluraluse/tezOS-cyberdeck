# tezos-cyberdeck ("tezOS") — Compile & Test Execution Plan

**Audience**: a senior engineer (human or agent) with no prior context on this
project, working in a Debian-based development environment (Debian/Ubuntu or
derivative — the commands below assume `apt`).

**Purpose**: get the full project building and passing its own test suite in
your environment, verify the claims this document makes are actually true by
re-running them yourself, and produce an accurate status report — not to
redesign or extend the project. Where you find something that doesn't match
what's claimed here, that's a real finding: fix it if it's small and obvious,
flag it clearly if it isn't.

**Scope boundary, stated up front**: this environment (a Debian dev box, not
the target Raspberry Pi) can build and run everything **except** real display
output (fbdev) and real physical input (evdev from a keypad). Those two
things are architecturally isolated behind a `tezos_fb_t` struct and evdev
file descriptors specifically so everything else — the render logic, the
entire crypto/chain layer, every app screen's actual behavior — is fully
testable without real hardware. Don't try to fake an fbdev device; there is
no meaningful way to do that, and the project doesn't need you to.

(Historical note: `linux_shim.c` originally targeted DRM/KMS. Real hardware
bring-up found the working panel overlay uses the kernel's legacy `fbtft`
staging driver instead, which exposes a plain fbdev device
(`/dev/fb1`), not a DRM one — see
`docs/build-notes/linux-shim-fbdev-rewrite-build-notes.md`. The shim was
rewritten accordingly; this document now reflects that.)

---

## 1. What this project actually is

A Tezos hardware wallet / cyberdeck: custom window manager ("tezOS") on a
Raspberry Pi Zero 2 W, airgapped signer as the trust boundary, running a
suite of Tezos-focused apps (wallet, block explorer, contract scanner,
NFT gallery, social messaging, camera). ~4,700 lines of C across 58 source
files, plus 21 permanent test suites (171 individual assertions).

**Read `README.md` first** — it has the accurate, current file tree with
inline notes on what's real vs. placeholder for every module. Then
`ROADMAP.md` for milestone-by-milestone status. Don't skip this; guessing at
project structure instead of reading the existing docs is how stale
assumptions get reintroduced (this has happened at least twice in this
project's history — see `docs/build-notes/wallet-app-build-notes.md` for
two documented examples of exactly that mistake and how it was caught).

### Current real/placeholder split (verify this yourself, don't just trust it)

**Real, verified, working:**
- Core render engine, window manager, input state machine (`src/core/`)
- Full chain layer: Base58, operation forging, ed25519 signing, BIP39/SLIP-10
  HD derivation, Michelson/Micheline encoding, TZKT reads, RPC operation
  broadcasting, Beacon (TZIP-10) crypto/message/QR/pairing, a spec-verified
  Matrix client (`src/chain/`)
- Three real apps: Wallet, Settings, Camera (`src/apps/`)

**Explicit placeholders (by design, clearly marked in code comments):**
- Signer is software-only (`tezos_mock_signer_*`) — no real STM32/Pico
  firmware exists, no physical confirmation step exists
- Explorer, Scanner, Gallery, Discover, Messenger, 1-bit Art apps are all
  unimplemented stub screens
- No storage layer — nothing persists across a restart
- No WiFi configuration UI
- RPC node URL in `wallet.c` (`DEV_RPC_NODE`) is a placeholder, not a
  production configuration mechanism

---

## 2. Environment setup

```bash
sudo apt update
sudo apt install -y \
    build-essential gcc gdb \
    libsodium-dev \
    libcurl4-openssl-dev \
    libcjson-dev \
    libqrencode-dev \
    libzbar-dev \
    pkg-config \
    unzip zip \
    python3 python3-pil
```

Notes on why each is here (don't skip packages because they seem unrelated
to "compiling C" — several of these back real, load-bearing functionality):
- `libsodium-dev` — all cryptography (ed25519, BLAKE2b, X25519, secretbox)
  goes through real libsodium, not a hand-rolled implementation. Verify
  this is actually true by grepping for it — if you find hand-rolled crypto
  primitives anywhere, that's a serious finding, report it immediately.
- `libcurl4-openssl-dev`, `libcjson-dev` — the network client (TZKT reads,
  RPC injection, Matrix client) and every JSON message schema.
- `libqrencode-dev`, `libzbar-dev` — real QR encode/decode for Beacon
  pairing and Camera's Scan mode. Not a placeholder library.
- `python3-pil` — used only for converting the project's `.ppm` test-output
  images to `.png` for visual inspection. Not required to build or test;
  convenient if you want to actually look at rendered screens.

If any `apt install` fails or a package is unavailable on your specific
Debian version, note the exact error and find the equivalent package name
for your distro — do not silently skip a package and continue.

---

## 3. Unpack and build

```bash
unzip tezos-cyberdeck.zip
cd tezos-cyberdeck
```

### 3a. Run the permanent test suite first

This is the fastest way to get a real signal on whether your environment
matches what this was built against.

```bash
chmod +x tests/run_all_tests.sh
./tests/run_all_tests.sh
```

**Expected result**: `SUMMARY: 21 passed, 0 failed`.

If anything fails here, **stop and diagnose before proceeding** — this
suite covers cryptography verified against official test vectors (BIP39,
SLIP-0010, Taquito's own forging/signing reference output, the official
Matrix spec, the real `beacon-sdk` source). A failure here means either
your environment has a real problem (wrong library version, ABI mismatch)
or something in the codebase has regressed. Either way, don't proceed to
hardware-adjacent work on top of a failing foundation.

Two of the 21 suites (`test_network.c`, `test_rpc.c`'s sibling checks,
`test_matrix.c`) attempt real live network calls. This environment likely
has normal internet access (unlike the sandbox this project was originally
built in, which was domain-restricted) — if so, you may see these
suites do something genuinely new: a live TZKT call succeeding, for
instance. Read the printed output, not just the pass/fail count, for these
specific suites; they say plainly whether the live call worked or fell
back to fixture-only verification, and either outcome is fine.

### 3b. Build the actual target binary (`linux_shim.c`)

This is the real binary that would run on the Pi. It will **not run** here
(no real `/dev/fb1`), and that's expected — you're verifying it *compiles
and links* cleanly against the real dependencies, matching exactly what
ships.

**Use globs, not a hand-maintained file list.** An earlier version of this
exact section hand-listed source files and silently drifted stale —
missing newer apps (messenger, gallery, explorer, discover, beacon_wallet,
scanner, snake) and even `tezos_boot_splash.c` itself. This is the same
drift bug `scripts/install-tezos-service.sh`'s own comments document being
bitten by twice; use that script's actual current command (or run the
script itself) rather than retyping a list by hand:

```bash
cd src/shim-linux
CORE_SOURCES=$(ls ../core/*.c | grep -v demo_idle_screen.c)  # demo_idle_screen.c has its own main()
APP_SOURCES=$(find ../apps -name '*.c')
gcc -O2 -Wall -Wextra -std=c11 -o tezos_shim linux_shim.c \
    $CORE_SOURCES \
    ../chain/*.c \
    $APP_SOURCES \
    -I../core -I../chain -I../apps/wallet -I../apps/settings \
    $(pkg-config --cflags --libs libsodium) \
    -lcurl -lcjson -lqrencode -lzbar -lm
```

**Expected result**: exit code 0, zero warnings at `-O2` (this exact
optimization level matters — this project has already caught real bugs
that only surfaced at `-O2` and not at the default; don't drop the `-O2`
flag).

Then run it:

```bash
./tezos_shim
```

**Expected result**: it prints a clear error about `/dev/fb1` not
existing and exits with status 1. **This is the correct, designed
behavior in this environment, not a bug** — verify the failure message
matches "fbdev init failed" / "open(/dev/fb1) failed" and nothing else
looks wrong (a segfault, a different error, a hang would all be real
problems; a clean one-line error and exit is exactly right).

This exact command is also what `scripts/install-tezos-service.sh` runs
(that script additionally handles the systemd service install for the
real Pi) — if you want to sanity-check the script itself rather than
type the command by hand, read through it, but don't actually run its
`sudo systemctl` steps in this dev environment; those are meaningless
without a real Pi and a real panel.

---

## 4. Deeper verification — go beyond "it compiles"

The test suite in step 3a is real, not superficial, but a senior engineer
reviewing this project should independently confirm a few of its stronger
claims rather than just trust the printed "PASS" lines. Pick at least a
few of these:

### 4a. Re-verify a cryptographic claim independently
`tests/test_signer.c` claims the signer produces a signature that
byte-for-byte matches Taquito's own published test vector. Don't just
trust the test — open the file, find the expected signature string, and
manually confirm it against Taquito's actual source
(`packages/taquito-signer/test/taquito-signer.spec.ts` in
`github.com/ecadlabs/taquito`) if you want full confidence.

### 4b. Confirm the render engine actually renders, not just "doesn't crash"
```bash
cd tests
gcc -Wall -Wextra -std=c11 -o /tmp/render_check test_wallet_screens.c \
    ../src/apps/wallet/wallet.c ../src/core/tezos_core.c ../src/core/tezos_gfx.c \
    ../src/core/tezos_fonts.c ../src/core/tezos_widgets.c ../src/core/tezos_t9.c \
    ../src/core/tezos_popup.c ../src/chain/tezos_base58.c ../src/chain/tezos_forge.c \
    ../src/chain/tezos_signer.c ../src/chain/tezos_bip39.c ../src/chain/tezos_slip10.c \
    ../src/chain/tezos_micheline.c ../src/chain/tezos_http.c ../src/chain/tezos_tzkt.c \
    ../src/chain/tezos_rpc.c \
    -I../src/core -I../src/chain -lcurl -lcjson $(pkg-config --cflags --libs libsodium)
/tmp/render_check
```
Most test files write `.ppm` output for visual screens (check each test
file's source for `tezos_fb_dump_ppm` calls to find the exact output
filenames). Convert and actually look at a few:
```bash
python3 -c "from PIL import Image; Image.open('somefile.ppm').save('somefile.png')"
```
Confirm what you see matches what the test's `CHECK()` messages claim.

### 4c. Confirm the letterbox/logical-canvas claim
`tests/test_letterbox.c` claims the render pipeline can correctly display
a 320×480-designed UI on a mismatched physical resolution (e.g. testing
via HDMI to a 1080p monitor) without distortion. This is architecturally
important — see `docs/architecture/CORE-ARCHITECTURE.md`'s "logical
canvas" section for why. Re-run this test, then actually generate and view
the visual output to confirm it looks like a correctly-proportioned
portrait UI centered on a black background, not a stretched mess.

---

## 5. Known limitations — do not attempt to "fix" these in this environment

Be aware of these so you don't waste time trying to work around them or,
worse, build something that fakes them into appearing to work:

1. **No real display output is possible here.** Don't stub
   `/dev/fb1` or hack around `fbdev_init()` — there is no meaningful
   way to verify real display output without a real Pi + real panel.
2. **No real evdev keypad input is possible here.** Same reasoning.
3. **Live network calls in `test_network.c`/`test_matrix.c` may or may not
   succeed** depending on this environment's actual network policy —
   neither outcome is a defect in the code being tested.
4. **The signer is deliberately, explicitly insecure software** — it holds
   a private key in plain process memory. This is not a bug to fix; it's
   a documented placeholder standing in for real signer hardware that
   doesn't exist yet (see `docs/architecture/SIGNER-PROTOCOL.md`).
5. **Six app screens are stub placeholders** (Explorer, Scanner, Gallery,
   Discover, Messenger, 1-bit Art) — pushing them will show a generic
   "not built yet" screen. This is expected, not something to debug.

---

## 6. Deliverable expected back

A short written report (a few paragraphs, not another huge document)
covering:

1. **Environment**: exact Debian/Ubuntu version, `gcc --version`, package
   versions installed for the libraries above.
2. **Test suite result**: the exact `SUMMARY: N passed, M failed` line,
   plus the full output of any suite that didn't pass.
3. **Full-stack build result**: exit code and full compiler output
   (warnings included, even if the build still succeeded) for the
   `linux_shim.c` build in step 3b.
4. **Any discrepancy found** between what this document (or the project's
   own `README.md`/`ROADMAP.md`/`docs/`) claims and what you actually
   observed — however small. This project's own build notes document
   several real bugs caught by exactly this kind of independent
   re-verification; finding another one is a legitimate, valuable outcome
   of this exercise, not a failure of the plan.
5. Anything in section 4 you spot-checked, and what you found.

Do not use this exercise to start building new features, fixing the
placeholder gaps listed in section 1, or extending test coverage unless
explicitly asked to as a separate task — the scope here is compile,
test, verify, report.
