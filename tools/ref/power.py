#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The power test: one loop on machines both instruments have, with the same controls (tools/ref/power.loop),
# played on the Monomachine (tools/ref/mm_ref ROM loop DIR) and on MONO2K (host/m2k_ref DIR --loop DRIVE: the bus as
# it is, and driven). For each: rms, peak, the low end (the rms under 120 Hz, dBFS, and its share of the energy),
# crest; and each track found where the spec puts it (the kick's thumps on the beats, the hat's highs on the offbeats,
# the bass's low end between the kicks), so a track that did not play shows.
#   .venv/bin/python tools/ref/power.py build/power
import math, os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep import load, SR

STEP = SR * 15 / 120


def band(x, lo, hi):
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / SR)
    X[(f < lo) | (f >= hi)] = 0
    return np.fft.irfft(X, len(x))


def db(v):
    return 20 * math.log10(max(v, 1e-12))


def measure(path):
    x = load(path).mean(axis=1)
    x = x[int(2 * STEP * 16):]                       # past the first two bars
    rms, peak = math.sqrt(np.mean(x ** 2)), np.abs(x).max()
    low = band(x, 0, 120)
    hi = band(x, 4000, SR / 2)
    lowrms = math.sqrt(np.mean(low ** 2))
    def at(sig, steps, width=0.06):
        out = []
        for bar in range(int(len(x) / (16 * STEP))):
            for s in steps:
                a = int((bar * 16 + s) * STEP)
                out.append(np.mean(sig[a:a + int(width * SR)] ** 2))
        return db(math.sqrt(np.mean(out)))
    return dict(rms=db(rms), peak=db(peak), low=db(lowrms), share=100 * lowrms ** 2 / rms ** 2, crest=db(peak / rms),
                kick=at(low, (0, 4, 8, 12)), hat_on=at(hi, (2, 6, 10, 14)), hat_off=at(hi, (1, 5, 9, 13)),
                bass=at(low, (2, 7, 11, 15), 0.05))


if __name__ == "__main__":
    d = sys.argv[1]
    rows = [("Monomachine", "loop_mm.wav"), ("MONO2K, bus as it is", "loop_m2k_driv0.wav"),
            ("MONO2K, driven bus", "loop_m2k_driv88.wav")]
    print(f"{'':22s} {'rms':>7s} {'peak':>7s} {'<120 Hz':>8s} {'share':>6s} {'crest':>6s} | kick  hat on/off  bass")
    for name, f in rows:
        m = measure(os.path.join(d, f))
        print(f"{name:22s} {m['rms']:7.1f} {m['peak']:7.1f} {m['low']:8.1f} {m['share']:5.1f}% {m['crest']:6.1f} | "
              f"{m['kick']:5.1f} {m['hat_on']:5.1f}/{m['hat_off']:5.1f} {m['bass']:5.1f}")
