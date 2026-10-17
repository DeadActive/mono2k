#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The HP's saturation, modelled: float versions of candidate structures (the clip's place in and around the resonant
# two-pole, one-poles after it) run on a C3 sine at the cutoff (HPQ 48, BASE 16), their settled h1 h3 h5 h7 against
# the Monomachine's (chain_drive12 / drive10, settled at DIST 64 72 80 127), the output scaled once on MONO2K's linear
# render (DIST 16). Findings (docs: README, known gaps):
#   m2k           MONO2K's clamped two-pole states: h1 3-6 dB loud, h3 25-27 dB short below DIST 80: it barely clips
#   hsat          the two-pole's output clipped at the DIST ceiling, then the 1.75 fc one-pole: h3 h5 within 2 dB,
#                 h1 3.6 dB loud
#   hsat_pole     the same with a one-pole at the cutoff after the clip: h1 within 0.7 dB, h3 h5 within 1 dB from
#                 DIST 72 (h5 h7 2-3.7 dB short at 64)
# but a clip on the HP's output breaks every unresonant drive case (dd_076..124: a square at the ceiling overshoots
# the two-pole, and the Monomachine does not clip that): the Monomachine's limit sits inside the resonance.
#   .venv/bin/python tools/ref/hpsat.py
import os, sys, math
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep import load, SR


def interp(pts, x):
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        if x <= x1:
            return y0 + (y1 - y0) * (x - x0) / (x1 - x0)
    return pts[-1][1]


def dist_h(d):
    if d < 64:
        return -1.35 * (64 - d) + 0.34
    if d >= 88:
        return 10.55 + (d - 88) * 0.62
    return interp([(64, 0), (72, 0), (76, 1.95), (80, 4.58), (84, 7.78), (88, 10.55)], d)


def dist_c(d):
    return 0.6 + interp([(64, 5.79), (68, 3.94), (72, 0.0), (76, -2.27), (80, -3.24)], d)


HPQ = [(0, .80), (8, 1.03), (16, 1.43), (24, 1.9), (32, 3.5), (40, 5.6), (48, 8.7), (56, 15.6), (64, 30), (72, 50),
       (80, 85), (88, 140), (96, 190), (104, 340), (112, 486), (120, 800), (127, 1200)]
HPC = [(0, 1.24), (16, 1.26), (32, 1.114), (48, 1.0), (64, 0.994), (80, 0.999), (96, 0.9982)]


def sat(v, c):
    return max(-c, min(c, v))


def run(kind, note, q, d, secs=1.0, pole=1.0, one=1.75):
    f0 = 440 * 2 ** ((note - 69) / 12)
    fc = f0 * interp(HPC, q)
    k = 1 / interp(HPQ, q)
    gain, c = 10 ** (dist_h(d) / 20), 10 ** (dist_c(d) / 20)
    n = int(secs * SR)
    x = np.sin(2 * np.pi * f0 * np.arange(n) / SR) * gain
    g = math.tan(math.pi * fc / SR)
    a1 = 1 / (1 + g * (g + k)); a2 = g * a1; a3 = g * a2
    def G(r):
        t = math.tan(math.pi * r * fc / SR)
        return t / (1 + t)
    G1, Gp = G(one), G(pole)
    s1 = s2 = sp = s1p = 0.0
    y = np.zeros(n)
    for i in range(n):
        u = sat(x[i], c)
        v3 = u - s2; v1 = a1 * s1 + a2 * v3; v2 = s2 + a2 * s1 + a3 * v3
        hp = u - k * v1 - v2
        s1 = 2 * v1 - s1; s2 = 2 * v2 - s2
        if kind == "m2k":
            s1, s2 = sat(s1, c), sat(s2, c)
        else:
            hp = sat(hp, c)
        if kind == "hsat_pole":
            v = (hp - s1p) * Gp; lo = v + s1p; s1p = lo + v; hp -= lo
        v = (hp - sp) * G1; lo = v + sp; sp = lo + v
        y[i] = hp - lo
    return y, f0


def harm(y, f0, t0=0.5):
    seg = y[int(t0 * SR):]
    w = np.hanning(len(seg)); t = np.arange(len(seg)) / SR
    return [20 * math.log10(abs(seg * w @ np.exp(-2j * np.pi * k * f0 * t)) / (w.sum() / 2) + 1e-12) for k in (1, 3, 5, 7)]


if __name__ == "__main__":
    MM = {64: (-15.9, -17.2, -20.9, -24.3), 72: (-21.7, -22.7, -26.1, -28.8), 80: (-24.9, -25.9, -29.1, -31.7),
          127: (-24.9, -25.9, -29.1, -31.6)}           # measured: chain_drive10 d10_hp048_n48_d*
    y, f0 = run("m2k", 48, 48, 16)
    root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    off = harm(load(os.path.join(root, "build/ref/m2k_cdrive12/d12_b16_q048_n48_d016.wav"))[:, 0], f0, 1.0)[0] - harm(y, f0)[0]
    for kind in ("m2k", "hsat", "hsat_pole"):
        cells = []
        for d in (64, 72, 80, 127):
            h = [v + off for v in harm(*run(kind, 48, 48, d))]
            cells.append(" ".join(f"{a - b:+5.1f}" for a, b in zip(h, MM[d])))
        print(f"{kind:10s} model - Monomachine, h1 h3 h5 h7 at DIST 64 | 72 | 80 | 127:  " + " | ".join(cells))
