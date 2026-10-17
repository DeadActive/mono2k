#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# A SWAV-ENS chord's voices, each measured on its own: the voice's pitch from the spec line's PCH2..4 and TUNE (the
# interval table read from firmware/gen/m2k_tables.h), then the strongest of its partials 1..6 under 3 kHz that no
# other voice's partial sits within 40 c of, its peak within +-60 c; cents from the intended pitch per voice (NaN:
# off). The window: from 15 ms after the onset to where the note falls 30 dB under its peak (1.4 s at most).
#   used by regress.py (ens_accept); .venv/bin/python tools/ref/ens_voices.py SPEC DIR [DIR]
import os, re, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep import load, SR

HERE = os.path.dirname(os.path.abspath(__file__))


def _table(name):
    h = open(os.path.join(HERE, "..", "..", "firmware", "gen", "m2k_tables.h")).read()
    body = h[h.index(name + "["):]
    return [int(v) for v in re.findall(r"-?\d+", body[body.index("{") + 1:body.index("}")])]


PCH = _table("T_ENS_PCH")


def intended(note, cc):
    n = note + (cc.get(55, 64) - 64) * 2 / 64
    out = [n]
    for c in (48, 49, 50):
        v = PCH[cc.get(c, 62)]
        out.append(None if v == -32768 else n + v / 64)
    return out


def measure(x, notes):
    y = x[:, 0]
    e = np.convolve(np.abs(y), np.ones(256) / 256, "same")
    on = int(np.argmax(e > e.max() * 0.05))
    k = np.nonzero(e > e.max() * 0.03)[0]
    a, b = on + int(0.015 * SR), min(k[-1], on + int(1.4 * SR))
    seg = y[a:b]
    w = np.hanning(len(seg))
    nf = 1 << int(np.ceil(np.log2(len(seg) * 8)))
    sp = np.abs(np.fft.rfft(seg * w, nf)); df = SR / nf
    fs = [None if n is None else 440 * 2 ** ((n - 69) / 12) for n in notes]
    out = []
    for i, f in enumerate(fs):
        if f is None:
            out.append(float("nan")); continue
        best = None
        for h in range(1, 7):
            fh = h * f
            if fh > 3000: break
            if any(j != i and g is not None and any(abs(1200 * np.log2(fh / (m * g))) < 40 for m in range(1, 13)) for j, g in enumerate(fs)):
                continue
            lo, hi = int(fh * 2 ** (-60 / 1200) / df), int(fh * 2 ** (60 / 1200) / df) + 1
            kk = lo + int(np.argmax(sp[lo:hi]))
            if best is None or sp[kk] > best[0]:
                a0, b0, c0 = np.log(sp[kk - 1] + 1e-15), np.log(sp[kk] + 1e-15), np.log(sp[kk + 1] + 1e-15)
                dd = 0.5 * (a0 - c0) / (a0 - 2 * b0 + c0) if a0 - 2 * b0 + c0 else 0
                best = (sp[kk], 1200 * np.log2((kk + dd) * df / fh))
        out.append(float("nan") if best is None else float(best[1]))
    return out


def spec_lines(path):
    out = {}
    for l in open(path):
        if not l.strip() or l.startswith("#"): continue
        w = l.split()
        cc = {}
        for t in w[6:]:
            if t.startswith("m2k:"): continue
            t = t[3:] if t.startswith("mm:") else t
            k, v = t.split("="); cc[int(k)] = int(v)
        out[w[0]] = (int(w[2]), cc)
    return out


if __name__ == "__main__":
    lines = spec_lines(sys.argv[1])
    for n, (note, cc) in lines.items():
        row = []
        for d in sys.argv[2:]:
            row.append(measure(load(os.path.join(d, n + ".wav")), intended(note, cc)))
        print(f"{n:20s} " + " | ".join(" ".join("  off" if np.isnan(v) else f"{v:+5.1f}" for v in r) for r in row) +
              ("" if len(row) < 2 else "   M2K-MM: " + " ".join("  -  " if np.isnan(a) else f"{b - a:+5.1f}" for a, b in zip(*row))))
