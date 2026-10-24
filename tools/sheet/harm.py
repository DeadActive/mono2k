#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Sheet 4: sheet 3's 24 instruments, same numbers, on better notes. Progressions from the minor-key idioms of 2000s
# electronica/IDM (i9-VImaj7, dorian i7-IV9, m7s in parallel...), lush chord qualities (m7 m9 maj7 maj9 6 add9),
# close voicings led smoothly, a sine sub on the roots; lines that land on chord tones on the strong steps, in an
# A A' A B form over four bars, over a quiet bed (sheet 3's #2) and the sub.
#   harm.py MEL SHEET3_DIR OUTDIR SEED
import sys, os, re, random, itertools, wave, subprocess, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from loud import lufs                                        # noqa: E402
if __name__ == "__main__":
    MEL, S3, OUT, SEED = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
    os.makedirs(OUT, exist_ok=True)
else:
    SEED = 0
rng = random.Random(SEED)
NN = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"]
Q = {"m7": [0, 3, 7, 10], "m9": [0, 3, 7, 10, 14], "maj7": [0, 4, 7, 11], "maj9": [0, 4, 7, 11, 14], "9": [0, 4, 7, 10, 14],
     "6": [0, 4, 7, 9], "add9": [0, 4, 7, 14], "madd9": [0, 3, 7, 14], "m11": [0, 3, 7, 10, 14, 17]}
AEO, DOR, PHR = [0, 2, 3, 5, 7, 8, 10], [0, 2, 3, 5, 7, 9, 10], [0, 1, 3, 5, 7, 8, 10]
# (name, [(root above the tonic, quality)], bars each, scale)
T = [
    ("i9 VImaj7", [(0, "m9"), (8, "maj7")], [2, 2], AEO),
    ("i7 iv9", [(0, "m7"), (5, "m9")], [2, 2], AEO),
    ("i7 IV9 (dorian)", [(0, "m7"), (5, "9")], [2, 2], DOR),
    ("i9 VII6 VImaj7 v7", [(0, "m9"), (10, "6"), (8, "maj7"), (7, "m7")], [1, 1, 1, 1], AEO),
    ("i7 v7 VImaj7 iv9", [(0, "m7"), (7, "m7"), (8, "maj7"), (5, "m9")], [1, 1, 1, 1], AEO),
    ("VImaj7 VII6 i9", [(8, "maj7"), (10, "6"), (0, "m9")], [1, 1, 2], AEO),
    ("i(add9) IIImaj7 VII6 iv7", [(0, "madd9"), (3, "maj7"), (10, "6"), (5, "m7")], [1, 1, 1, 1], AEO),
    ("i7 IImaj7 (phrygian)", [(0, "m7"), (1, "maj7")], [2, 2], PHR),
    ("iv9 i7", [(5, "m9"), (0, "m7")], [2, 2], AEO),
    ("m7s in parallel: i III iv VI", [(0, "m7"), (3, "m7"), (5, "m7"), (8, "m7")], [1, 1, 1, 1], AEO),
    ("m9s in parallel: i VII VI VII", [(0, "m9"), (10, "m9"), (8, "m9"), (10, "m9")], [1, 1, 1, 1], AEO),
    ("i9 IIImaj9", [(0, "m9"), (3, "maj9")], [2, 2], AEO),
    ("i7 VImaj7 IIImaj7 VIIadd9", [(0, "m7"), (8, "maj7"), (3, "maj7"), (10, "add9")], [1, 1, 1, 1], AEO),
    ("i11 VImaj9", [(0, "m11"), (8, "maj9")], [2, 2], AEO),
]
KEYS = [0, 4, 5, 7, 9, 2]                                     # C E F G A D minor

def chord_pcs(key, root, q): return [(key + root + i) % 12 for i in Q[q]]

def pick_voices(key, root, q, n):
    """n of the chord's notes: a shell (3rd, 7th or 6th, the colour) for three, four with the colour kept"""
    iv = Q[q]
    third = iv[1]; seventh = iv[3] if len(iv) > 3 and iv[3] < 12 else None
    colour = [i for i in iv if i >= 12]
    if n == 3:
        v = [third, seventh if seventh is not None else 7, colour[0] if colour else 7 if seventh is not None else 0]
    else:
        v = ([third, 7, seventh, colour[0]] if colour and seventh is not None else
             [0, third, 7, seventh if seventh is not None else (colour[0] if colour else 12)])
        if len(set(x % 12 for x in v)) < 4 and colour: v = [third, 7, seventh or 9, colour[0]]
    return sorted(set((key + root + x) % 12 for x in v))

