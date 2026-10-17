#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The resonance at low cutoffs, settled (tools/ref/chain.py hres8): a sine on the cutoff, 12 s. Its output demodulated
# at the sine's own frequency gives the amplitude over time; the build-up A(t) = |A (1 - exp(-t / tau - j 2 pi df t))|
# is fitted for the settled level A, the time constant tau (Q = pi f0 tau) and the detuning df between the sine and the
# resonance (sign not resolved by the magnitude alone). The settled level is read directly too, over 10.5 .. 11.9 s.
#   .venv/bin/python tools/ref/hres8.py build/ref/mm_chres8 build/ref/m2k_chres8
import os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep import load, SR


def envelope(x, f, hop=441):
    t = np.arange(len(x)) / SR
    z = x * np.exp(-2j * np.pi * f * t)
    k = int(SR / f * 8)                                # eight periods: a smooth average
    c = np.convolve(z, np.ones(k) / k, "same") * 2
    return t[::hop], np.abs(c[::hop])


def fit(t, a, f0):
    sel = (t > 0.05) & (t < 11.9)
    t, a = t[sel], a[sel]
    best = None
    for tau in np.exp(np.linspace(np.log(0.01), np.log(30), 200)):
        for df in np.linspace(0, 3.0, 121):
            m = np.abs(1 - np.exp((-1 / tau - 2j * np.pi * df) * t))
            amp = np.dot(m, a) / np.dot(m, m)
            e = np.mean((amp * m - a) ** 2) / np.mean(a ** 2)
            if best is None or e < best[0]:
                best = (e, tau, df, amp)
    e, tau, df, amp = best
    return amp, tau, df, np.pi * f0 * tau, e


if __name__ == "__main__":
    dirs = sys.argv[1:]
    for n in (48, 72):
        f = 440 * 2 ** ((n - 69) / 12)
        for kind in ("hp127", "hp096", "lp127"):
            name = f"h8_sin_{kind}_n{n}"
            cells = []
            for d in dirs:
                x = load(os.path.join(d, name + ".wav"))[:, 0]
                t, a = envelope(x, f)
                settled = 20 * np.log10(max(np.mean(a[(t > 10.5) & (t < 11.9)]), 1e-12))
                amp, tau, df, q, e = fit(t, a, f)
                cells.append(f"settled {settled:7.2f} dB  fit {20 * np.log10(amp):7.2f} dB tau {tau:6.2f} s (Q {q:6.0f}) "
                             f"df {df:5.2f} Hz ({1200 * np.log2((f + df) / f):5.2f} c) err {e:.3f}")
            print(f"{name:18s} " + "\n                   ".join(f"{os.path.basename(d):10s} {c}" for d, c in zip(dirs, cells)))
