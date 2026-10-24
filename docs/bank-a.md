# Bank A

Four parts, A01 to A04; A05 to A16 are blank. It replaces LO-BIT CLUB (factory revision 10) whole: a store saved
with LO-BIT CLUB's parts takes these where those still stand, and the slots after A04 go blank the same way.

## How they were made

Nothing here was written from habit. The owner rejected three banks written that way ("cheesy, plastic, toy";
"random sounds mixed that don't gel"), so these were made in steps, each one picked by ear:

1. **Sound sheets** (`tools/sheet/`). Sheet 1: 6000 random settings across the machines, the silent, clipped and static
   ones thrown out, 24 picked for variety; the owner kept them all. Sheet 3: instruments (pads, stabs, keys, leads)
   drawn in musical ranges and kept by how much of their energy sits on the notes they play. Sheet 4: those
   instruments on idiomatic minor-key progressions ("perfect now").
2. **Research** (`docs/composing.md`): Elektron pattern craft, early-2000s IDM harmony and melody, mixing a small
   groovebox. The rules every part follows are there.
3. **Drafts**, eight then five, heard and judged one by one. Two survived (CIRCUIT and SUSPEND); the other two were
   written from their sounds only, since those had already worked together.
4. **Fixes by measurement**, each from something the owner heard: pitch-dropping percussion ("a laser", "a sword"),
   a snare ringing a pitched buzz from hit to hit, a bass that was a thump without a note, chord hits filtered above
   their own chord, echoes off the tempo, drums off the key. `tools/sheet/fit.py` measures how much of each track sits
   on the bar's chord; spectrograms showed the rings and sweeps.

Each part is a 4-bar pattern that plays as a 16-bar form: what enters after the first pass (`!1ST`), what alternates
(1:2, 2:2), what comes on the third and fourth passes (3:4, 4:4), LFOs over 16 bars. The kick is on the grid and on the
root, every echo and LFO on the tempo, every pitched track in the harmony. Each part sits at -15.1 dBFS rms (the
bank's loudness, through `BANK_DRIVE`), each track levelled alone first by its role in the part.

The factory's code for each part is written by `tools/sheet/mel.c` (`cdump`) from the part's score in
`tools/sheet/four/`, so the firmware plays what the owner heard; the host's render of the bank matches those renders
pass by pass.

## The parts

| Slot | Name | BPM | Key | What it is | Tracks |
|---|---|---|---|---|---|
| A01 | CIRCUIT | 126 | A minor | electro: a broken kick, a sine bass whose line moves through each chord and steps into the next, chord hits in the gaps the kick and bass leave, FM keys whose arpeggio is its own melody on alternate passes; the snare and hats in from the second pass | DRM-KICK, DRM-SNAR, DRM-HATS, GND-SIN, SWAV-ENS, FM+STAT |
| A02 | TIDE | 122 | E dorian | Em7 and A9 on the ENS pad, two bars each; an FM bass off the beat; FM keys answering, ending two ways; the snare from the second pass, the hats on the third and fourth | SWAV-ENS, DRM-KICK, DRM-SNAR, FM+DYN, FM+STAT, DRM-HATS |
| A03 | SUSPEND | 110 | D | sus2 chords moved whole (Dsus2 Bbsus2 Gsus2), their 3rd coming in every other pass; a sparse kick, a sine rim, a sine sub walking down at the end; FM keys on each chord's 2nd, 5th and 6th | SWAV-ENS, DRM-KICK, GND-SIN, GND-SIN, FM+STAT, DRM-CYMB |
| A04 | LANTERN | 100 | A | add9 chord hits moved whole (Aadd9 Fadd9 Cadd9 Gadd9), swung; FM keys on each chord's 3rd, 5th and 9th; a kick, a sine rim, a sine sub; the cymbal from the third pass | SWAV-ENS, DRM-KICK, GND-SIN, GND-SIN, FM+STAT, DRM-CYMB |
