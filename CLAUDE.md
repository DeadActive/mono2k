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
- Web: `sh web/build.sh`, serve `web/` (`.claude/launch.json`: mono2k-web on port 8742).

## Style

C99, 4 spaces, unity build, static everything, short comments that say why. The screen's grid: header 0..15, focus
18..121 (148 / 92 golden split), step strip 124..146, knobs 148..239.
