# MONO2K

**Six mono machines for the M-VAVE FM-1.** A Monomachine-style synthesizer firmware: six tracks, each one machine,
a 64-step sequencer with parameter locks, trig conditions and ratchets, and a lofi bus that breaks the sound
beautifully. A 1-bit screen: dithered type and machine marks, Swiss grid, PS1-era glitches.

> **Status: prototype (v0.1).** The core (sound, sequencer, screen) runs in the browser and on a computer. The FM-1
> build (Felucca's platform around this core) is the next step; nothing has been flashed yet.

**Try it in your browser:** <https://inigohernandez.github.io/mono2k/> (the firmware's own core, compiled to
WebAssembly, with sound).

## Run it

```bash
sh web/build.sh
```

```bash
python3 -m http.server 8742 --directory web
```

Then open <http://localhost:8742>, press Start, click the panel and play (Z… for keys, Space PLAY, A GRID, 5 SYN).

Host build (renders the factory patterns to WAV, tours the pages and saves the screens as PBM):

```bash
cc -O2 -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/host/m2k_host host/m2k_host.c -lm && build/host/m2k_host build/host
```

## What is in it

| | |
|---|---|
| Machines | GND-SIN, GND-NOIS, SID-8BIT, FM+STAT, FM+DYN, SWAV-SAW, SWAV-ENS, DIGI-WAV, VOX-6: clean-room designs after the Monomachine's families, integer only |
| Per track | the Monomachine's chain and pages: SYN A–H · AMP (ATK HOLD DEC REL DIST VOL PAN PORT) · FILT (HP / LP as base and width, resonance, envelope) · FX (EQ, SRR, a delay of its own in 8-bit mu-law) · DSTR (bits, fold, xor, SPU send) · 3 LFOs (any track's parameter) · ARP |
| Sequencer | 6 × 64 steps, note / accent / slide, parameter locks, trig conditions (chance, A:B, 1ST, PRE, FILL), ratchets, swing, live recording, patterns queued at the bar |
| Bus | SPU-style reverb at 22 kHz, beat repeat (reverse, halving, tape stop), tape wow and flutter, 4-bit ADPCM crunch, hiss and dust, drive |
| MIDI | channels 1–6 are tracks 1–6, notes and the Monomachine's CC map (SYN 48–55, AMP 56–63, FILT 72–79, FX 80–87, LFO 1/2/3 on 88/104/112, level 7, mute 3), start / stop |
| Screen | 240 × 240 at 1 bit (a 7.2 KB framebuffer): Inter Tight in outline-and-dither, Model:Cycles-style machine cards, machine isotypes after Fors' marks (flat, and extruded with dithered sides), dissolves, glitches, live envelopes, filters, LFOs and scopes; light or dark (GLO > SET > SCRN; on the web it follows the page's theme) |

## Where things are

- `firmware/src/m2k.h`: the API the platform calls (render, keys, buttons, encoders, MIDI, frame, LEDs, store).
- `firmware/src/m2k_core.c`: the unity build; `m2k_dsp.c` machines and the track chain, `m2k_fx.c` delays and the bus,
  `m2k_seq.c` sequencer and factory patterns, `m2k_gfx.c` 1-bit drawing, `m2k_ui.c` pages and controls.
- `tools/gen_tables.py`, `gen_font.py`, `gen_icons.py`: the tables, fonts and marks, into `firmware/gen/` (Pillow and
  numpy in `.venv`).
- `web/`: the emulator page; `m2k_web.c` is the core plus exports, `worklet.js` runs it, `app.js` draws the panel.
- `host/m2k_host.c`: the host harness.

## The next steps

1. **The FM-1 build**: Felucca 1.6.1's platform (HAL, loader, USB, storage, installer) around this core, the audio on
   the second core as CS-1 does, the 1-bit framebuffer blitted in strips with the palette's two colours.
2. **Calibration against the Monomachine** (the user's own OS file, in the gearmulator plugin, never shipped): a
   reference renderer, then the machines tuned note by note.
3. More machines (DPRO-BBOX, FX machines that take the previous track), songs, MIDI out and clock.

## Credits and licence

GPL-3.0-only. Built on the open FM-1 work: Felucca by Leo Kuroshita (Hügelton Instruments), CS-1, X0X, FoMni, zp12
and the others. Inter Tight: SIL OFL 1.1 (`assets/fonts/OFL.txt`). The FM-1's panel geometry as measured for
CoralFlower's emulator. MONO2K is not affiliated with Elektron or M-VAVE; Monomachine and Machinedrum are Elektron's,
FM-1 is M-VAVE's.
