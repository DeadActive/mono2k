# Bank A (ea69d53): why it sounds melodically off

Measured with `build/host/m2k_host build/pitch --pitch` (each track alone, two loops, every trig logged with the note
the engine was given and the parameters in effect; then the track's most played note held 1.5 s on its kit) and
`.venv/bin/python tools/demo_pitch.py build/pitch [--steps]`.

Estimator: a harmonic comb (up to 10 harmonics under 9 kHz) on a Hann window of each trig's steady part (from 15 ms
after the trig to the next trig, at most 0.6 s, trimmed to where it stands within 30 dB of its peak), 8x zero-padded,
searched ±120 c in half cents. SWAV-ENS chord voices: the strongest partial within ±80 c of each intended voice.
"In context" is the median over a track's trigs; "held" is the engine alone. Reliable on clean held tones (DIGI-WAV
0.0 c, the 808 +0.2 c); unreliable for inharmonic FM, short trigs of a pitch-swept kick, and strong vibrato (a window
of a few cycles lands off centre): those are flagged, not counted as errors.

## Engine: what it was

| Cause | Measured | Where | Status |
|---|---|---|---|
| The bus's tape flutter took a quarter of the delay swing at 11x the wow's rate: ±58 c at TAPE 50, ±25 c at 22-24, ±10-12 c at 8-12 (±172 c at 127) | sine through the bus, per-cycle pitch | A01 A02 A03 A07 A08 A11 A12 | **fixed**: ±10.7 c at 127, ±4 c at 50; the wow (the "worn tape") kept. Self-test fails before |
| SWAV-ENS's DTUN spreads the voices by -dt/2, +dt, -1.5 dt, +2 dt (dt = DTUN/2 in 1/64 semitone): at DTUN 50-70 the chord's voices sit -4..-52, +23..+54, -36..-79 and +57..+81 c off | held and in context, each voice | every SWAV-ENS chord: A01 A02 A03 A07 A09 A11 A12 A13 | **open**: SWAVE-ENS is the next machine (Phase 2): its detune is to be measured on the Monomachine and rebuilt before the new bank |
| GND-SIN's TUNE moved a semitone a step | | | fixed in fe73fa5; no demo used it |

No other tune scaling is wrong: PTCH (semitones, shown as SEMI), SID DTN2 (whole semitones, shown so), SID and FM
FINE (1.56 c, shown in cents), SWAV-SAW TUNE (3.125 c) all play what they show. DIGI-WAV's osc 2 DTUN moves 12.5 c
a step (±8 semitones) and shows a bare number. Held notes of GND-SIN, SID, FM, SWAV-SAW, DIGI-WAV and VOX-6 (vibrato
off) sit within ±3 c of the note, plus the tape's wow where a pattern sets TAPE.

No format conversion moves a pitch: every demo saved and reloaded as version 4 and as 5 keeps its notes, PTCH, machine
pages and their locks (self-test; a PTCH shift in the v4 path and a note shift in the v5 path both fail it).

## Voicing: what the demos chose

- **Resonant LP sweeps on the SID basses pull short notes ~20 c flat** (A04 A05 A06 A13 A14 A16; A15 -48 c): as the
  envelope sweeps the resonant cutoff down through the harmonics their phase turns, bending them. Opened (LPQ 0,
  WDTH 127) the same lines measure +0.1 c (A04, 90 % within 1.2 c). Acoustic, not a fault; on short bass notes, audible.
- **Resonant cutoffs ringing off the key.** The filter follows the note in steps of a tone and a half, so a resonant
  LP rings at a fixed interval over every note: +15 st (A04 A05 A13 A16), +18 st (A06: an octave and a tritone),
  +6 st at Q 46 (A09: a tritone, the strongest), +21 st on A07's pad. Off the key on 18-55 of each line's trigs.
- **GND-SIN's default pitch drop left on melodic lines**: PDEP 40 / PDEC 40 starts each note up to 15 semitones high
  (A01 T5, A02 T5, A07 T5, A11 T6, A12 T6: basses and subs).
- **VOX-6 vibrato** at VIBR 40-50 swings ±31-39 c (A06, A08, A12).
- **Inharmonic FM on a melody**: A11's bell (ratios 3.5 over 2.76) has no clear pitch.

## Composition: the notes

| Part | Declared | Finding |
|---|---|---|
| A01 DEEP | D minor | in key |
| A02 JUNGLE | F minor (Fm9 Dbmaj7 Bbm9 C7sus4) | in key |
| A03 DAYDRM | F major | in key (maj7 tensions only) |
| A04 ELECTRO | A minor | in key |
| A05 GLITCH | C minor | in key |
| A06 PRISM | E dorian | **the DIGI-WAV line (E D C B D) and the SID bass play C natural against the FM arpeggio's C#**; the kick sits at C |
| A07 FOG | F minor | the Ebmaj7 brings D natural against the voice's Eb |
| A08 LIQUID | D-flat | **the bass passes through B (C-flat)**; the kick at C, a semitone under the root |
| A09 DREXC | E minor | in key (the SID's tritone ring above) |
| A10 STUTTR | G minor | F# on three lines: harmonic minor, consistent |
| A11 SYRUP | A major | **the tonal kick at C, against C# in every chord** |
| A12 SPU | C major | in key |
| A13 GROOVE | G minor | in key |
| A14 ROBOT | C minor | B and F#: harmonic minor (the G chord), consistent |
| A15 BRAIN | D dorian | in key |
| A16 SHARDS | F# minor | in key |

Every GND-SIN kick is pitched at C whatever the key (A04 and A09 excepted); with long, tonal decays it is part of the
harmony.

## For the new bank

Measure each melodic track with the two commands above; keep it within a few cents unless the detune is the point,
and state it then. Pitch kicks to the key. Zero GND-SIN's pitch drop on melodic lines. Keep resonant cutoffs on whole
octaves (BASE / WDTH + 8 n) or low Q on bass lines, or make the ring part of the key.
