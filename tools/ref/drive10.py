#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Drive apart from the build-up (tools/ref/chain.py drive10): per render, the fundamental's level over the whole note
# (demodulated, 20 ms steps) and, settled, h1 with h3 and h5 against it over the note's last 0.8 s. Both instruments
# side by side; the worst difference of the level over time (after the first 50 ms) beside the settled numbers.
#   .venv/bin/python tools/ref/drive10.py build/ref/mm_cdrive10 build/ref/m2k_cdrive10
import os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep import load, SR
from hres8 import envelope


def settled(x, f0, t0, t1):
    s = x[int(t0 * SR):int(t1 * SR)]
    t = np.arange(len(s)) / SR
    h = [abs(np.dot(s, np.exp(-2j * np.pi * k * f0 * t))) * 2 / len(s) for k in (1, 3, 5)]
    db = lambda v: 20 * np.log10(max(v, 1e-12))
    return db(h[0]), max(-60, db(h[1]) - db(h[0])), max(-60, db(h[2]) - db(h[0]))


if __name__ == "__main__":
    a, b = sys.argv[1:3]
    for n in sorted(x[:-4] for x in os.listdir(a) if x.startswith("d10_") and x.endswith(".wav")):
        note = int(n.split("_n")[1][:2])
        f0 = 440 * 2 ** ((note - 69) / 12)
        xa, xb = load(os.path.join(a, n + ".wav"))[:, 0], load(os.path.join(b, n + ".wav"))[:, 0]
        gate = len(xa) / SR - 0.05
        ta, ea = envelope(xa, f0, 882)
        tb, eb = envelope(xb, f0, 882)
        m = min(len(ea), len(eb))
        sel = (ta[:m] > 0.05) & (ta[:m] < gate - 0.05)
        diff = 20 * np.log10(np.maximum(eb[:m], 1e-9) / np.maximum(ea[:m], 1e-9))[sel]
        sa, sb = settled(xa, f0, gate - 0.85, gate - 0.05), settled(xb, f0, gate - 0.85, gate - 0.05)
        print(f"{n:24s} settled h1 {sa[0]:7.2f} {sb[0]:7.2f} ({sb[0] - sa[0]:+5.2f}) | h3 {sa[1]:6.1f} {sb[1]:6.1f} | h5 {sa[2]:6.1f} {sb[2]:6.1f}"
              f" | over the note: M2K - MM median {np.median(diff):+5.2f} dB, worst {diff[np.argmax(np.abs(diff))]:+6.2f} dB")
