#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# GND-NOIS's RED, fitted to the Monomachine's (tools/ref/chain_gnoi42.spec, its numbers in regress_ref.json): the colour
# is white, pink (the economy pink filter) and brown (a one-pole on the pink's first, 16.5 Hz, and one over the top) from
# the same noise, their weights (none below 0) fitted per measured RED value to the octaves (each against 250 Hz .. 8
# kHz's, as the suite measures) and the level against RED 0, the chain's own shape (the Monomachine's white noise
# through it) as the weighting. Prints the weights for gen_tables.py (GNOI_RED_W).
#   .venv/bin/python tools/ref/gnoi_fit.py [build/ref/mm_cgnoi42]
import json, math, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from sweep import load, SR

PINK = ((16.5, 42.15), (264.0, 8.014), (3946.0, 2.448))      # the economy pink filter: one-poles, their DC gains
PINK_W = 0.1848                                               # and the white through it
OCT = (125, 250, 500, 1000, 2000, 4000, 8000, 16000)
REDS = (0, 16, 32, 48, 64, 80, 96, 112, 127)


def lp(f, fc):
    z = np.exp(-2j * np.pi * f / SR)
    g = 1 - math.exp(-2 * math.pi * fc / SR)
    return g / (1 - (1 - g) * z)                                # y += g (x - y): DC gain 1


def basis(f, fb, ft=None):
    """white, pink (scaled to white's level at 1 kHz), brown (a one-pole at fb, then one at ft over the top; scaled
    alike)"""
    pk = PINK_W + sum(k * lp(f, fc) for fc, k in PINK)
    br = lp(f, fb) * (lp(f, ft) if ft else 1)
    i = np.argmin(np.abs(f - 1000))
    return np.stack([np.ones_like(pk), pk / abs(pk[i]), br / abs(br[i])])


def model(w, B, C, f):
    H = np.abs((w[:, None] * B).sum(axis=0)) ** 2 * C
    def oc(c):
        s = (f >= c / math.sqrt(2)) & (f < c * math.sqrt(2)); return H[s].mean()
    r = np.mean([oc(c) for c in (250, 500, 1000, 2000, 4000, 8000)])          # as regress.py's noise_bands
    return np.array([10 * math.log10(oc(c) / r) for c in OCT]), 10 * math.log10(H.sum() / C.sum())


def least_squares(res, w, it=60):
    """Levenberg-Marquardt on a few parameters, numerical Jacobian"""
    lam = 1e-2
    r = res(w)
    for _ in range(it):
        J = np.stack([(res(w + e) - r) / 1e-5 for e in np.eye(len(w)) * 1e-5], axis=1)
        A = J.T @ J
        step = np.linalg.solve(A + lam * np.diag(np.diag(A) + 1e-9), -J.T @ r)
        r2 = res(w + step)
        if r2 @ r2 < r @ r:
            w, r, lam = w + step, r2, lam / 3
        else:
            lam *= 4
    return w


def main():
    ref = json.load(open(os.path.join(HERE, "regress_ref.json")))["cases"]
    d = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "build", "ref", "mm_cgnoi42")
    y = load(os.path.join(d, "g42_red_000.wav"))[int(0.2 * SR):int(0.9 * SR)].mean(axis=1)
    C = np.abs(np.fft.rfft(y * np.hanning(len(y)))) ** 2
    f = np.fft.rfftfreq(len(y), 1 / SR)
    sel = (f > 20) & (f < 20000); C, f = C[sel], f[sel]
    m0 = ref["gnoi:g42_red_000:noise_bands:{}"]
    best = None
    # the brown on the pink's own first pole (16.5 Hz: 12 or 25 fit no better), its top searched
    for fb, ft in [(16.5, ft) for ft in (None, 8000.0, 11000.0, 14000.0, 18000.0)]:
        B = basis(f, fb, ft)
        w = np.array([1.0, 0.0, 0.0])
        b0, _ = model(w, B, C, f)
        rows, worst = [], 0.0
        for r in REDS:
            m = ref[f"gnoi:g42_red_{r:03d}:noise_bands:{{}}"]
            tb = np.array(m[1:]) - np.array(m0[1:])
            tl = m[0] - m0[0]
            def res(q):
                b, l = model(q ** 2, B, C, f)                     # the weights squared: none below 0
                return np.concatenate([b - b0 - tb, [2 * (l - tl)]])
            tries = [least_squares(res, np.sqrt(np.maximum(s0, 1e-3)))
                     for s0 in (w, np.array([1.0, 0.0, 0.0]), np.array([0.0, 1.0, 0.0]), np.array([0.0, 0.0, 1.0]),
                                np.array([0.0, 0.5, 0.5]))]
            q = min(tries, key=lambda q: float(res(q) @ res(q)))
            w = q ** 2
            b, l = model(w, B, C, f)
            rows.append((r, w.copy(), np.abs(b - b0 - tb).max(), l - tl))
            worst = max(worst, np.abs(b - b0 - tb).max())
        print(f"brown at {fb:5.1f} Hz, its top at {ft}: worst band {worst:.2f} dB")
        if best is None or worst < best[0]:
            best = (worst, (fb, ft), rows)
    worst, fb, rows = best
    print(f"best: brown at {fb[0]} Hz, its top at {fb[1]}")
    for r, w, wb, wl in rows:
        print(f"RED {r:3d}: white {w[0]:7.4f} pink {w[1]:7.4f} brown {w[2]:7.4f}   worst band {wb:.2f} dB, level {wl:+.2f} dB")
    rows = [w for _, w, _, _ in rows]
    print("RED_W = [" + ", ".join("(" + ", ".join(f"{v:.4f}" for v in w) + ")" for w in rows) + "]")


if __name__ == "__main__":
    main()
