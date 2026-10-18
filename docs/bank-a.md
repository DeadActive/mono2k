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
| A04 | TENFOUR | 2 | Radiohead | 108 | F minor | after "Everything in Its Right Place": its opening progression, C Dbmaj7 Eb6 with C on top, over four bars (6, 4, 4 beats, then 2), swelling on a tape-worn crushed pad; a driven sine bass, its sub on C1 Db1 Eb1; a four-bar line from the first step, held and sliding; drums that never quite repeat | kick snare hat pad bass lead |
| A05 | SMOKE | v2 | Basic Channel | 120 | C minor | (brief v2, to be redone) | kick hat perc chord bass - |
| A06 | CAROM | 1 | Aphex Twin | 160 | A minor | a metal hit bouncing through each bar, closing in, over a dense industrial beat with rolls; a wave-folded sine bass boinging onto each note, A1-A2 | kick snare hat perc bass perc |
| A07 | VESPER | 2 | Plaid | 152 | F# minor | half time: an intricate four-bar line, one figure through F#m7 Dmaj7 Bm9 C#7sus4; an FM bass falling into each note, C#1-C#2; busy dusty drums | kick snare hat pad bass lead |
| A08 | MOTEL | v2 | Anthony Naples | 122 | B dorian | (brief v2, to be redone) | kick snare hat chord bass - |
| A09 | PYLONS | 2 | Boards of Canada | 84 | G minor | a deep SWAV-SAW bass walking under Gm9, Ebmaj7, Cm7, its sub on G1 Eb1 C2; a four-note motif, G D C Bb | kick snare hat pad bass lead |
| A10 | RIVETS | 1 | Autechre | 118 | C# minor | the drums lead, a swung broken funk: kick on the off-beats, ghost snares, accented hats, an FM clank; a punchy SWAV-SAW bass in 3 3 4 3 3, its sub on C#1-C#2 | kick snare hat perc bass perc |
| A11 | SYRUP | old | (the bank before) | 96 | A major | a favourite of the owner's, kept | kick snare hat pad lead bass |
| A12 | CUTUP | 1 | Prefuse 73 | 156 | D minor | half time: a swung hip-hop beat cut up, stuttering kick and snare, chopped hats, VO-6 vowel chops, a chopped stab (Dm9, Bbmaj7/D); an FM bass zapping onto each note, D1-D2 | kick snare hat voice bass chord |
| A13 | GRISTLE | 1 | Clark, SOPHIE | 168 | E minor | a slow industrial half-time groove, everything fast on top: a punchy kick, a metal-crack snare with ghost rolls, hats on nearly every step, FM metal boings and pitch-drop zaps; a rubbery FM bass sliding E1-E2 | kick snare hat perc bass perc |
| A14 | ORCHARD | 2 | Boards of Canada | 92 | E dorian | a four-note call, G A B E, answered by the dorian C#; Em7, A7; a driven sine bass, its sub on E1 A1 B1 | kick snare hat pad bass lead |
| A15 | SPLINTR | 1 | Aphex Twin | 160 | F minor | drill breaks with pitched snare retrigs dropping out every second loop, over a huge sustained SWAV-SAW bass, its subs on F1 Ab1 Db1 Eb1 | kick snare hat perc bass lead |
| A16 | FOUNDRY | 2 | Boards of Canada | 102 | G# dorian | after "Kaini Industries": its four bars and their bass notes, G#m7/B C#sus4 G#m9 C#9, under a four-bar line on a deep, gritty SWAV-SAW lead bass, its sub on C#1-D#2; drums that never quite repeat, a shaker and metal percussion in a dotted echo | kick snare hat pad bass perc |

A04 TENFOUR and A16 FOUNDRY are after two tracks the owner chose: Radiohead's "Everything in Its Right Place" and
Boards of Canada's "Kaini Industries". Their chord progressions come from the owner's MIDI files (`reference/midi/`,
kept out of the repository); the lines over them, the rhythms and the sounds are MONO2K's own. Each line was checked
against its file so that no melody follows it: FOUNDRY's shares no two intervals in a row with it, TENFOUR's no more
than two.

After the DIST fix (each machine clipping at its own ceiling, as the Monomachine's do), HAZE, SPLICE and RIBBON were
set to sound as they were approved: a lower DIST on their DIGI-WAV leads, so the clip starts where it did, and the
level lost back on VOL. Each renders within −44 to −50 dB of its approved version. SPLINTR's bass was revoiced an
octave up with SUB2 off, by the sub rule, its subs no longer under 30 Hz, and heard and approved so.

## Detune

Only where it's the point: the tape's wobble in HAZE (TAPE 80), PYLONS (72), ORCHARD (76), TENFOUR (70) and FOUNDRY
(76), about ±10 c, slow, and
HAZE's pad chorus; SPLINTR's bass, a little of SWAV-SAW's unison. Equal temperament everywhere.
