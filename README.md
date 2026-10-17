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

Host build (renders the factory patterns to WAV, tours the pages and saves the screens as PBM; with `--analyze`, renders
every pattern and solos every track, flagging what is too quiet, clicky or clipping):

```bash
cc -O2 -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/host/m2k_host host/m2k_host.c -lm && build/host/m2k_host build/host
```

## What is in it

| | |
|---|---|
| Machines | GND-SIN, GND-NOIS, SID-8BIT, FM+STAT, FM+DYN, SWAV-SAW, SWAV-ENS, DIGI-WAV, VOX-6: clean-room designs after the Monomachine's families, integer only |
| Per track | the Monomachine's chain and pages: SYN A–H · AMP (ATK HOLD DEC REL DIST VOL PAN PORT) · FILT (HP / LP as base and width following the note, resonance, envelope) · FX (EQ, SRR, a delay of its own in 8-bit mu-law, ping-pong on positive sends) · DSTR (bits, fold, xor, SPU send) · 3 LFOs (any track's parameter) · ARP |
| Projects | the modern Elektron way: 4 projects × 4 banks (A–D) × 16 patterns, each pattern with its own sounds. SAVE keeps the pattern (its reload point), GLO + SAVE reloads it, SAVE held keeps the project; GLO + REC / PLAY / SEQ copy, paste, clear (the pattern on HOME, the track's sound on its pages); GLO > SET picks and loads a project. On the web: a project picker, export / import as `.m2kp` files. Stored packed (a typical pattern 0.5–1 KB; the DEMO project 32 KB) |
| Factory | project DEMO, bank A: sixteen parts after the late-90s and 2000s digital era, written in batches of four, each heard and approved (`docs/bank-a.md`: reference, genre, key and idea of each). So far HAZE (after Boards of Canada), SPLICE (Autechre), RIBBON (Aphex Twin), FACETS (Aleksi Perälä), SMOKE (dub techno), TORQUE (hardgroove), TUNNEL (Overmono), MOTEL (Anthony Naples); A09–A16 hold the bank before until theirs. Banks B–D blank. A store saved with earlier demos takes the new ones where the old still stand |
| Sequencer | 6 × 64 steps, note / accent / slide, parameter locks, trig conditions (chance, A:B, 1ST, PRE, FILL), ratchets, swing, live recording and step entry (REC while stopped), pages held to set the length, patterns queued at the bar |
| Bus | SPU-style reverb at 22 kHz, beat repeat (reverse, halving, tape stop), tape wow and flutter, 4-bit ADPCM crunch, hiss and dust, drive; each control on a law measured over its whole range (GLCH a chance per sixteenth, 0.3–50 %) |
| MIDI | channels 1–6 are tracks 1–6, notes and the Monomachine's CC map (SYN 48–55, AMP 56–63, FILT 72–79, FX 80–87, LFO 1/2/3 on 88/104/112, level 7, mute 3), start / stop |
| Screen | 240 × 240 at 1 bit (a 7.2 KB framebuffer): Orbitron in outline-and-dither (one size for every big value), Model:Cycles-style machine cards, machine marks as one family of cut-out stamps after Fors' isotypes (flat, and extruded with dithered sides), dissolves, glitches, live envelopes, filters, LFOs and scopes; light or dark (GLO > SET > SCRN; on the web it follows the page's theme) |

## Where things are

- `firmware/src/m2k.h`: the API the platform calls (render, keys, buttons, encoders, MIDI, frame, LEDs, store).
- `firmware/src/m2k_core.c`: the unity build; `m2k_dsp.c` machines and the track chain, `m2k_fx.c` delays and the bus,
  `m2k_seq.c` sequencer, `m2k_factory.c` the factory patterns (a step grammar), `m2k_proj.c` projects, banks and the
  store, `m2k_gfx.c` 1-bit drawing, `m2k_ui.c` pages and controls.
- `tools/gen_tables.py`, `gen_font.py`, `gen_icons.py`: the tables, fonts and marks, into `firmware/gen/` (Pillow and
  numpy in `.venv`).
- `web/`: the emulator page; `m2k_web.c` is the core plus exports, `worklet.js` runs it, `app.js` draws the panel.
- `host/m2k_host.c`: the host harness.

## The next steps

1. **The FM-1 build**: Felucca 1.6.1's platform (HAL, loader, USB, storage, installer) around this core, the audio on
   the second core as CS-1 does, the 1-bit framebuffer blitted in strips with the palette's two colours. The projects'
   images move to the 1 MiB flash (`m2k_proj.c` get / put), and RAM keeps the playing bank, not all 64 patterns.
2. **Calibration against the Monomachine**, machine by machine: levels, spectra, the machines' own envelopes. The amp
   envelope, SWAV-SAW and the whole track chain are done (below); SWAV-ENS, SID, FM+, VO-6 and GND-NOIS are next.
3. More machines (DPRO-BBOX, FX machines that take the previous track), songs, MIDI out and clock.

## Author

