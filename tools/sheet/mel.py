#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Sheet 2: the pitched sounds the owner kept from sheet 1, playing lines and chords. The notes are drawn, not written:
# a scale at random (modes, pentatonics, whole tone, octatonic), a motif by a random walk over it cycling over trigs
# it does not divide, chords stacked from the scale and voice-led; four bars at 103, each item at -18 LUFS.
#   mel.py MEL OUTDIR SEED
import sys, os, subprocess, random, wave, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from loud import lufs                                        # noqa: E402  (the BS.1770 loudness)
MEL, OUT, SEED = sys.argv[1], sys.argv[2], int(sys.argv[3])
os.makedirs(OUT, exist_ok=True)
rng = random.Random(SEED)
SR = 44100
SCALES = {
    "aeolian": [0, 2, 3, 5, 7, 8, 10], "phrygian": [0, 1, 3, 5, 7, 8, 10], "dorian": [0, 2, 3, 5, 7, 9, 10],
    "harmonic minor": [0, 2, 3, 5, 7, 8, 11], "phrygian dominant": [0, 1, 4, 5, 7, 8, 10],
    "lydian": [0, 2, 4, 6, 7, 9, 11], "locrian": [0, 1, 3, 5, 6, 8, 10], "hirajoshi": [0, 2, 3, 7, 8],
    "in sen": [0, 1, 5, 7, 10], "minor pentatonic": [0, 3, 5, 7, 10], "whole tone": [0, 2, 4, 6, 8, 10],
    "octatonic": [0, 1, 3, 4, 6, 7, 9, 10],
}
NN = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
# sheet 1's pitched sounds: number -> (candidate, its note, held, edits as A)
S = {
    1: (1171, 28, 0, ""), 2: (5398, 33, 0, ""), 3: (3489, 34, 1, ""), 4: (1954, 35, 1, ""),
    6: (3611, 39, 0, "12=72 28=94 69=60|12 60 90"), 7: (5456, 39, 1, ""), 8: (5418, 41, 1, ""),
    9: (3419, 42, 0, "25=64|32 0 50,18 0 60"), 11: (1542, 52, 0, ""), 12: (4196, 53, 1, "49=39 57=39 65=45 28=90"),
    13: (2123, 53, 1, ""), 15: (4514, 55, 1, "8=40 18=50 19=80"), 16: (5166, 62, 1, ""), 17: (753, 71, 0, ""),
}

def track_lines(t, n):
    cid, _, _, e = S[n]
    kit, _, sq = e.partition("|")
    return [f"track {t} {cid} {kit}".strip()], [f"squeeze {t} {s}" for s in sq.split(",") if s]

