# MONO2K

Monomachine-style firmware for the M-VAVE FM-1 (six mono machines, 64-step sequencer with locks, conditions and
ratchets, a lofi bus, a 1-bit dithered Swiss screen). Plan and status: README.md.

## Hard rules

- Target: JieLi AC791N, no FPU, no libgcc: integer DSP only, **no 64-bit division** (64-bit multiply is fine),
  no floats in `firmware/src`. Tables come from `tools/gen_tables.py`.
- Memory: big buffers are `POOL` (Felucca's .pool section on the device). Keep RAM use in mind: delays 96 KB,
  glitch 64 KB, patterns ~31 KB.
- Machines are clean-room. Never copy or decompile code from Elektron's OS (the gearmulator ROM is a reference to
  listen to and measure, never a source). No Elektron or M-VAVE marks in the product name.
- The browser runs the same core: after any change to `firmware/src`, rebuild with `sh web/build.sh`.

## Build, test

- Generators: `.venv/bin/python tools/gen_tables.py`, `tools/gen_font.py [--sheet build/fonts.png]`,
  `tools/gen_icons.py [--sheet build/icons.png]`.
- Host: `cc -O2 -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/host/m2k_host host/m2k_host.c -lm && build/host/m2k_host build/host`,
  then `.venv/bin/python tools/montage.py build/host build/screens.png 2` to see every page.
  `build/host/m2k_host --selftest` checks the projects (save, reload, store, export / import, full images);
  `build/host/m2k_host build/patterns --analyze` solos every track of bank A and writes each pattern's WAV.
- Web: `sh web/build.sh`, serve `web/` (`.claude/launch.json`: mono2k-web on port 8742).
- Calibration (README, Calibration): `tools/ref/mm_ref` (gearmulator, the user's OS image at a path, never in the repo;
  it logs to stdout, so send stdout to /dev/null) against `host/m2k_ref.c`, same specs in `tools/ref/*.spec`.
  Pattern format: 2 amp ATK/HOLD/DEC/REL on the measured curve; 4 SWAV-SAW's page; 5 the track chain on the
  Monomachine's (DIST/DSND/EQG bipolar, the filter follows the note); older ones are converted on load, and the factory
  demos are written on the version-4 chain and converted the same way (`chain_v4`). The reference renderer writes
  32-bit float WAVs; `tools/ref/sweep.py`'s `load` reads both.

## Style

C99, 4 spaces, unity build, static everything, short comments that say why. The screen's grid: header 0..15, focus
18..121 (148 / 92 golden split), step strip 124..146, knobs 148..239.
