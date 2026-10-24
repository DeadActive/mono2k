# Composing bank A

How the eight demo parts are written: the owner's verdicts so far, then the rules taken from research on Elektron
pattern craft, early-2000s IDM harmony and mixing a small groovebox. Each rule names where it came from; *inferred*
marks a rule drawn from the sources rather than stated by one.

## What the owner has said

- The sounds come from the sound sheets the owner picked (`tools/sheet/`): sheet 1's drums and textures, sheet 3's
  pads, stabs, keys and leads, sheet 4's progressions ("perfect now"). Nothing is hand-designed from habit.
- Too noisy is the failure: crushed basses, a snare crushed by chance, echoes piling up. Simple grooves win: a kick on
  the beat, a snare on 2 and 4, the bass on the off-beats (parts 2B and 4B, "good direction").
- No pitch-dropping "laser" percussion. Every track on the tempo, every pitched sound in the harmony.
- Bad: chord progressions drawn at random from a scale; dense random rhythms layered; chip squares and arpeggios.

## Pattern

- Four or five voices at once at most, each in its own register; fix clutter by removing a part, not by mixing it.
  ([Elektronauts, busy tracks](https://www.elektronauts.com/t/dealing-with-busy-tracks-multiple-similar-parts/52805),
  [Attack, dusted deep house](https://www.attackmagazine.com/technique/beat-dissected/dusted-deep-house/): five elements
  is a full beat)
- Conditions give structure, not dice: A/B phrases on 1:2 and 2:2 so four bars read as eight, a fill on the last pass
  (4:4); chance only on quiet ghost notes.
  ([Elektronauts, conditional trigs](https://www.elektronauts.com/t/how-do-you-use-conditional-triggers/42272))
- Locks are events (an open hat, a fill, one stab with more echo); continuous motion comes from an LFO, never from a
  lock on every step. ([Elektronauts](https://www.elektronauts.com/t/machinedrum-style-p-lock-live-recording-on-any-other-machines/129993))
- One hat track plays closed and open: short and quiet, a longer decay and more level locked on the open hits; the hat
  drops out where the snare lands; accents on the off-beats.
  ([Elektronauts, hats](https://www.elektronauts.com/t/share-your-hi-hat-tricks-all-types-of-house-and-techno-music/87031),
  [16th hats](https://elektronauts.com/t/16th-notes-hihat-programming/6868))
- Swing small: 52-58 % loosens without sounding swung; the kick stays on the grid.
  ([Attack, Roger Linn](https://www.attackmagazine.com/?p=10694))
- At most one track on an odd length or speed, and only when it lines up again within the pattern.

## Low end

- The kick on the grid, tuned to the root, dry and centred. The bass never on a kick step (off-beats, or between the
  kicks), dry, centred, harmonic, filtered. ([Attack, rolling techno](https://www.attackmagazine.com/technique/beat-dissected/rolling-techno/),
  [Attack, bass styles](https://www.attackmagazine.com/technique/tutorials/low-end-theory-exploring-eight-common-bassline-styles/))
- Mono under 120 Hz; kick and bass apart in register (the kick's weight near 50-60 Hz, the bass's around 40 or 80).
  ([Gearnews](https://www.gearnews.com/kick-and-bass-workshop-studio/))

## Harmony

- One chord a bar or two; two-chord vamps are enough. m7, m9, maj7, add9, sus2 and bare fifths, not plain triads; the
  7th or 9th on top so it is heard. No V-I cadence: minor v, chains of m7, VI and VII, the dorian IV.
  ([Reverb Machine, Boards of Canada chords 1](https://reverbmachine.com/blog/boards-of-canada-chord-theory-part-one/),
  [2](https://reverbmachine.com/blog/boards-of-canada-chord-theory-part-two/))
- Planing: one chord shape moved whole to other roots (sus2, add9, m7). A tonic pedal under moving chords. A bass
  stepping down under fixed chords.
- Voicings between C3 and C5, roots and fifths only in the low register.
- Shapes: VI5 iv5 i5 III5 (fifths); v7 iii7 iv7 i7; i (second inversion) and VI add9 two bars each; Isus2 bVIsus2
  IVsus2; i bV add9 vii7 iv7sus4.

## Melody

- Rhythm first: write it on one note, then give it two or three pitches. Motifs of three or four notes; repeat, change
  one thing (the last note). ([Elektronauts, melodies](https://www.elektronauts.com/t/writing-better-melodies-and-harmonies-on-elektron-machines/139474?page=3))
- A range of about a tenth around middle C; long notes on chord tones; a leap followed by a step back.
- An arpeggio needs its own melody, rests, mixed gate lengths and a moving tone, or it sounds like a toy.
  ([Audiotent](https://www.audiotent.com/blogs/production-tips/keep-arpeggios-interesting))
- Toy sounds come from high, short, bright notes on squares, V-I cadences, close triads in the treble, strict up-down
  16th arpeggios and a chord a beat. *Inferred.*

## Mix

- One crushed or driven element per part at most: crush and rate reduction turn decaying tails into hiss, and the
  2-5 kHz pile-up is what tires the ear. ([Wikipedia, bit depth](https://en.wikipedia.org/wiki/Audio_bit_depth),
  [Yamaha](https://ca.yamaha.com/en/files/HoW_Tips_and_Tricks_Guide_2020_628aa6d801d56e8ec692ffe8e7339b3f.pdf))
- Echo on one or two tracks (hats, one stab), its feedback under half, its time on the tempo (a dotted 8th for dub
  chords). ([Tarekith, Monomachine tips](https://tarekith.com/assets/monomachine_tipsandtricks.htm),
  [Attack, dub chords](https://www.attackmagazine.com/technique/synth-secrets/dub-techno-synth-chords/))
- Reverb: one space, less than it seems; none on the kick or the bass.
  ([Attack, reverb](https://attackmagazine.com/technique/tutorials/how-to-use-reverb-in-a-mix/))
- Balance by register and decay first ([Model:Cycles Q&A with its designer](https://www.elektronauts.com/t/model-cycles-q-a-with-ess/122712?page=8));
  the bus drive only rounds the kick's peaks. Each part at the bank's loudness (`tools/demo_levels.py`).
