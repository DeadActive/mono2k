# Bank A

Sixteen parts after the late-90s and 2000s digital era (Warp, Rephlex and Skam electronica, IDM, drill'n'bass, early
digital hardware), with SOPHIE, Overmono, Anthony Naples, Pizza Hotline and Zorrovian as the modern lens on it.
Each is original, written for MONO2K's six machines (`firmware/src/m2k_factory.c`, one comment a part): the
references say what a part is after, never what it quotes. One idea a part, the rest supporting it; one or two
chords or a modal centre; every melodic track in the declared key; kicks and basses on the key.

They arrive in batches of four, each heard and approved before the next. Until a slot's part is here it holds the bank
before (ea69d53, `docs/bank-a-diagnosis.md`).

The table is read by the tools: `tools/demo_pitch.py` takes each part's key from it, `tools/demo_levels.py` the
tracks' roles (drums by peak against the kick, the rest by rms; `-` a track that rests).

| Slot | Name | Reference | Genre | BPM | Key | Idea | Tracks |
|---|---|---|---|---|---|---|---|
| A01 | HAZE | Boards of Canada | downtempo, hauntology | 88 | D minor | a dark, wobbling one-chord vamp and a dusty beat; four low notes over it | kick snare hat pad bass lead |
| A02 | SPLICE | Autechre | IDM, abstract electro | 104 | E phrygian | a low metal rhythm that mutates on an odd length (56 steps), over an open chord | kick perc hat lead pad bass |
| A03 | RIBBON | Aphex Twin | drill'n'bass | 168 | F minor | ratcheting breaks over a pedal F and a detuned, dark line; every second loop the drums drop out | kick snare hat lead pad bass |
| A04 | FACETS | Aleksi Perälä | raw digital ambient | 124 | A minor | a low, bit-crushed machine pulse on A over a crushed dark pad (Am(add9), Dm7), its filter and place moving at random | kick hat lead pad bass - |

Leads sit low (C3 to C4, never over C5) on the owner's darker and warmer waves, low-passed and worn (light drive, a
lower sample rate or fewer bits); they are fragments of a few long notes, or a sequence that is the texture. The
rhythm, the bass and the atmosphere carry each part.

## Detune

Only where it's the point: HAZE's tape (TAPE 80, about ±10 c, slow) and its pad's chorus; RIBBON's line, its
second oscillator 12.5 c sharp of the first, so it beats. Equal temperament everywhere, Aleksi Perälä's part
included.