MONO2K is made by Sequencist: [GitHub](https://github.com/InigoHernandez) ·
[Bandcamp](https://sequencist.bandcamp.com/) · [YouTube](https://www.youtube.com/@sequencist) ·
[Instagram](https://www.instagram.com/sequencist.fm) · [SoundCloud](https://soundcloud.com/sequencist)

## Calibration

MONO2K's machines are its own, but they are measured against the Monomachine. `tools/ref/mm_ref` boots an owner's
Monomachine OS image in [gearmulator-md-mm](https://github.com/joelanders/gearmulator-md-mm)'s emulation and plays it
over MIDI only, as the instrument is played; `host/m2k_ref.c` plays MONO2K the same way; the scripts compare them. The
OS image is read from the path given and never copied: no ROM, no ROM code and no recordings of it are in this repo.

```
cmake -S tools/ref -B build/ref -G Ninja -DGEARMULATOR=/path/to/gearmulator-md-mm && cmake --build build/ref
build/ref/mm_ref /path/to/os.bin render build/ref/mm < tools/ref/amp.spec > /dev/null
build/host/m2k_ref build/ref/m2k < tools/ref/amp.spec
.venv/bin/python tools/ref/envtimes.py build/ref/mm build/ref/m2k
```

Done so far, all measured on OS 1.32B:

- **The amp envelope.** The attack rises in a straight line in 0.90 ms × e^(0.0814·v), decay and release fall 20 dB
  in 8.3 ms × e^(0.0665·v), the hold lasts 14.6 ms a step; a key let go ends the hold and the decay. MONO2K follows
  within a few percent from 0 to 127.
- **SWAV-SAW** on SWAVE-SAW's page: the unison pairs' detune and levels, the square and sine subs, within 0.2 c and
  0.5 dB (`tools/ref/swave_saw*.spec`).
- **SWAV-ENS** on SWAVE-ENS's page (`tools/ref/chain_ens15..20.spec`, `ensacc`): PCH2–4 off its interval table (−12
  to +12 semitones, 61–63 OFF, the just 3/2 4/3 5/4 6/5 beside their equal-tempered neighbours: all 66 group ends
  within 0.4 c); a voice two saws an octave under its note, the second PW/128 of a period on, WAVE fading their sum
  out and their difference in at 0.63 (levels within 0.8 dB over WAVE × PW); no level shared between the voices
  (0.4 dB); TUNE ±2 semitones (0.7 c); the chorus a copy whose pitch travels a triangle, CHRL its mix (within 1.5 dB).
  Bank A's chords, converted, against the Monomachine's voice by voice: within 0.5 c. MONO2K's chorus is mono, where
  the Monomachine's widens the stereo by CHRW (a known gap).
- **The track chain** (`tools/ref/chain.py` writes the specs, `chain_anal.py` reads them), in the Monomachine's order:
  - VOL and LEV squared; PAN cos / sin; the track clips per channel after the pan, just under full scale at LEV 127.
    MONO2K matches within 0.02 dB.
  - DIST: below 64 headroom, 1.35 dB a step; 65–72 nothing; above, a gain into a hard clip whose ceiling falls from
    +5.8 dB to −3.2 dB of a full-scale machine between 64 and 80. Within 0.15 dB.
  - The filter follows the note: a cutoff of 2^((step − 16)/8) × the note, for the HP (BASE) and the LP (BASE + WDTH),
    exact from C1 to C7. The LP is a resonant two-pole (1.35 × the cutoff at LPQ 0, onto it as the Q rises, up to
    Q ≈ 1000 and +54 dB without oscillating), the clip, then two one-poles at 1.55 ×; the HP a resonant two-pole and
    a one-pole at 1.75 ×, 18 dB an octave. Within 0.1 dB (LP) and 0.4 dB (HP, to −44 dB); the resonance within 0.3 %,
    and from Q 96 up both peaks sit 0.18 % under the cutoff, as the Monomachine's do. The filter follows GND-SIN's TUNE
    too (±2 semitones, 3.125 c a step). Its coefficients are exact down to 20 Hz: a low cutoff's damping used to round
    away (at 131 Hz HPQ 127 rang on without decaying).
  - The amp envelope comes after the filter and the drive: a note that decays keeps its distortion, and the
    Monomachine's oscillator keeps feeding the filter between notes.
  - BOFS / WOFS move the cutoff 7·d·|d|/256 steps; the filter envelope runs in straight lines (decay 863 ms at 64,
    ×1.565 every 8 steps; 127 holds).
  - EQ: a band added to the signal, Q 2.2, +36 / −31 dB, its cut the inverse of a boost as deep. SRR: a rate from
    44.1 kHz to 2.8 kHz, the samples joined by straight lines (its images match within 1 dB).
  - The delay: DTIM in 256ths of a note plus 7.6 ms, the send squared, negative keeping the source's side and positive
    ping-pong, feedback DFB / 64, the echoes through a one-pole HP and LP. Times within 0.5 ms.

  Known gaps (`tools/ref/regress.py`, each held to its measured size): the Monomachine's HP saturates inside its
  resonance (from about −16 dBFS out at DIST 64, a square through a high-pass) where MONO2K's stays near clean, and
  falls 7–9 dB further an octave under a resonant cutoff; its Q moves with the cutoff (HPQ 96: ~230 at 8.4 kHz, ~160
  at 4.2 kHz), MONO2K takes one value per setting. Tried and set aside: a clip on the two-pole's output with a one-pole
  at the cutoff after it matches the saturated tone (`tools/ref/hpsat.py`: h1 within 0.7 dB, h3 h5 within 1 dB) but
  clips every unresonant drive (a square at the ceiling overshoots the two-pole; the Monomachine passes that); the
  pole alone, appearing from HPQ 24 (`tools/ref/hpfit.py`: four poles fit the resonant skirt on noise at C4), closes
  the passband an octave over a resonant cutoff at C3 but not at C4, where the Monomachine's is level: the shape, like
  the Q, depends on the cutoff. The next candidate: the HP's Q and its skirt pole from a table over HPQ × cutoff (two
  dimensions, an octave a column), fitted on resonant sweeps at several notes, and the saturation inside the
  resonance. The delay keeps half-rate
  8-bit lines, so ping-pong past 0.37 s (and any delay past 0.74 s) slows the line's clock and darkens, where the
  Monomachine stays bright; SWAV-SAW's top harmonics at C7 are 2–6 dB short.

Patterns saved before are moved onto each new scale as they load (version 1: the envelope; 5: the chain, each old
setting to the one giving the same Hz, dB or time, the filter placed against the note each track plays most; 6:
GND-SIN's TUNE, which moved a semitone a step, kept to the semitone within the new ±2; 7: SWAV-ENS's page, each
old interval folded into −11..+12 by octaves, one folding to 0 to +12, keeping every chord's notes; its DTUN (a fixed
spread, not the chorus's moving copy), shape, level and sub have no counterpart and reset; a trig the old level
silenced has its intervals locked OFF; an LFO aimed at the old page is cleared, intervals included, since the new axis
has OFF and an octave down in it; 8: the wave bank's choice, MONO2K's 32 for every older pattern; 9: the bus's
controls on their new laws, each old value to the new one that measures the same, HISS by its crackle).

The bus's controls are MONO2K's own, so their laws are a design choice, made by measurement (`m2k_host DIR --bus`,
`tools/bus_curves.py`: every value of each on a reference loop). Before, most of each range sat past where it was
already intense: GLCH 10 replaced the mix a tenth of the time and DRIV 10 drove it 2.9 dB into the knee; HISS's
crackle came in long before its hiss could be heard; TAPE's wow grew 0.33 c a step to ±42 c; ADPC had three sounds
over 64–127. Now GLCH, DRIV and HISS's crackle put the old 10 at 64 and grow by a ratio a step (0.3 % a sixteenth at
GLCH 1); TAPE puts the old 20 (±6.4 c) at 64; HISS's hiss has its own law (65 dB under the mix at 1, 40 at 64);
ADPC keeps 0–64 and its rate falls continuously above. The old extremes stay at 127. Measured on the old engine and
the new, every old value converts to one that measures alike (the hiss there as loud or louder: its law brings it
in sooner); `tools/check.sh` holds both.

DIGI-WAV plays either bank, set on SYN's third page (BANK: M2K, MONO2K's 32 waves across WAVE's range as before; M64,
the owner's 64, two WAVE values a wave: an even one is that wave exactly, an odd one between two). The owner's play
without their DC (each cycle's mean, up to -29 % of the peak, taken out at playback, the files and the table kept as
they are): with it a trig stepped, 13-17 dB more under 60 Hz at the onset on OS13, OS36 and OS48; four that would
pass 16 bits so (OS29, OS37, OS48, OS58) play 0.4-1.4 dB down. MONO2K's 32 play as before. Every wave of both
at C2, C4 and C6 (`tools/wave_check.py`): the owner's in tune within 0.2 c, their levels and aliasing within the range
of MONO2K's own (the aliasing of a 256-point table read without a band limit, both banks alike), their thump at a
trig within MONO2K's worst (one of its own, 9.1 dB, kept: saved patterns play it). The
DigiPro waves and BeatBox samples are the OS's data and are not copied: DIGI-WAV plays MONO2K's own waves.

## Credits and licence

GPL-3.0-only. Built on the open FM-1 work: Felucca by Leo Kuroshita (Hügelton Instruments), CS-1, X0X, FoMni, zp12
and the others. Orbitron by The League of Moveable Type: SIL OFL 1.1 (`assets/fonts/orbitron/OFL.txt`); the firmware
carries 1-bit renderings of it. The FM-1's panel geometry as measured for
CoralFlower's emulator. MONO2K is not affiliated with Elektron or M-VAVE; Monomachine and Machinedrum are Elektron's,
FM-1 is M-VAVE's.

The 64-wave bank (`assets/waves/mono2k64/`) is the project owner's own waveforms, licensed with the project
(GPL-3.0-only) unless the owner says otherwise.