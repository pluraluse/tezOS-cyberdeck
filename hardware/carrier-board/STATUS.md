# Status: real, decided as a v1-only concern — v0 uses point-to-point wiring

**Real, direct decision**: v0 (current bring-up) uses a magnet wire
harness — real, direct point-to-point connections, no PCB. The custom
carrier PCB is a **version 1 concern**, not part of getting the device
working for the first time.

Stage 1 custom carrier PCB for the Pi Zero 2 W (KiCad project).
Replaces the v0 magnet-wire harness for display, TCA8418, battery
management, and signer UART. Build after M1-M8 (see ROADMAP.md) and
after v0 hardware bring-up is actually working.

See ROADMAP.md and BOM.md at the project root for sequencing and scope.