def voicings(pcs, lo=50, hi=74, span=12):
    opts = [[p for p in range(lo, hi + 1) if p % 12 == pc] for pc in pcs]
    for c in itertools.product(*opts):
        c = sorted(c)
        if c[-1] - c[0] <= span: yield c

def lead(prev, pcs, span):
    vs = list(voicings(pcs, span=span))
    if not vs: vs = list(voicings(pcs, span=span + 7))
    if prev is None: return min(vs, key=lambda v: abs(sum(v) / len(v) - 61))
    return min(vs, key=lambda v: sum(abs(a - b) for a, b in zip(v, prev)) + 0.3 * abs(sum(v) / len(v) - 61))

def sub_note(key, root):
    p = 33 + ((key + root - 33) % 12)
    return p

def prog_events(tpl, key, n, span):
    """[(step, bars, voicing, root pitch for the sub, chord pcs)]"""
    name, chords, bars, sc = tpl
    out, prev, step = [], None, 0
    for (r, q), b in zip(chords, bars):
        pcs = pick_voices(key, r, q, n)
        v = lead(prev, pcs, span); prev = v
        out.append((step, b, v, sub_note(key, r), chord_pcs(key, r, q)))
        step += 16 * b
    return out

def chord_name(key, r, q): return NN[(key + r) % 12] + {"m7": "m7", "m9": "m9", "maj7": "maj7", "maj9": "maj9", "9": "9",
                                                         "6": "6", "add9": "add9", "madd9": "m(add9)", "m11": "m11"}[q]

def euclid(k, n, rot): return [((i - rot) % n * k) % n < k for i in range(n)]

SUB = 0                                                       # GND-SIN
def sub_lines(t, ev):
    L = [f"ptrack {t} {SUB}", f"kit {t} 8 6", f"kit {t} 9 127", f"kit {t} 10 127", f"kit {t} 11 70", f"kit {t} 41 127",
         f"kit {t} 37 58", f"kit {t} 35 0"]
    for s, b, v, root, _ in ev:
        for bar in range(b): L.append(f"trig {t} {s + 16 * bar} {root}")
    return L

def ens_trig(t, s, v):
    iv = [p - v[0] for p in v[1:]]
    return [f"trig {t} {s} {v[0]}", f"ens {t} {s} {iv[0]} {iv[1]} {iv[2]}"]

def chords_item(head, kind, ens, tpl, key):
    """a pad (a chord a bar) or a stab (a syncopated cell) on the progression, the sub under it"""
    nt = 1 if ens else 3
    ev = prog_events(tpl, key, 4 if ens else 3, 12 if ens else 14)
    hits = [True] + [False] * 15 if kind == 0 else euclid(rng.randint(3, 5), 16, rng.randint(1, 3))
    L = list(head)
    for s0, b, v, root, _ in ev:
        for s in range(s0, s0 + 16 * b):
            if not hits[s % 16]: continue
            if ens: L += ens_trig(0, s, v)
            else: L += [f"trig {t} {s} {p}" for t, p in enumerate(v)]
    L += sub_lines(nt, ev)
    return L, nt + 1

CELLS_K = ["x..x..x...x.x...", "x.x...x.x...x...", "..x..x....x..x..", "x..x..x..x..x...", "x...x.x...x.x..."]
CELLS_L = ["x.......x...x...", "x.....x.....x...", "x...........x.x.", "x.......x.x.....", "....x.......x..."]

