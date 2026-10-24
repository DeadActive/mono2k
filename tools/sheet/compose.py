#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Bank A's eight parts, written to docs/composing.md: the owner's picked sounds only (sheet 1's drums cleaned, sheet 3's
# instruments), simple grooves (the kick on the grid and on the root, the bass off the kick, the snare on 2 and 4, one
# hat track closed and open), a chord a bar or two voiced with the colour on top, motifs written rhythm first, A/B
# phrases on 1:2 and 2:2 and a fill on the last pass, one crushed element at most, everything on the tempo and in key.
#   compose.py MEL OUTDIR [PART..]
import sys, os, wave, subprocess, itertools, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from loud import lufs                                        # noqa: E402
MEL, OUT = sys.argv[1], sys.argv[2]
ONLY = set(sys.argv[3:])
os.makedirs(OUT, exist_ok=True)
NN = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"]
PC = {n: i for i, n in enumerate(NN)}; PC.update({"C#": 1, "D#": 3, "F#": 6, "G#": 8, "A#": 10})
C_P75, C_P50, C_1_2, C_2_2, C_1_4, C_4_4 = 1, 2, 6, 7, 11, 14
C_2_4, C_3_4, C_1ST, C_N1ST = 12, 13, 15, 16
P_SH, P_VOL, P_GATE, P_DSND, P_LEVL = 7, 13, 41, 28, 37

def pitch(name):
    """'F#4' -> 66"""
    n, o = (name[:-1], int(name[-1]))
    return PC[n] + 12 * (o + 1)

# the sounds: sheet 1's (their kit, cleaned as the owner asked: no crush, centred low end, no laser) and sheet 3's
SOUNDS = {
    "kick":    ("orig 3203", "14=64 69=0 35=0"),                                  # 1 #20, dry, centred
    "snare":   ("orig 1851", "69=40 2=40 14=64 32=0"),                            # 1 #23, its tone's sweep short
    "hats":    ("orig 2324", "12=76 19=60 6=40 7=0"),                             # 1 #19 A, closed on a short decay
    "cym24":   ("orig 4425", "35=40 18=55 33=30"),                                # 1 #24 A
    "cym18":   ("orig 1933", "17=60 25=64 28=85"),                                # 1 #18 A
    "fmhit":   ("orig 753",  "26=0 32=0 2=61 4=83 28=90 33=20"),                  # 1 #17, harmonic, unaliased
    "sperc":   ("orig 1542", "3=5 2=15"),                                         # 1 #11, a sine perc: no drop, little noise
    "fmbass":  ("orig 5398", "32=0 26=0 69=0 14=64 0=96 4=64 1=64 17=46 19=20"),              # 1 #2, harmonic, dry
    "sawbass": ("orig 3489", "32=0 26=0 12=84 69=0 14=64 17=30 3=10"),            # 1 #3, uncrushed, dark, steady
    "enshit":  ("orig 3419", "25=64 32=0 18=20"),                                 # 1 #9 A, the chord hits
    "enshit2": ("orig 3419", "25=64 32=0 18=20 16=12 17=50"),                     # the same, its highpass under the notes
    "fmbass2": ("orig 5398", "32=0 26=0 69=0 14=64 0=64 4=64 1=64 2=60 6=20 17=30 19=20"),  # 1 #2 clean: 1:1, less depth and feedback, its first harmonics
    "stab10":  ("mtrack 1 1056", ""),                                             # 3 #10, the stab the owner liked
    "stab12":  ("mtrack 1 1070", "26=0 17=32 23=76 19=18"),                                         # 3 #12, its rate reduction off
    "pad2":    ("mtrack 0 30", ""),                                               # 3 #2, the ENS pad
    "pad5":    ("mtrack 0 29", ""),                                               # 3 #5, FM+STAT pad
    "lead22":  ("mtrack 3 3111", ""),                                             # 3 #22, the pulse lead
    "lead23":  ("mtrack 3 3092", ""),                                             # 3 #23, the saw lead
    "keys18":  ("mtrack 2 2073", "26=0"),                                         # 3 #18, FM+STAT keys
    "keys14":  ("mtrack 2 2055", ""),                                             # 3 #14, FM+PAR keys
    "keys19":  ("mtrack 2 2058", ""),                                             # 3 #19, FM+STAT keys
    "sub":     ("ptrack 0", ""),                                                  # a sine
    "sinebass": ("ptrack 0", "0=18 6=50 8=0 9=10 10=60 11=30 41=30"),             # a sine bass: a little fold, its 2nd harmonic
    # rebuilt after the owner heard "a repetitive sound, maybe some percussion": 1 #23 rang a harmonic stack from hit to
    # hit (its square tone, its clap's repeats), 1 #19 pinged a metal tone at 3-4 kHz. Now mostly noise, short, a sine
    # body under the snare and some room; the hats a noise source high-passed, no drive
    "snare2":  ("orig 1851", "0=16 1=30 2=20 3=110 4=90 5=70 6=0 7=6 8=0 9=0 10=40 11=20 12=64 14=64 16=8 17=70 35=40 69=0 32=0"),
    "hats2":   ("orig 2324", "5=117 2=20 4=100 6=18 7=0 12=64 16=54 17=60 19=0 69=0 28=64 32=0"),
    # then "a percussion like a sword attack": the snare's sine body still swept down (a small zing) under a long noise
    # in reverb, the open hat rang out each bar. Now no sweep, a shorter darker noise, less room; the hats shorter, darker
    "snare3":  ("orig 1851", "0=16 1=0 2=0 3=100 4=80 5=64 6=0 7=6 8=0 9=0 10=28 11=14 12=64 14=64 16=8 17=52 35=24 69=0 32=0 22=64 23=64"),   # no filter envelope: its sweep was the swish
    "hats3":   ("orig 2324", "5=117 2=10 4=100 6=12 7=0 12=64 16=50 17=40 19=0 69=0 28=64 32=0"),
}

