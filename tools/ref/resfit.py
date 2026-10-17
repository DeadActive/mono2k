#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The resonance resolved (tools/ref/chain.py res6): the response on noise against the open chain, and a resonance
# curve P(f) = A / (1 + (2 Q (f - fp) / fp)^2) fitted over the bins around its peak: where the peak sits against the
# nominal cutoff (f0 x 2^((step - 16) / 8)), its height over the passband, its Q. One fit per render; the fit, not a
# single bin, carries the answer (each bin of a noise ratio is uncertain by a dB or two).
#   .venv/bin/python tools/ref/resfit.py build/ref/mm_cres6 [build/ref/m2k_cres6]
import os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep import load, SR

N = 32768


def psd(x, a=0.3, b=4.2):
    s = x[int(a * SR):int(b * SR), 0]
    w = np.hanning(N)
    segs = [s[i:i + N] * w for i in range(0, len(s) - N, N // 4)]
    return np.fft.rfftfreq(N, 1 / SR), np.mean([np.abs(np.fft.rfft(v)) ** 2 for v in segs], axis=0)


def fit(d, name, ref, fc):
    f, p = psd(load(os.path.join(d, name + ".wav")))
    _, r = psd(load(os.path.join(d, ref + ".wav")))
    ratio = p / np.maximum(r, 1e-30)
    k = int(np.argmax(np.where(np.abs(f / fc - 1) < 0.15, np.convolve(ratio, np.ones(5) / 5, "same"), 0)))
    best = None
    for q in np.exp(np.linspace(np.log(2), np.log(3000), 160)):
        bw = f[k] / q
        sel = np.abs(f - f[k]) < max(6 * bw, 40)
        for fp in f[k] + np.linspace(-3, 3, 61) * (SR / N):
            m = 1 / (1 + (2 * q * (f[sel] - fp) / fp) ** 2)
            a = np.dot(m, ratio[sel]) / np.dot(m, m)
            e = np.mean((np.log(np.maximum(a * m, 1e-30)) - np.log(np.maximum(ratio[sel], 1e-30))) ** 2)
            if best is None or e < best[0]:
                best = (e, fp, q, a)
    e, fp, q, a = best
    # the passband: the ratio well above (HP) or below (LP) the peak, in dB
    pb = ratio[(f > fc * 1.6) & (f < min(fc * 2.1, 17000))] if "hp" in name else ratio[(f > fc / 4) & (f < fc / 2.5)]
    gain = 10 * np.log10(a / np.median(pb)) if len(pb) else float("nan")
    return fp, q, gain, np.sqrt(e)


def report(d):
    rows = {}
    for n in sorted(x[:-4] for x in os.listdir(d) if x.endswith(".wav") and (x.startswith("r6_hp") or x.startswith("r6_lp"))):
        note = int(n.split("_n")[1][:2])
        step = 40 if note == 84 else 48
        fc = 440 * 2 ** ((note - 69) / 12) * 2 ** ((step - 16) / 8)
        fp, q, g, e = fit(d, n, f"r6_open_n{note}", fc)
        rows[n] = (fp, (fp / fc - 1) * 100, q, g, e)
    return rows


def tune(d):
    out = {}
    for n in sorted(x[:-4] for x in os.listdir(d) if x.startswith("r6_tune")):
        x = load(os.path.join(d, n + ".wav"))[int(0.15 * SR):int(0.55 * SR), 0]
        sp = np.abs(np.fft.rfft(x * np.hanning(len(x)), 8 * len(x)))
        k = int(np.argmax(sp))
        a, b, c = np.log(sp[k - 1]), np.log(sp[k]), np.log(sp[k + 1])
        fq = (k + 0.5 * (a - c) / (a - 2 * b + c)) * SR / (8 * len(x))
        out[n] = 1200 * np.log2(fq / 261.6256)
    return out


if __name__ == "__main__":
    ds = sys.argv[1:]
    res = [report(d) for d in ds]
    print(f"{'render':16s}" + "".join(f" | {os.path.basename(d):>32s}" for d in ds))
    for n in res[0]:
        cells = []
        for r in res:
            if n in r:
                fp, off, q, g, e = r[n]
                cells.append(f"{fp:8.1f} Hz {off:+6.2f}% Q{q:6.0f} {g:+5.1f} dB")
            else:
                cells.append(" " * 32)
        print(f"{n:16s}" + "".join(f" | {c}" for c in cells))
    for d in ds:
        t = tune(d)
        if t:
            print(os.path.basename(d), "GND-SIN TUNE, cents from C4:", " ".join(f"{k[-3:]}:{v:+.1f}" for k, v in t.items()))
