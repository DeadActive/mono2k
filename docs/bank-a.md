# Bank A

One world: late-90s and 2000s digital-gritty IDM, the Warp, Skam and Rephlex era. Two flavours alternate across the
bank:

1. **Drummy, glitchy IDM** (after Aphex Twin and Autechre): the drums lead, chopped, ratcheted, crushed and locked
   (pitched retrigs, crushed hits, fills that change every loop by condition), over a heavy bass; little or no
   melody.
2. **Soft and digital-gritty, with a catchy deep melody** (after Boards of Canada): a slow, dusty hip-hop beat, a deep
   heavy bass, and a short motif you remember after one loop, worn by tape and grit.

A01 HAZE is the template, and stays as it is. Every part has a powerful low bass (roots C1 to C2, mono, sustained or
punchy, locked with the kick) and hard drums. Melodies are motifs of three to five notes in C3 to C4 (never over C5),
minor or modal, on the owner's darker and warmer waves. Every part is original: the references say what it is after,
never what it quotes. The bank's bus drive (`BANK_DRIVE`, about 6 dB into the soft clip, like a master limiter) sets
every slot at HAZE's loudness.

The bass rules (`tools/demo_pitch.py` checks both):

- A SWAV-SAW bass is played so its subs land in 30–80 Hz, never under 30 Hz. SUBX and SUB1 sound an octave under
  the note and SUB2 two, so for a root on G1, play G2 with SUB1, or G3 with SUB2.
- No SID bass until SID's level gap is fixed: MONO2K's SID stands 7 dB over the Monomachine's (the README's known
  gaps).

The parts arrive in batches of four, each heard and approved before the next; slots without a part from this brief
still hold an earlier one. The table is read by the tools: `tools/demo_pitch.py` takes each part's key from it,
`tools/demo_levels.py` the tracks' roles (drums by peak against the kick, the rest by rms while sounding; `-` a track
that rests) and prints each part's rms, low-end share (the energy in 30–120 Hz: under 30 Hz is not heard) and crest
beside HAZE's.

| Slot | Name | Flavour | Reference | BPM | Key | Idea | Tracks |
|---|---|---|---|---|---|---|---|
| A01 | HAZE | 2 | Boards of Canada | 88 | D minor | a dark, wobbling one-chord vamp and a dusty beat; four low notes over it | kick snare hat pad bass lead |
| A02 | SPLICE | v2 | Autechre | 104 | E phrygian | (brief v2, to be redone) | kick perc hat lead pad bass |
| A03 | RIBBON | v2 | Aphex Twin | 168 | F minor | (brief v2, to be redone) | kick snare hat lead pad bass |
| A04 | FACETS | v2 | Aleksi Perälä | 124 | A minor | (brief v2, to be redone) | kick hat lead pad bass - |
| A05 | SMOKE | v2 | Basic Channel | 120 | C minor | (brief v2, to be redone) | kick hat perc chord bass - |
| A06 | TORQUE | v2 | Ben Sims | 134 | E minor | (brief v2, to be redone) | kick hat snare perc lead bass |
| A07 | TUNNEL | v2 | Overmono | 130 | G minor | (brief v2, to be redone) | kick snare hat pad voice bass |
| A08 | MOTEL | v2 | Anthony Naples | 122 | B dorian | (brief v2, to be redone) | kick snare hat chord bass - |
| A09 | PYLONS | 2 | Boards of Canada | 84 | G minor | a deep SWAV-SAW bass walking under Gm9, Ebmaj7, Cm7, its sub on G1 Eb1 C2; a four-note motif, G D C Bb | kick snare hat pad bass lead |
| A10 | RIVETS | 1 | Autechre | 118 | C# minor | the drums lead, a swung broken funk: kick on the off-beats, ghost snares, accented hats, an FM clank; a punchy SWAV-SAW bass in 3 3 4 3 3, its sub on C#1-C#2 | kick snare hat perc bass perc |
| A11 | SYRUP | old | (the bank before) | 96 | A major | a favourite of the owner's, kept | kick snare hat pad lead bass |
| A12 | SPU | old | (the bank before) | 84 | C major | kept for now | hat pad lead voice lead bass |
| A13 | GROOVE | old | (the bank before) | 124 | G minor | kept for now | kick snare hat chord bass perc |
| A14 | ORCHARD | 2 | Boards of Canada | 92 | E dorian | a four-note call, G A B E, answered by the dorian C#; Em7, A7; a driven sine bass, its sub on E1 A1 B1 | kick snare hat pad bass lead |
| A15 | SPLINTR | 1 | Aphex Twin | 160 | F minor | drill breaks with pitched snare retrigs dropping out every second loop, over a huge sustained SWAV-SAW bass, its subs on F1 Ab1 Db1 Eb1 | kick snare hat perc bass lead |

After the DIST fix (each machine clipping at its own ceiling, as the Monomachine's do), HAZE, SPLICE and RIBBON were
set to sound as they were approved: a lower DIST on their DIGI-WAV leads, so the clip starts where it did, and the
level lost back on VOL. Each renders within −44 to −50 dB of its approved version. SPLINTR's bass was revoiced an
octave up with SUB2 off, by the sub rule, its subs no longer under 30 Hz, and heard and approved so.

## Detune

Only where it's the point: the tape's wobble in HAZE (TAPE 80), PYLONS (72) and ORCHARD (76), about ±10 c, slow, and
HAZE's pad chorus; SPLINTR's bass, a little of SWAV-SAW's unison. Equal temperament everywhere.