class Part:
    def __init__(self, name, bpm, key, swing, desc):
        self.name, self.bpm, self.key, self.desc = name, bpm, key, desc
        self.L = [f"glob 1 {round((swing - 50) * 127 / 25)}"]
        self.tgt, self.who = {}, {}
        self.bars = 16
    def lfo(self, t, slot, dest, spd, depth, wave=0, mult=0):
        """an LFO on track t: dest a parameter, a triangle (0) or sine (1), its cycle 256 / (spd x 2^mult) beats"""
        b = 42 + 8 * slot
        for o, v in ((1, dest + 1), (2, (wave * 128 + 64) // 7), (3, 12), (4, (mult * 128 + 64) // 8), (5, spd), (6, 0), (7, 64 + depth)):
            self.L.append(f"kit {t} {b + o} {v}")
    def track(self, t, sound, target, *edits):
        head, ed = SOUNDS[sound]
        w = head.split()
        if w[0] == "ptrack":
            self.L.append(f"ptrack {t} {w[1]}")
            for e in f"{ed} {' '.join(edits)}".split():
                a, b = e.split("="); self.L.append(f"kit {t} {a} {b}")
        else:
            self.L.append(" ".join(f"{w[0]} {t} {' '.join(w[1:])} {ed} {' '.join(edits)}".split()))
        if w[0] != "ptrack": self.L.append(f"clear {t}")
        self.tgt[t] = target; self.who[t] = sound
    def kit(self, t, p, v): self.L.append(f"kit {t} {p} {v}")
    def hit(self, t, s, note, cond=0, acc=False, slide=False, locks=()):
        fl = ("a" if acc else "") + ("s" if slide else "")
        self.L.append(f"trig {t} {s} {note} {fl}".rstrip())
        if cond: self.L.append(f"cond {t} {s} {cond}")
        for p, v in locks: self.L.append(f"lock {t} {s} {p} {v}")
    def grid(self, t, cells, note, bars=range(4), cond=0, acc=(), locks=None):
        """cells: one 16-step string for every bar ('x' hit, 'X' accent, 'o' open hat, 'g' ghost by chance)"""
        for b in bars:
            for i, c in enumerate(cells):
                if c == ".": continue
                s = 16 * b + i
                lk = list((locks or {}).get(c, ()))
                cd = C_P50 if c == "g" else cond
                self.hit(t, s, note, cd, acc=c in "Xo" or i in acc, locks=lk)
    def chord(self, t, s, voicing, cond=0, locks=()):
        """SWAV-ENS: the voicing's base note on the trig, the other three as intervals"""
        b = voicing[1]; iv = [p - b for p in voicing if p != b]
        self.hit(t, s, b, cond, locks=locks)
        self.L.append(f"ens {t} {s} {iv[0]} {iv[1]} {iv[2]}")

# chords: tones over the root, the colour last (it goes on top when it can)
Q = {"m7": [0, 3, 7, 10], "m9": [0, 3, 10, 14], "maj7": [0, 4, 7, 11], "maj9": [0, 4, 11, 14], "add9": [0, 4, 7, 14],
     "madd9": [0, 3, 7, 14], "sus2": [0, 7, 12, 14], "5": [0, 7, 12, 19], "6": [0, 4, 7, 9], "m11": [0, 3, 10, 17],
     "7sus4": [0, 5, 7, 10], "6/9": [0, 4, 9, 14]}

def voice(root, q, prev=None, lo=50, hi=75, n=4):
    """an ENS voicing: n notes, within +-12 of its base (the second lowest), the colour on top, near the last chord"""
    tones = Q[q][:n]; colour = (root + tones[-1]) % 12
    opts = [[p for p in range(lo, hi + 1) if p % 12 == (root + iv) % 12] for iv in tones]
    best = None
    for c in itertools.product(*opts):
        v = sorted(c)
        if len(set(v)) < n or v[-1] - v[0] > 24 or any(abs(p - v[1]) > 12 for p in v): continue
        if any(p < 52 and (p - root) % 12 not in (0, 7) for p in v): continue      # only roots and fifths down low
        score = 0 if v[-1] % 12 == colour else 4
        score += sum(abs(a - b) for a, b in zip(v, prev)) / 2 if prev else abs(sum(v) / n - 62) / 2
        if best is None or score < best[0]: best = (score, v)
    return best[1]

def prog(part, t, chords, rhythm, lo=50, hi=75, cond_last=None, locks=None):
    """chords: [(root name, quality, bars)]; rhythm: a 16-step string per bar for the chord's hits"""
    bar, prev, out = 0, None, []
    for root, q, bars in chords:
        v = voice(PC[root], q, prev, lo, hi); prev = v; out.append(v)
        for b in range(bars):
            for i, c in enumerate(rhythm):
                if c == ".": continue
                lk = list((locks or {}).get(c, ()))
                cd = C_1_2 if c == "1" else C_2_2 if c == "2" else 0
                part.chord(t, 16 * (bar + b) + i, v, cd, lk)
        bar += bars
    return out

def bassline(part, t, chords, cells, octave_lo, slide_steps=(), conds=None):
    """the chord's root on each hit of the cell, in the register from octave_lo up"""
    bar = 0
    for root, q, bars in chords:
        n = octave_lo + (PC[root] - octave_lo) % 12
        for b in range(bars):
            for i, c in enumerate(cells):
                if c == ".": continue
                s = 16 * (bar + b) + i
                nn = n + 12 if c == "8" else n
                part.hit(t, s, nn, (conds or {}).get(s, 0), slide=i in slide_steps)
        bar += bars

def line(part, t, notes, slides=()):
    """notes: {step: (name, cond)}"""
    for s, v in sorted(notes.items()):
        name, cd = v if isinstance(v, tuple) else (v, 0)
        part.hit(t, s, pitch(name), cd, slide=s in slides)

PARTS = []

# 1. DUB: the stab the owner liked on a dub vamp, a dotted-8th echo thrown wider on one hit; F minor, 118
p = Part("dub", 118, "F", 54, "F minor, 118: the stab you liked (3 #10) on Fm9 and Dbmaj9 in a dotted-8th echo, kick, "
         "clap, hats closed and open, a sine sub between the kicks, a sine perc answering on alternate loops")
F1 = [("F", "m9", 2), ("Db", "maj9", 2)]
p.track(0, "stab10", -19, "27=48 28=100 29=56")              # the echo: a dotted 8th, feedback under half
p.L += ["kit 0 51 18", "kit 0 52 0", "kit 0 53 12", "kit 0 54 8", "kit 0 55 8", "kit 0 56 0", "kit 0 57 84"]  # LFO2: the filter, 8 bars
vs = prog(p, 0, F1, "..x....x..x.....")
p.chord(0, 62, vs[-1], C_4_4)                                # the last pass: one more stab
p.L.append("lock 0 42 28 116")                               # one stab thrown into the echo
p.track(1, "kick", -16.5); p.grid(1, "x...x...x...x...", 41)
p.track(2, "snare", -19, "35=50"); p.grid(2, "....x.......x...", 52)
p.track(3, "hats", -25); p.grid(3, "..o....x..o....x", 53, locks={"o": [(P_SH, 60)], "x": [(P_VOL, 44)]})
p.track(4, "sub", -16, "8=6 9=127 10=127 11=50 41=60")
bassline(p, 4, F1, "...x......x.....", 29)
p.track(5, "sperc", -25)
for b in range(4):
    p.hit(5, 16 * b + 13, pitch("C5"), C_1_2); p.hit(5, 16 * b + 7, pitch("Ab4"), C_2_2)
PARTS.append(p)

# 2. MERIDIAN: part 2B grown: the ENS pad's Em7 Cmaj7 Gmaj7 Dadd9, a written lead; E minor, 120
p = Part("meridian", 120, "E", 54, "E minor, 120: part 2B grown: the ENS pad (3 #2) on Em7 Cmaj7 Gmaj7 Dadd9, kick, "
         "snare, the FM bass on the off-beats, the pulse lead's descending motif ending two ways, the cymbal")
E2 = [("E", "m7", 1), ("C", "maj7", 1), ("G", "maj7", 1), ("D", "add9", 1)]
p.track(0, "pad2", -20); prog(p, 0, E2, "x...............")
p.track(1, "kick", -16.5); p.grid(1, "x...x...x...x...", 40)
p.track(2, "snare", -19); p.grid(2, "....x.......x...", 59); p.hit(2, 63, 59, C_4_4, locks=[(P_VOL, 46)])
p.track(3, "fmbass", -16.5); bassline(p, 3, E2, "..x...x...x...x.", 28)
p.track(4, "lead22", -19)
line(p, 4, {0: "B4", 6: "A4", 10: "G4", 16: "B4", 22: "G4", 26: "E4", 32: "D5", 38: "B4", 42: "F#4",
            48: "E4", 54: "F#4", 58: ("A4", C_1_2), 59: ("D4", C_2_2)})
p.track(5, "cym24", -28); p.grid(5, "..x...x...x...x.", 59, acc=(2, 10))
p.hit(5, 46, 59, C_4_4, locks=[(P_SH, 90)])
PARTS.append(p)

# 3. GRANITE: part 4B grown: Cm9 Gm7 stabs answering an off-beat saw bass; G minor, 128
p = Part("granite", 128, "G", 52, "G minor, 128: part 4B grown: the ENS stab (3 #12) on Cm9 and Gm7, kick, hats closed "
         "and open, the snare on alternate loops at first, the saw bass off the kick, quiet FM metal")
G3 = [("C", "m9", 2), ("G", "m7", 2)]
p.track(0, "stab12", -19); prog(p, 0, G3, "...x..x....x..1.")
p.track(1, "kick", -16.5); p.grid(1, "x...x...x...x...", 43)
p.track(2, "hats", -28); p.grid(2, "..x...x...x...o.", 43, locks={"o": [(P_SH, 70)]})
p.track(3, "snare", -19); p.grid(3, "....x.......x...", 54, bars=range(2), cond=C_2_2); p.grid(3, "....x.......x...", 54, bars=range(2, 4))
p.track(4, "sawbass", -16.5, f"{P_GATE}=40"); bassline(p, 4, G3, "..x.......x.....", 31)
p.track(5, "fmhit", -26); p.grid(5, "...x...x...x..x.", 62, cond=C_P75)
PARTS.append(p)

# 4. HALFLIGHT: bare fifths VI5 iv5 i5 III5 on two FM pads, a gliding bass on the roots, a pentatonic lead with a
# small tape wobble; D minor, 96, swung
p = Part("halflight", 96, "D", 58, "D minor, 96: bare fifths Bb5 G5 D5 F5 on the FM+STAT pad (3 #5) twice, kick and "
         "snare with a ghost, the saw bass gliding on the roots, the saw lead in D minor pentatonic with a slight wobble")
D4 = [("Bb", 52), ("G", 50), ("D", 50), ("F", 53)]
p.track(0, "pad5", -21, "14=50"); p.track(1, "pad5", -21, "14=78")
for b, (r, lo) in enumerate(D4):
    root = lo + (PC[r] - lo) % 12
    p.hit(0, 16 * b, root); p.hit(1, 16 * b, root + 7)
p.track(2, "kick", -16.5); p.grid(2, "x......x..x.....", 38)
p.track(3, "snare", -19); p.grid(3, "....x.......x..g", 49, locks={"g": [(P_VOL, 40)]})
p.track(4, "sawbass", -16.5, "15=25", f"{P_GATE}=110")
for b, (r, _) in enumerate(D4):
    n = 29 + (PC[r] - 29) % 12
    p.hit(4, 16 * b, n); p.hit(4, 16 * b + 10, n + 7, slide=True)
p.hit(4, 62, 29 + (PC["E"] - 29) % 12, C_1_2, slide=True)
p.track(5, "lead23", -19, "17=30", "19=15", "51=8", "52=27", "53=12", "54=24", "55=64", "56=0", "57=66")   # LFO2: a slow wobble on TUNE
line(p, 5, {0: "A4", 8: "G4", 10: "F4", 16: "A4", 24: "G4", 26: "D4", 32: "A4", 40: "G4", 42: "F4",
            48: "C5", 56: "A4", 58: ("G4", C_1_2), 59: ("F4", C_2_2)})
PARTS.append(p)

# 5. CIRCUIT: electro: a broken kick, the FM bass syncopated with an octave, chord hits answering, keys whose
# arpeggio is its own melody, every other loop; A minor, 126
p = Part("circuit", 126, "A", 52, "A minor, 126: electro: kick, snare, hats with ghosts, the FM bass syncopated with "
         "an octave, ENS chord hits (1 #9) on Am9 Fmaj7 G6, FM+STAT keys (3 #18) on alternate loops")
A5 = [("A", "m9", 2), ("F", "maj7", 1), ("G", "6", 1)]
p.track(0, "kick", -16.5); p.grid(0, "x......x..x.....", 33); p.hit(0, 61, 33, C_4_4)
p.track(1, "snare", -19); p.grid(1, "....x.......x...", 47)
p.track(2, "hats", -27); p.grid(2, "x.xg..x.x.xg..o.", 57, locks={"g": [(P_VOL, 36)], "o": [(P_SH, 50)]})
p.track(3, "fmbass", -16.5); bassline(p, 3, A5, "...x..8..x....x.", 33)
p.track(4, "enshit", -20); prog(p, 4, A5, "..........x.....")
p.track(5, "keys18", -20)
bar = 0; prev = None
for r, q, bars in A5:
    v = voice(PC[r], q, prev); prev = v
    for b in range(bars):
        for i, k in zip([0, 3, 7, 10], [1, 3, 2, 0]):
            p.hit(5, 16 * (bar + b) + i, v[k] if v[k] >= 60 else v[k] + 12, C_2_2)
    bar += bars
PARTS.append(p)

# 6. VELVET: deep house: Cm9 Fm9 stabs on the first off-beat, ghost hats under swing, a bass between the kicks,
# a three-note keys motif ending two ways; C minor, 122
p = Part("velvet", 122, "C", 56, "C minor, 122: deep house: the stab (3 #10) on Cm9 and Fm9 on the first off-beat, kick, "
         "clap, ghost hats, a sine bass between the kicks, FM+PAR keys (3 #14) answering")
C6 = [("C", "m9", 2), ("F", "m9", 2)]
p.track(0, "stab10", -19, "10=50", "69=40"); prog(p, 0, C6, "..x.......x.....")
p.track(1, "kick", -16.5); p.grid(1, "x...x...x...x...", 36)
p.track(2, "snare", -19, "35=40"); p.grid(2, "....x.......x...", 47)
p.track(3, "hats", -25); p.grid(3, ".gxg.gxg.gxg.gxo", 48, locks={"g": [(P_VOL, 34)], "o": [(P_SH, 60)]})
p.track(4, "sub", -16, "8=4 9=20 10=60 11=40 41=40"); bassline(p, 4, C6, "..x...x...x...x.", 36)
p.track(5, "keys14", -20)
line(p, 5, {3: "G4", 11: "Bb4", 13: "C5", 35: "Ab4", 43: "C5", 45: ("Eb5", C_1_2), 46: ("D5", C_2_2)})
PARTS.append(p)

# 7. SUSPENSION: sus2 chords planed (I bVI IV), a three-note motif moving with them; D, 110
p = Part("suspension", 110, "D", 54, "D, 110: sus2 chords moved whole (Dsus2 Bbsus2 Gsus2) on the ENS pad (3 #2), a "
         "sparse kick, a sine rim, a sub held, FM+STAT keys (3 #19) on each chord's 2nd, 5th and 6th")
D7 = [("D", "sus2", 1), ("Bb", "sus2", 1), ("G", "sus2", 2)]
p.track(0, "pad2", -20, "8=20", "17=42"); prog(p, 0, D7, "x...............")
p.track(1, "kick", -16.5); p.grid(1, "x.........x.....", 38)
p.track(2, "sperc", -21); p.grid(2, "....x.......x...", 69)
p.track(3, "sub", -16, "8=10 9=127 10=127 11=70 41=127"); bassline(p, 3, D7, "x...............", 26)
p.track(4, "keys19", -20)
bar = 0
for r, q, bars in D7:
    root = 57 + (PC[r] - 57) % 12
    for b in range(bars):
        s0 = 16 * (bar + b)
        for i, d in zip([2, 6, 9], [2, 7, 9]):
            p.hit(4, s0 + i, root + d)
    bar += bars
p.hit(4, 62, 76, C_1_2)
p.track(5, "cym24", -29); p.grid(5, "..x...x...x...x.", 57)
PARTS.append(p)

# 8. FERRUM: metallic: m7 chords planed i III iv VI, FM metal and a cymbal, the saw bass; B minor, 136
p = Part("ferrum", 136, "B", 52, "B minor, 136: metallic: the ENS stab (3 #12) on m7 chords moved whole (Bm7 Dm7 Em7 "
         "Gm7), kick, snare, FM metal hits, the cymbal off the beat, the saw bass off the kick")
B8 = [("B", "m7", 1), ("D", "m7", 1), ("E", "m7", 1), ("G", "m7", 1)]
p.track(0, "stab12", -20); prog(p, 0, B8, "x.....x.........")
p.track(1, "kick", -16.5); p.grid(1, "x...x...x...x...", 35)
p.track(2, "snare", -19); p.grid(2, "....x.......x...", 59); p.hit(2, 62, 59, C_4_4); p.hit(2, 63, 59, C_4_4)
p.track(3, "fmhit", -25); p.grid(3, "x..x..x...x..x..", 66, acc=(0,))
p.track(4, "cym18", -29); p.grid(4, "..x...x...x...x.", 59)
p.track(5, "sawbass", -16.5, f"{P_GATE}=30"); bassline(p, 5, B8, "..x...x...x...x.", 35)
PARTS.append(p)


# ------------------------------------------------------------------ the four: 5 and 7 evolved, three from their world
# The owner: "the only ones I like are 5 and 7", "but still to evolve"; the others "no cuajan": random sounds mixed.
# So every sound below is one of 5's or 7's (kick, snare, hats, the FM bass, the ENS chord hits and pad, the FM+STAT
# keys, the sine rim and sub, the cymbal), and each pattern runs a 16-bar form in its 4 bars: what enters after the
# first pass (!1ST), what alternates (1:2 2:2), what comes on the 3rd and 4th (3:4 4:4), an LFO over 16 bars.

# 5 evolved: CIRCUIT
p = Part("circuit_e", 126, "A", 52, "5 evolved: A minor, 126: the kick and FM bass from the top, chord hits on step 11 "
         "then a second on the 4th pass, the hats in from the 2nd pass, the keys on the 2nd and 4th with a sparser "
         "answer on the 3rd, the chord filter opening over 16 bars")
p.bars = 20
A5 = [("A", "m9", 2), ("F", "maj7", 1), ("G", "6", 1)]
p.track(0, "kick", -16.5); p.grid(0, "x......x..x.....", 33); p.hit(0, 61, 33, C_4_4)
p.track(1, "snare", -19); p.grid(1, "....x.......x...", 47, cond=C_N1ST); p.hit(1, 31, 47, C_2_2, locks=[(P_VOL, 40)])
p.track(2, "hats", -27); p.grid(2, "x.xg..x.x.xg..o.", 57, cond=C_N1ST, locks={"g": [(P_VOL, 36)], "o": [(P_SH, 50)]})
p.track(3, "fmbass", -16.5); bassline(p, 3, A5, "...x..8..x....x.", 33, conds={60: C_4_4})
p.hit(3, 60, 45, C_4_4)
p.track(4, "enshit", -20); vs = prog(p, 4, A5, "..........x.....")
for b, v in enumerate([vs[0], vs[0], vs[1], vs[2]]): p.chord(4, 16 * b + 2, v, C_4_4)
p.lfo(4, 2, 17, 4, 22)                                       # its filter, opening and closing over 16 bars
p.track(5, "keys18", -20)
bar = 0; prev = None
for r, q, bars in A5:
    v = voice(PC[r], q, prev); prev = v
    for b in range(bars):
        for i, k in zip([0, 3, 7, 10], [1, 3, 2, 0]):
            p.hit(5, 16 * (bar + b) + i, v[k] if v[k] >= 60 else v[k] + 12, C_2_2)
        for i, k in zip([5, 13], [3, 1]):                    # the 3rd pass: two notes answering
            p.hit(5, 16 * (bar + b) + i, v[k] if v[k] >= 60 else v[k] + 12, C_3_4)
    bar += bars
p.lfo(5, 2, 5, 8, 14)                                        # the keys' FM depth, over 8 bars
PARTS.append(p)

# 5 evolved again: CIRCUIT. The owner: "the bass and some sounds disconnected and not harmonious between tracks".
# Measured: the chord hits 29 % on their chord (their highpass at 20x the note: only upper partials), the FM bass 42 %
# (depth 112, feedback 111: a growl without a pitch). Now: the chord hits' highpass under the notes, the bass clean
# (1:1, depth 60, feedback 20) and locked to the kick (just after it, never a step before), its fill on the G root
p = Part("circuit_e2", 126, "A", 52, "5 evolved: A minor, 126: the kick and FM bass from the top, the bass answering "
         "just after the kick, chord hits on step 11 with their chord now audible, a second on the 4th pass, the hats "
         "in from the 2nd pass, the keys on the 2nd and 4th with a sparser answer on the 3rd, the chords' filter over 16 bars")
p.bars = 20
A5 = [("A", "m9", 2), ("F", "maj7", 1), ("G", "6", 1)]
p.track(0, "kick", -16.5); p.grid(0, "x......x..x.....", 33); p.hit(0, 61, 33, C_4_4)
p.track(1, "snare3", -19); p.grid(1, "....x.......x...", 47, cond=C_N1ST)
p.hit(1, 63, 47, C_2_2, locks=[(P_VOL, 36)]); p.hit(1, 62, 47, C_4_4, locks=[(P_VOL, 44)])
p.track(2, "hats3", -30)
for bar, cells in enumerate(["..x...x...x...x.", "..x...x...x..gx.", "..x...x...x...x.", "..x...x.g.x....."]):
    p.grid(2, cells, 57, bars=[bar], cond=C_N1ST, locks={"g": [(P_VOL, 34)]})
p.hit(2, 62, 57, C_4_4, locks=[(P_SH, 30)])                  # the open hat: once, the 4th pass's turn
# the bass: sheet 1's FM bass was a thump (7-13 % on its notes), so a sine; and a line, not a cell: the owner heard
# the root-octave cell as "boing bong, repetitive, without any sense". Each bar a long root just after the kick, then
# short answers on the chord's 5th, 3rd or 7th, the bar's end stepping into the next chord (G into F, A into G, B back
# into A, gliding); the octave only as the 4th pass's fill
p.track(3, "sinebass", -16.5, "9=24", "10=90", "41=127")
# the lengths as HOLD locks (the gate is the kit's, a lock does not reach it): 40/32 of a beat long, 5 short
BASS = {2: ("A1", 40), 8: ("A1", 5), 11: ("E2", 5), 14: ("G2", 5),
        18: ("A1", 40), 24: ("A1", 5), 27: ("C2", 5), 30: ("G1", 12, "s"),
        34: ("F1", 40), 40: ("F1", 5), 43: ("C2", 5), 46: ("A1", 12, "s"),
        50: ("G1", 40), 56: ("G1", 5), 59: ("D2", 5), 62: ("B1", 12, "s")}
for st, v in BASS.items():
    p.hit(3, st, pitch(v[0]), locks=[(9, v[1]), (10, 90 if v[1] >= 40 else 44)])   # struck: a slide would not retrigger
p.hit(3, 60, pitch("G2"), C_4_4, locks=[(9, 5), (10, 44)])
# the chord hits: once a bar on the same step was "a repetitive sound"; now a phrase in the gaps the kick, bass and snare
# leave (6, 9, 13), a different place each bar, one hit alternating with the next pass, the turn's two from the 2nd
p.track(4, "enshit2", -20)
vs, prev = [], None
for r, q, bars in A5:
    v = voice(PC[r], q, prev); prev = v; vs += [v] * bars
for st, bar, cd in ((6, 0, 0), (13, 1, C_1_2), (15, 1, C_2_2), (6, 2, 0), (9, 3, C_N1ST), (13, 3, 0)):
    p.chord(4, 16 * bar + st, vs[bar], cd)
p.lfo(4, 2, 17, 4, 22)
p.track(5, "keys18", -20)
bar = 0; prev = None
for r, q, bars in A5:
    v = voice(PC[r], q, prev); prev = v
    for b in range(bars):
        for i, k in zip([0, 3, 7, 10], [1, 3, 2, 0]):
            p.hit(5, 16 * (bar + b) + i, v[k] if v[k] >= 60 else v[k] + 12, C_2_2)
        for i, k in zip([5, 13], [3, 1]):
            p.hit(5, 16 * (bar + b) + i, v[k] if v[k] >= 60 else v[k] + 12, C_3_4)
    bar += bars
p.lfo(5, 2, 5, 8, 14)
PARTS.append(p)

p = Part("circuit_nostab", 126, "A", 52, "Circuit with the chord hits muted, to check which sound repeated")
p.bars = 20; p.L = list(PARTS[-1].L); p.tgt = dict(PARTS[-1].tgt); p.L += ["kit 4 13 0", "kit 4 37 0", "kit 4 35 0", "kit 4 28 64"]; p.tgt.pop(4)
PARTS.append(p)

# 7 evolved: SUSPENSION
p = Part("suspension_e", 110, "D", 54, "7 evolved: D, 110: the sus2 chords moved whole, their 3rd coming in on every "
         "other pass (add9), the rim from the 2nd pass, the cymbal on the 2nd and 4th, the keys' motif gaining a note, "
         "the sub walking down at the end of the 4th, the pad's filter over 16 bars")
p.bars = 20
D7 = [("D", "sus2", 1), ("Bb", "sus2", 1), ("G", "sus2", 2)]
D7b = [("D", "add9", 1), ("Bb", "add9", 1), ("G", "add9", 2)]
p.track(0, "pad2", -20, "8=20", "17=42")
prog(p, 0, D7, "1...............")
prog(p, 0, D7b, ".2..............")
p.lfo(0, 2, 17, 4, 18)
p.track(1, "kick", -16.5); p.grid(1, "x.........x.....", 38); p.grid(1, ".......x........", 38, bars=[3], cond=C_N1ST)
p.track(2, "sperc", -21); p.grid(2, "....x.......x...", 69, cond=C_N1ST); p.hit(2, 63, 69, C_4_4, locks=[(P_VOL, 44)])
p.track(3, "sub", -16, "8=10 9=127 10=127 11=70 41=127"); bassline(p, 3, D7, "x...............", 26)
for s_, n_ in ((56, 31), (60, 29)): p.hit(3, s_, n_, C_4_4)
p.track(4, "keys19", -20)
bar = 0
for r, q, bars in D7:
    root = 57 + (PC[r] - 57) % 12
    for b in range(bars):
        s0 = 16 * (bar + b)
        for i, d in zip([2, 6, 9], [2, 7, 9]):
            p.hit(4, s0 + i, root + d)
        p.hit(4, s0 + 13, root + 14, C_2_2)                  # every other pass: the 9th an octave up
    bar += bars
p.hit(4, 62, 76, C_3_4)
p.track(5, "cym24", -29); p.grid(5, "..x...x...x...x.", 57, cond=C_2_2); p.hit(5, 0, 57, C_1_4, locks=[(P_SH, 90)])
PARTS.append(p)

# TIDE: 5's bass and keys under 7's pad: dorian i7 IV9, a kick on the beat, the snare from the 2nd pass; E, 122
p = Part("tide", 122, "E", 54, "E dorian, 122: 7's ENS pad on Em7 and A9 two bars each, 5's FM bass off the beat with "
         "an octave, 5's FM+STAT keys answering, kick from the top, the snare from the 2nd pass, the hats on the 3rd "
         "and 4th, the pad's filter over 16 bars")
p.bars = 20
E9 = [("E", "m7", 2), ("A", "9", 2)]
Q["9"] = [0, 4, 10, 14]
p.track(0, "pad2", -20, "17=46"); vs = prog(p, 0, E9, "x.......x.......")
p.lfo(0, 2, 17, 4, 20)
p.track(1, "kick", -16.5); p.grid(1, "x...x...x...x...", 40)
p.track(2, "snare3", -19); p.grid(2, "....x.......x...", 59, cond=C_N1ST)       # the rebuilt snare: no ring, no swish
p.hit(2, 63, 59, C_2_2, locks=[(P_VOL, 36)])
p.track(3, "fmbass", -16.5); bassline(p, 3, E9, "..x...x...8...x.", 28)
p.track(4, "keys18", -20)
line(p, 4, {3: "B4", 6: "G4", 11: "D5", 35: "C#5", 38: "B4", 43: ("E5", C_1_2), 44: ("A4", C_2_2)})
for s_, nm in ((19, "B4"), (27, "A4"), (51, "C#5"), (59, "B4")): p.hit(4, s_, pitch(nm), C_2_2)
p.track(5, "hats3", -30)                                     # the rebuilt hats: noise, short; a cell a bar
for bar, cells in enumerate(["..x...x...x...x.", "..x...x...x..gx.", "..x...x...x...x.", "..x...x.g.x....."]):
    p.grid(5, cells, 52, bars=[bar], cond=C_3_4, locks={"g": [(P_VOL, 34)]})
    p.grid(5, cells.replace("g", "x"), 52, bars=[bar], cond=C_4_4)
p.hit(5, 62, 52, C_4_4, locks=[(P_SH, 30)])
PARTS.append(p)

# LANTERN: 5's chord hits in 7's world: add9 chords moved whole (Aadd9 Fadd9 Cadd9 Gadd9), 7's keys moving with them;
# A, 100
p = Part("lantern", 100, "A", 56, "A, 100: 5's ENS chord hits on add9 chords moved whole (Aadd9 Fadd9 Cadd9 Gadd9), "
         "7's FM+STAT keys on each chord's 3rd, 5th and 9th, a kick and sine rim, 7's sub, the cymbal from the 3rd pass, "
         "the chords' filter over 16 bars")
p.bars = 20
A9 = [("A", "add9", 1), ("F", "add9", 1), ("C", "add9", 1), ("G", "add9", 1)]
p.track(0, "enshit", -20); prog(p, 0, A9, "x.....x.........")
p.lfo(0, 2, 17, 4, 20)
p.track(1, "kick", -16.5); p.grid(1, "x......x..x.....", 33)
p.track(2, "sperc", -21); p.grid(2, "....x.......x...", 76)
p.track(3, "sub", -16, "8=10 9=127 10=127 11=60 41=100"); bassline(p, 3, A9, "x.........x.....", 33)
p.track(4, "keys19", -20)
bar = 0
for r, q, bars in A9:
    root = 57 + (PC[r] - 57) % 12
    s0 = 16 * bar
    for i, d in zip([3, 8, 11], [4, 7, 14]): p.hit(4, s0 + i, root + d)
    p.hit(4, s0 + 14, root + 12, C_2_2)
    bar += bars
p.track(5, "cym24", -29); p.grid(5, "..x...x...x...x.", 64, cond=C_3_4); p.grid(5, "..x...x.g.x...x.", 64, cond=C_4_4, locks={"g": [(P_VOL, 40)]})
PARTS.append(p)

# RELAY: 5's drums and bass at half time, 7's keys singing over Dm9 and Bbmaj7; D minor, 112
p = Part("relay", 112, "D", 54, "D minor, 112, half time: 5's kick, snare on 3, hats; 5's FM bass long and short; 5's "
         "ENS chord hits on Dm9 and Bbmaj7; 7's FM+STAT keys singing a motif that ends two ways; the hats doubling on "
         "the 4th pass")
p.bars = 20
D2 = [("D", "m9", 2), ("Bb", "maj7", 2)]
p.track(0, "kick", -16.5); p.grid(0, "x.......x.x.....", 38)
p.track(1, "snare", -19); p.grid(1, "........x.......", 49); p.hit(1, 63, 49, C_4_4, locks=[(P_VOL, 44)])
p.track(2, "hats", -27); p.grid(2, "..x...x...x...x.", 50, locks={}); p.grid(2, "...g...g...g...g", 50, cond=C_4_4, locks={"g": [(P_VOL, 34)]})
p.track(3, "fmbass", -16.5); bassline(p, 3, D2, "...x......8.....", 26)
p.track(4, "enshit", -20); prog(p, 4, D2, "......x.....x...")
p.lfo(4, 2, 17, 4, 18)
p.track(5, "keys19", -20)
line(p, 5, {0: "A4", 6: "F4", 10: "E4", 16: "D4", 32: "A4", 38: "F4", 42: "D5", 48: ("C5", C_1_2), 49: ("A4", C_2_2)})
PARTS.append(p)

def render(lines, path, bpm, secs, solo=None, levl=None, vol=None):
    spec = os.path.join(OUT, "spec.txt")
    extra = "".join(f"\nkit {t} 37 {v}" for t, v in (levl or {}).items()) + "".join(f"\nkit {t} 13 {v}" for t, v in (vol or {}).items())
    open(spec, "w").write(f"bpm {bpm}\nlen 64\nsecs {secs}\n" + "\n".join(lines) + "\nsync" + extra +
                          (f"\nsolo {solo}" if solo is not None else "") + f"\nout {path}\n")
    subprocess.run([MEL, spec], check=True)
    w = wave.open(path); return np.frombuffer(w.readframes(w.getnframes()), '<i2').reshape(-1, 2) / 32768

def level_tracks(p, off):
    levl, vol = {}, {}
    tmp = os.path.join(OUT, "solo.wav"); secs = min(60.0, p.bars * 240 / p.bpm)
    for t, target in p.tgt.items():
        target += off
        lo, hi = 0, 127
        for _ in range(7):
            mid = (lo + hi) // 2; levl[t] = mid
            if lufs(render(p.L, tmp, p.bpm, secs, t, levl, vol)) < target: lo = mid + 1
            else: hi = mid
        levl[t] = lo
        l = lufs(render(p.L, tmp, p.bpm, secs, t, levl, vol))
        while l < target - 0.7 and vol.get(t, 64) < 127:
            vol[t] = min(127, vol.get(t, 64) + 6); l = lufs(render(p.L, tmp, p.bpm, secs, t, levl, vol))
    return levl, vol

key = open(os.path.join(OUT, "key.txt"), "a")
for i, p in enumerate(PARTS, 1):
    if ONLY and p.name not in ONLY: continue
    secs = min(60.0, p.bars * 240 / p.bpm)                    # the whole form and back to its top
    path = os.path.join(OUT, f"{i}_{p.name}.wav")
    off = -4.5
    for it in range(3):
        levl, vol = level_tracks(p, off)
        x = render(p.L, path, p.bpm, secs, None, levl, vol)
        rms = 10 * np.log10((x ** 2).mean())
        if abs(rms + 15.1) < 0.4: break
        off += -15.1 - rms
    open(os.path.join(OUT, f"{i}_{p.name}.txt"), "w").write(f"bpm {p.bpm}\n" + "\n".join(p.L) + "\nsync" + "".join(
        f"\nkit {t} 37 {v}" for t, v in levl.items()) + "".join(f"\nkit {t} 13 {v}" for t, v in vol.items()) + "\n")
    key.write(f"{i}\t{p.name}\t{p.desc}\n")
    print(f"{i} {p.name}: rms {rms:.1f} dBFS, {lufs(x):.1f} LUFS, peak {20 * np.log10(np.abs(x).max()):.1f}, {secs:.0f} s", flush=True)