def pitch(root, sc, d):
    return root + sc[d % len(sc)] + 12 * (d // len(sc))

def euclid(k, n, rot):
    return [((i - rot) % n * k) % n < k for i in range(n)]

def scale_pick(used):
    names = [s for s in SCALES if used.count(s) < 2]
    s = rng.choice(names); used.append(s); return s

def line_item(n, used):
    cid, centre, held, _ = S[n]
    sc_name = scale_pick(used); sc = SCALES[sc_name]
    key = rng.randrange(12)
    root = centre - ((centre - key) % 12)
    if centre - root > 7: root += 12 if root + 12 <= centre + 5 else 0
    L = rng.choice([3, 4, 5, 5, 6, 7])
    d = rng.choice([0, 0, len(sc) // 2, 1]); mot = [d]
    for _ in range(L - 1):
        if rng.random() < 0.15: st = rng.choice([-1, 1]) * rng.randint(len(sc) // 2, len(sc))
        else: st = rng.choice([-1, 1, -1, 1, -2, 2, 0])
        d = max(-len(sc), min(2 * len(sc), d + st)); mot.append(d)
    if held: hits = euclid(rng.randint(2, 5), 16, rng.randrange(16))
    else: hits = euclid(rng.randint(5, 11), 16, rng.randrange(16))
    var = rng.random() < 0.5
    out, sq = track_lines(0, n)
    out += [f"seed {SEED * 100 + n}"]
    k = 0; notes = []
    for s in range(64):
        if not hits[s % 16]: continue
        dd = mot[k % L] + (1 if var and s >= 48 else 0); k += 1
        p = pitch(root, sc, dd)
        while p < 22: p += 12
        while p > 88: p -= 12
        fl = ("a" if rng.random() < 0.15 else "") + ("s" if held and rng.random() < 0.3 else "")
        out.append(f"trig 0 {s} {p} {fl}".rstrip()); notes.append(p)
    if not held: out.append("motion 0")
    out += sq
    names = " ".join(NN[(pitch(root, sc, m)) % 12] for m in mot)
    return out, f"line, {sc_name} on {NN[root % 12]}: a {L}-note motif ({names}) over {sum(hits)} hits a bar" + \
                (", lifted a step in bar 4" if var else "")

SHAPES = {"triad": [0, 2, 4], "7th": [0, 2, 6], "upper 7th": [2, 4, 6], "fourths": [0, 3, 6], "sus2": [0, 1, 4],
          "cluster": [0, 1, 2], "open": [0, 4, 8]}
SHAPES4 = {"7th": [0, 2, 4, 6], "fourths": [0, 3, 6, 9], "add9": [0, 2, 4, 8], "sus": [0, 1, 4, 7], "open": [0, 4, 7, 9]}

def progression(sc, k):
    ch = [0 if rng.random() < 0.6 else rng.randrange(len(sc))]
    while len(ch) < k:
        c = rng.randrange(len(sc))
        if c != ch[-1]: ch.append(c)
    return ch

def lead(prev, chord):
    """each voice the octave nearest the previous chord's same voice"""
    if prev is None: return sorted(chord)
    out = []
    for p, q in zip(sorted(chord), prev):
        best = min((p + 12 * o for o in range(-2, 3)), key=lambda x: abs(x - q))
        out.append(best)
    return sorted(out)

def bars_of(k):
    return {2: [2, 2], 3: rng.choice([[2, 1, 1], [1, 1, 2]]), 4: [1, 1, 1, 1]}[k]

def chord_item(voices, used):
    """voices: three sound numbers, low to high (the same one thrice for a stack)"""
    sc_name = scale_pick(used); sc = SCALES[sc_name]
    k = rng.choice([2, 3, 4]); prog = progression(sc, k); bars = bars_of(k)
    shape = rng.choice(list(SHAPES)); sh = SHAPES[shape]
    centre = 50 + rng.randint(-3, 6)
    root = centre - ((centre - rng.randrange(12)) % 12)
    out, sqs = [], []
    for t, n in enumerate(voices):
        o, s = track_lines(t, n); out += o; sqs += s
        out.append(f"kit {t} 14 {[40, 64, 88][t]}")
    out.append(f"seed {SEED * 100 + 50 + len(used)}")
    prev, step, names = None, 0, []
    for c, b in zip(prog, bars):
        ch = lead(prev, [pitch(root, sc, c + i) for i in sh]); prev = ch
        names.append("-".join(NN[p % 12] for p in ch))
        for bar in range(b):
            for t, p in enumerate(ch): out.append(f"trig {t} {step + 16 * bar} {p}")
        step += 16 * b
    for t, n in enumerate(voices):
        if not S[n][2]: out.append(f"motion {t}")
    out += sqs
    who = "#%d stacked" % voices[0] if len(set(voices)) == 1 else "#%d #%d #%d" % tuple(voices)
    return out, f"chords, {who}: {sc_name} on {NN[root % 12]}, {shape}, " + " | ".join(names)

def ens_item(used):
    sc_name = scale_pick(used); sc = SCALES[sc_name]
    k = rng.choice([2, 3, 4]); prog = progression(sc, k); bars = bars_of(k)
    shape = rng.choice(list(SHAPES4)); sh = SHAPES4[shape]
    root = 42 + ((rng.randrange(12) - 42) % 12)            # its register as heard on sheet 1 or above: its filter sits high over the note
    out, sq = track_lines(0, 9)
    out.append(f"seed {SEED * 100 + 90 + len(used)}")
    hits = euclid(rng.randint(4, 9), 16, rng.randrange(16))
    step, names = 0, []
    for c, b in zip(prog, bars):
        ps = [pitch(root, sc, c + i) for i in sh]
        bass = ps[0]; iv = []
        for p in ps[1:]:
            d = p - bass
            while d > 12: d -= 12
            iv.append(d if d else 12)
        names.append("-".join(NN[p % 12] for p in ps))
        for s in range(step, step + 16 * b):
            if hits[s % 16]:
                out.append(f"trig 0 {s} {bass}"); out.append(f"ens 0 {s} {iv[0]} {iv[1]} {iv[2]}")
        step += 16 * b
    out.append("motion 0"); out += sq
    return out, f"chords, #9 (SWAV-ENS, four voices in one): {sc_name} on {NN[root % 12]}, {shape}, " + " | ".join(names)

def render(lines, path, trim, vol=None):
    spec = os.path.join(OUT, "spec.txt")
    vl = "".join(f"\nkit {t} 13 {vol}" for t in range(3)) if vol else ""
    with open(spec, "w") as f:
        f.write("bpm 103\nlen 64\nsecs 9.9\n" + "\n".join(lines) + f"\ntrim {trim}{vl}\nout {path}\n")
    subprocess.run([MEL, spec], check=True)
    w = wave.open(path); return np.frombuffer(w.readframes(w.getnframes()), '<i2').reshape(-1, 2) / 32768

used = []
items = [line_item(n, used) for n in [1, 2, 3, 4, 6, 7, 8, 11, 12, 13, 15, 16, 17]]
items += [ens_item(used), ens_item(used)]
items += [chord_item(v, used) for v in [[3, 3, 3], [4, 4, 4], [7, 7, 7], [8, 8, 8], [12, 12, 12], [13, 13, 13],
                                         [15, 15, 15], [16, 16, 16], [7, 13, 16]]]
key = open(os.path.join(OUT, "key.txt"), "w")
for i, (lines, desc) in enumerate(items, 1):
    path = os.path.join(OUT, f"{i:02d}.wav")
    lo, hi = 10, 400
    for _ in range(8):
        mid = (lo + hi) // 2
        if lufs(render(lines, path, mid)) < -18: lo = mid + 1
        else: hi = mid
    x = render(lines, path, lo); l = lufs(x); vol = None
    while l < -18.5 and (vol or 64) < 127:                  # the LEVL at its top: the rest from VOL
        vol = min(127, (vol or 64) + 6); x = render(lines, path, 400, vol); l = lufs(x); lo = 400
    tail = f"\ntrim {lo}" + ("".join(f"\nkit {t} 13 {vol}" for t in range(3)) if vol else "")
    with open(os.path.join(OUT, f"{i:02d}.txt"), "w") as f: f.write("\n".join(lines) + tail + "\n")
    key.write(f"{i}\t{desc}\n")
    print(f"{i:2d} {l:6.1f} LUFS  peak {20*np.log10(np.abs(x).max()+1e-9):5.1f}  {desc}", flush=True)
