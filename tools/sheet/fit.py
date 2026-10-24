#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# How well each track of a part fits the harmony: each track alone, bar by bar, the share of its pitched energy (60 Hz
# .. 3 kHz) on the bar's chord notes, and on the key's scale.   fit.py MEL SPEC BPM KEY "A m9,A m9,F maj7,G 6"
import sys, os, wave, subprocess, numpy as np
from numpy.lib.stride_tricks import sliding_window_view as swv
MEL, SPEC, BPM, KEY, CH = sys.argv[1], sys.argv[2], float(sys.argv[3]), sys.argv[4], sys.argv[5]
NN = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"]
PC = {n: i for i, n in enumerate(NN)}; PC.update({"C#": 1, "D#": 3, "F#": 6, "G#": 8, "A#": 10})
Q = {"m7": [0, 3, 7, 10], "m9": [0, 3, 7, 10, 14], "maj7": [0, 4, 7, 11], "6": [0, 4, 7, 9], "add9": [0, 4, 7, 14],
     "sus2": [0, 2, 7], "9": [0, 4, 7, 10, 14], "maj9": [0, 4, 7, 11, 14]}
chords = [c.split() for c in CH.split(",")]
cpcs = [{(PC[r] + i) % 12 for i in Q[q]} for r, q in chords]
mode = [0, 2, 3, 5, 7, 8, 10] if not KEY.endswith("M") else [0, 2, 4, 5, 7, 9, 11]
scale = {(PC[KEY.rstrip("M")] + d) % 12 for d in mode}
spec = open(SPEC).read()
tracks = sorted({int(l.split()[1]) for l in spec.splitlines() if l.split()[:1] and l.split()[0] in ("orig", "mtrack", "ptrack")})
names = {int(l.split()[1]): " ".join(l.split()[:3]) for l in spec.splitlines() if l.split()[:1] and l.split()[0] in ("orig", "mtrack", "ptrack")}
bar = int(240 / BPM * 44100)
for t in tracks:
    d = os.path.dirname(SPEC)
    open(f"{d}/fit_spec.txt", "w").write(f"len 64\nsecs {16 * 240 / BPM:.2f}\n" + spec + f"\nsolo {t}\nout {d}/fit.wav\n")
    subprocess.run([MEL, f"{d}/fit_spec.txt"], check=True)
    w = wave.open(f"{d}/fit.wav"); x = np.frombuffer(w.readframes(w.getnframes()), '<i2').reshape(-1, 2).mean(1) / 32768
    on, sc, tot = 0.0, 0.0, 0.0
    for b in range(16):
        seg = x[b * bar:(b + 1) * bar]
        if len(seg) < 8192: break
        M = np.abs(np.fft.rfft(swv(seg, 8192)[::4096] * np.hanning(8192), axis=1)) ** 2
        f = np.fft.rfftfreq(8192, 1 / 44100); k = (f > 60) & (f < 3000)
        pc = np.round(12 * np.log2(f[k] / 440) + 69).astype(int) % 12
        C = np.bincount(pc, weights=M[:, k].sum(0), minlength=12)
        cp = cpcs[b % len(cpcs)]
        on += C[list(cp)].sum(); sc += C[list(scale)].sum(); tot += C.sum()
    print(f"track {t} {names[t]:22s} on the chord {100 * on / tot:5.1f}%   on the scale {100 * sc / tot:5.1f}%")
