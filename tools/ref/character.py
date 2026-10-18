#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# A machine's character, the Monomachine against MONO2K, on renders of the same spec lines (tools/ref/mm_ref render,
# host/m2k_ref): its level; its spectrum in octave bands, each against the line's own whole (where its energy sits);
# for a pitched line, its harmonics against the fundamental, and how much of its energy is not on a harmonic (aliasing,
# inharmonic grit), dB under the whole; its energy over 6 kHz. The steady part only (0.2 .. 1.1 s).
#   .venv/bin/python tools/ref/character.py SPEC MM_DIR M2K_DIR [NAME ...]
import math, os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep import load, SR

BANDS = [63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000]
UNPITCHED = {2, 14}                    # GND-NOIS, and SWAVE-ENS's chords


def db(v):
    return 10 * math.log10(max(v, 1e-30))


def analyse(path, note=None):
    return analyse_x(load(path), note)


def analyse_x(y, note=None):
    x = y.mean(axis=1)
    x = x[int(0.2 * SR):int(1.1 * SR)]
    X = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2
    f = np.fft.rfftfreq(len(x), 1 / SR)
    tot = X[f > 20].sum()
    out = dict(rms=20 * math.log10(max(math.sqrt(np.mean(x ** 2)), 1e-9)))
    out["bands"] = [db(X[(f >= b / math.sqrt(2)) & (f < b * math.sqrt(2))].sum() / tot) for b in BANDS]
    if note is not None:
        f0 = 440 * 2 ** ((note - 69) / 12)
        on = np.zeros(len(f), bool)
        harm = []
        for k in range(1, int(18000 / f0) + 1):
            near = np.abs(f - k * f0) < max(3.0, 0.012 * k * f0)
            on |= near
            if k <= 12:
                harm.append(db(X[near].sum()))
        out["harm"] = [h - harm[0] for h in harm]
        out["grit"] = db(X[(~on) & (f > 20)].sum() / tot)
        out["hf"] = db(X[f > 6000].sum() / tot)
    return out


if __name__ == "__main__":
    spec, mm, m2k = sys.argv[1], sys.argv[2], sys.argv[3]
    want = set(sys.argv[4:])
    print(f"{'line':16s} {'MM':>6s} {'M2K-MM':>6s} | bands 63 125 250 500 1k 2k 4k 8k 16k: M2K - MM, dB | "
          f"harmonics 2..8: M2K - MM | grit MM/M2K | >6k MM/M2K")
    for l in open(spec):
        w = l.split()
        if not w or w[0].startswith("#") or (want and w[0] not in want):
            continue
        name, machine, note = w[0], int(w[1]), int(w[2])
        pm, pk = os.path.join(mm, name + ".wav"), os.path.join(m2k, name + ".wav")
        if not (os.path.exists(pm) and os.path.exists(pk)):
            continue
        n = None if machine in UNPITCHED else note
        a, b = analyse(pm, n), analyse(pk, n)
        s = f"{name:16s} {a['rms']:6.1f} {b['rms'] - a['rms']:+6.1f} | " + " ".join(f"{y - x:+4.0f}" for x, y in zip(a["bands"], b["bands"]))
        if n is not None:
            s += " | " + " ".join(f"{y - x:+4.0f}" for x, y in zip(a["harm"][1:8], b["harm"][1:8]))
            s += f" | {a['grit']:5.1f}/{b['grit']:5.1f} | {a['hf']:5.1f}/{b['hf']:5.1f}"
        print(s)
