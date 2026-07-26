# tezOS Cyberdeck — Build Notes Index

Master index of all build-notes documents. Add a line here every time a new
build-notes file is created, so the full decision trail stays navigable.

| Doc | Covers |
|---|---|
| [camera-app-build-notes.md](./camera-app-build-notes.md) | Camera app: Scan mode vs. Capture mode, resolution presets, color-depth modes, box-average downsample pipeline, minting integration |
| [settings-app-build-notes.md](./settings-app-build-notes.md) | Input model (softkeys + up/down), Settings app scope, profile architecture, discreet profile switching, About screen contents |
| [hardware-roadmap-build-notes.md](./hardware-roadmap-build-notes.md) | Stage 1 custom carrier board (Zero 2 W) vs. Stage 2 Compute Module migration (CM4 vs CM5), decision status |

## Conventions for future build-notes files

- One file per app or subsystem, named `<name>-build-notes.md`.
- Add the new file to the table above the same session it's created.
- Record **decisions made**, not just options considered — mark open
  items explicitly as "Not yet decided" rather than leaving them implicit.
- Where a decision depends on another subsystem (e.g., an app's UI depending
  on the not-yet-defined tezOS visual/interaction design system), note the
  dependency explicitly rather than silently assuming it.