def line_notes(tpl, key, kind, ev, rng):
    """AA'AB: bar one's contour from chord tones on the strong steps and scale steps between; each bar refit to its
    chord; the last ends on the root or the fifth, held"""
    name, chords, bars, sc = tpl
    scale = sorted(set((key + x) % 12 for x in sc))
    arp = kind == 2 and rng.random() < 0.34
    cell = "x.x.x.x.x.x.x.x." if arp else rng.choice(CELLS_K if kind == 2 else CELLS_L)
    centre = 67 if kind == 2 else 70
    def chord_at(s):
        for s0, b, v, root, pcs in ev:
            if s0 <= s < s0 + 16 * b: return pcs
        return ev[-1][4]
    def near(p, pcs, up=None):
        c = [q for q in range(p - 7, p + 8) if q % 12 in pcs and 55 <= q <= 84]
        if up is not None: c = [q for q in c if (q > p) == up] or c
        return min(c, key=lambda q: (abs(q - p), q)) if c else p
    def step(p, d):
        q = p
        for _ in range(abs(d)):
            q += 1 if d > 0 else -1
            while q % 12 not in scale: q += 1 if d > 0 else -1
        return q
    notes = {}
    if arp:
        order = rng.choice([[0, 1, 2, 3], [0, 1, 2, 3, 2, 1], [0, 2, 1, 3], [3, 2, 1, 0]])
        k = 0
        for s in range(64):
            if cell[s % 16] != "x": continue
            pcs = chord_at(s)
            tones = sorted(q for q in range(centre - 6, centre + 10) if q % 12 in pcs)[:4]
            notes[s] = tones[order[k % len(order)] % len(tones)]; k += 1
        return notes, cell, True
    # bar one
    pos = [i for i, c in enumerate(cell) if c == "x"]
    moves = [rng.choice([-1, 1, -1, 1, -2, 2]) if rng.random() < 0.65 else 0 for _ in pos]
    first = near(centre, chord_at(0))
    for bar in range(4):
        p = near(first + (0 if bar != 1 else rng.choice([-3, 2, 3])), chord_at(16 * bar))
        for j, i in enumerate(pos):
            s = 16 * bar + i
            strong = i in (0, 8) or j == 0
            if j: p = step(p, moves[j]) if moves[j] else near(p, chord_at(s), up=rng.random() < 0.5)
            if strong: p = near(p, chord_at(s))
            if bar == 3 and j == len(pos) - 1:
                pcs = chord_at(s); tonic = (ev[-1][3]) % 12
                p = near(p, [tonic, (tonic + 7) % 12])
            notes[s] = p
    return notes, cell, False

def line_item(head, kind, tpl, key, bed):
    ev = prog_events(tpl, key, 4, 12)
    notes, cell, arp = line_notes(tpl, key, kind, ev, rng)
    L = list(head)
    for s, p in sorted(notes.items()):
        sl = " s" if kind == 3 and rng.random() < 0.25 else ""
        L.append(f"trig 0 {s} {p}{sl}")
    L += [bed.replace("mtrack 0", "mtrack 1"), "kit 1 37 34", "kit 1 14 64"]
    for s0, b, v, root, _ in ev:
        for bar in range(b): L += ens_trig(1, s0 + 16 * bar, v)
    L += sub_lines(2, ev)
    return L, 3, ("an arpeggio of the chords" if arp else f"a line on '{cell}'")

def render(lines, path, trim, vol=None, nt=1):
    spec = os.path.join(OUT, "spec.txt")
    vl = "".join(f"\nkit {t} 13 {vol}" for t in range(nt)) if vol else ""
    open(spec, "w").write("bpm 103\nlen 64\nsecs 9.9\n" + "\n".join(lines) + f"\ntrim {trim}{vl}\nout {path}\n")
    subprocess.run([MEL, spec], check=True)
    w = wave.open(path); return np.frombuffer(w.readframes(w.getnframes()), '<i2').reshape(-1, 2) / 32768

if __name__ == "__main__":
    key_out = open(os.path.join(OUT, "key.txt"), "w")
    bed = [l for l in open(os.path.join(S3, "02.txt")).read().splitlines() if l.startswith("mtrack")][0]
    order = list(range(len(T))); rng.shuffle(order)
    for n in range(1, 25):
        src = open(os.path.join(S3, f"{n:02d}.txt")).read().splitlines()
        head = [l for l in src if l.startswith("mtrack")]
        kind = int(head[0].split()[2]); ens = len(head) == 1 and kind in (0, 1) and any(l.startswith("ens") for l in src)
        tpl = T[order[(n - 1) % len(T)]]; key = KEYS[(n * 5 + SEED) % len(KEYS)]
        if kind in (0, 1):
            lines, ntr = chords_item(head, kind, ens, tpl, key); what = "pad" if kind == 0 else "stab"
        else:
            lines, ntr, what = line_item(head, kind, tpl, key, bed)
        names = " | ".join(chord_name(key, r, q) for r, q in tpl[1])
        path = os.path.join(OUT, f"{n:02d}.wav")
        lo, hi = 10, 400
        for _ in range(8):
            mid = (lo + hi) // 2
            if lufs(render(lines, path, mid)) < -18: lo = mid + 1
            else: hi = mid
        x = render(lines, path, lo); l = lufs(x)
        open(os.path.join(OUT, f"{n:02d}.txt"), "w").write("\n".join(lines) + f"\ntrim {lo}\n")
        desc = f"{n}\t{NN[key]} minor\t{tpl[0]}\t{names}\t{what}"
        key_out.write(desc + "\n"); print(f"{desc}\t{l:.1f} LUFS", flush=True)
