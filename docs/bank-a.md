# Bank A: LO-BIT CLUB

A live set in sixteen parts, after the Elektron-and-handhelds club music of Berlin and Sweden: Dataline's live sets
(immersive, trippy, emotional grooves: minimal techno, breaks, acid, ambient, a Machinedrum and a TB-03 into an
Octatrack), Auvrel's IDM from one digital drum synth (the Division Department 01/IV: punctate, MIDI-sequenced
"micro anthems"), Sqein's dark polymetric FM textures under heavy reverb, sm0hm's Game Boy chiptek and NES FM, and
random_meteor's lo-bit dark synth for the night; its sounds after LØSTLVLS's Monomachine pack (HI: leads, chords,
flutes, strings; LO: basses, drones, growls; DR; FX). Every part is MONO2K's own: the references say what world it is
after, never what it quotes. It replaces the bank before (MAMMOTH .. SPLICE, factory revision 9), whole.

The rules:

- **The drums are the DRM- machines** (DRM-KICK, DRM-SNAR, DRM-HATS, DRM-CYMB, after the LXR's voices): a drum synth's
  palette, tuned, swept, FM'd. GND-PERC, GND-NOIS and FM+ add the clicks, noise and metal.
- **Polymeter where it helps**: a track at 3/4x or 3/2x (its own steps, free over the pattern: 4 against 3, or
  triplets), melodies whose notes cycle over a different count than their trigs, conditions that change a bar a loop.
- **A powerful bass**: roots C1 to C2, mono, locked with the kick; acid lines on one saw through the resonant filter.
- **Melodies are short motifs**, minor or modal, C3 to C5; chords on SWAV-ENS; chip lines on SID-6581 and FM+STAT.
- **Lo-bit, not lo-fi**: ADPCM, bits and rate reduction where they bite, the SPU reverb deep in the dark parts, tape
  only where its wobble is the point.
- The loudness: every part at the bank before's level, its rms over 30 s at -15.1 dBFS through the bank's bus drive
  (`BANK_DRIVE`); `tools/demo_levels.py` balances the tracks by their roles and trims each part to it.

The order is the set's: four rows of four, each a step of the night. Row one opens (warm, minimal), row two peaks
(acid, techno, breaks), row three runs fast (chiptek, NES FM, footwork, chip jungle), row four comes down (half-time,
night, flutes, the last loop).

| Slot | Name | Flavour | Reference | BPM | Key | Idea | Tracks |
|---|---|---|---|---|---|---|---|
| A01 | LUMEN | open | Dataline | 124 | A minor | the set's first groove: a round kick and a clap, hats opening on the off-beat, an off-beat sine bass, a dub stab moving Am7 to Fmaj7 in echoes, and an FM call of five notes over four trigs, so it lands somewhere new each bar | kick snare hat bass chord lead |
| A02 | PULSAR | open | Auvrel | 122 | G minor | a drum synth alone, and a sub: a tight kick, a rim on 1:2, hats in triplets (3/2x), a ride at half speed, and the hook on a second DRM-KICK, a swept FM pluck at 3/4x playing five notes against the bar | kick perc hat perc lead bass |
| A03 | LATTICE | open | Sqein | 120 | D phrygian | dark and polymetric under a long dark reverb: a deep kick in 3 3 2, FM+PAR metal at 3/4x, ticks at 3/2x, a growling FM bass on D1 and E♭1, a phrygian drone, a half-time snare | kick perc hat bass pad snare |
| A04 | SPUDUB | open | Dataline, the PS1's SPU | 118 | C minor | dub techno: a soft deep kick, a shaker, a rim, a Cm7 stab thrown into a ping-pong echo under the SPU reverb, its filter breathing, a sparse sub | kick perc snare chord bass hat |
| A05 | PROCESS | peak | 0F.digital, nanoloop | 126 | A dorian | process techno, every track on its own clock: sine bloops in triplets, five notes over six trigs; an FM tick at 3/4x by chance; noise in 32nds, mostly by chance; a kick on the beat; open hats; a bass jumping the octave | kick perc perc hat hat bass |
| A06 | SQUELCH | peak | Dataline, the TB-03 | 128 | E minor | acid: a SID saw through the resonant lowpass, sliding and accented, each accent's filter thrown open, its cutoff drifting over the bars; a 909-ish kick on E1, hats opening off the beat, a clap, a ride, a rim | kick bass hat snare perc perc |
| A07 | GROWL | peak | LØSTLVLS's LO | 130 | F minor | techno with a growl: an FM+DYN growl on the off-beats, F2 to C3, its depth thrown by a random LFO, a sine sub an octave under it; a rumbling kick; driving hats; a snare rolling into the turn; a noise riser every four bars | kick chord hat snare perc bass |
| A08 | BREAKS | peak | Dataline | 132 | E minor | breaks: a kick off the grid, a snare with ghosts, hats by chance; a SID reese synced to a swept second oscillator; a hoover stab diving B3 to E3; an Em7 pad | kick snare hat bass lead pad |
| A09 | SPRITES | fast | sm0hm's Game Boy chiptek | 136 | F# minor | chiptek: a SID pulse bass pumping the octave, F#m D E C#m; a square arpeggio of each chord in sixteenths, three notes over sixteen steps; crushed noise hats; a clap rolling into the turn; a pulse lead's five-note hook in echoes | kick bass chord hat snare lead |
| A10 | OPLL | fast | sm0hm's NES FM | 128 | B minor | the console with an FM chip: the noise channel as hats and a snare, the triangle's bass bouncing octaves (Bm G D A), a two-operator FM lead sliding with a vibrato coming in, the 12.5 % pulse in arpeggios, a crash every four bars | kick hat bass lead chord perc |
| A11 | JUKE | fast | foodman, suesett | 160 | F minor | footwork: a long 808 kick in threes against the bar, a clap, rapid hats ratcheting by chance, VO-6 chopping a vowel in threes (another vowel each hit), an 808 sub dropping into F1 D♭1 E♭1, a SID stab answering | kick snare hat voice bass lead |
| A12 | CHIPJNG | fast | KUF's chip jungle | 172 | A minor | chip jungle: a kick and a snare cut as a break with ghosts and rolls; a dark held SID saw bass, A1 F1 G1 E1, sliding; a square lead in eighths, echoing; a ride; hats by chance | kick snare perc bass lead hat |
| A13 | VIOLET | down | L-tron | 112 | G minor | half time in slow motion: two squares harmonising a bar a chord (D5 over B♭4, B♭4 over G4, C5 over A4, A4 over F4), the tape slowing them; a soft kick, a snare on three deep in the reverb, swung hats; a sub on G1 E♭1 F1 D1 | kick snare hat lead chord bass |
| A14 | DUSKLIT | down | random_meteor | 100 | C minor | lo-bit night, swung: a crushed snare, 8-bit noise hats, a dark reed-like lead on one of the owner's waves (C4 E♭4 G4 F4 E♭4 D4) in a dotted echo, a chorused Cm7 to A♭maj7, a soft SID bass sliding C2 A♭1 B♭1 G1 | kick snare hat lead pad bass |
| A15 | FLUTE | down | LØSTLVLS's HI | 96 | D dorian | swung hip-hop: a boom-bap kick and snare, hats with ghosts; a breathy sine flute with a vibrato singing D5 C5 A4 F4 G4 in a long echo; strings swelling Dm7 to G7; a sub on D2 G1 A1 C2 | kick snare hat lead pad bass |
| A16 | EMBERS | down | Sqein, Dataline | 116 | E minor | the last loop: a soft kick thinning out by its conditions, ticks at 3/4x by chance, FM bells (B4 G4 E4 F#4) deep in a long dark reverb, an Em7 drone, a sub now and then, a near-sine motif (B3 E4 G4) in an echo that won't stop | kick hat lead pad bass lead |
