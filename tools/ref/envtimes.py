#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Times the amp envelope in the amp.spec renders, Monomachine against MONO2K: decay and release to -20 and -40 dB,
# attack to within 1 dB of the top.   .venv/bin/python tools/ref/envtimes.py build/ref/mm_amp build/ref/m2k_amp
import os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep import load as load2

HOP = 0.002


def env(path):
    x = load2(path)[:, 0]
    n = int(44100 * HOP)
    e = np.sqrt(np.convolve(x ** 2, np.ones(n * 4) / (n * 4), "same"))[::n]   # rms over 8 ms, every 2 ms
    return 20 * np.log10(np.maximum(e, 1e-9))


def fall(e, start, ref, by):
    for i in range(start, len(e)):
        if e[i] < ref - by:
            return (i - start) * HOP * 1000
    return float("inf")


def times(kind, e):
    if kind == "dec":
        p = int(np.argmax(e[:200]))
        return fall(e, p, e[p], 20), fall(e, p, e[p], 40)
    if kind == "rel":
        off = int(0.5 / HOP)
        return fall(e, off, e[off - 5], 20), fall(e, off, e[off - 5], 40)
    top = e[int(2.5 / HOP)]
    for i in range(len(e)):
        if e[i] > top - 1:
            return i * HOP * 1000, top
    return float("inf"), top


mm, m2k = sys.argv[1], sys.argv[2]
for kind, vals in (("dec", [0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 126, 127]),
                   ("rel", [0, 16, 32, 48, 64, 80, 96, 112, 126]), ("atk", [0, 16, 32, 48, 64, 80, 96, 112, 126])):
    print(kind.upper(), "   value   MM -20dB  -40dB(ms)    M2K -20dB  -40dB(ms)" if kind != "atk" else "   value   MM to top(ms)  level    M2K to top(ms)  level")
    for v in vals:
        a = times(kind, env(f"{mm}/{kind}{v:03d}.wav"))
        b = times(kind, env(f"{m2k}/{kind}{v:03d}.wav"))
        print("        %5d   %8.0f %8.0f       %8.0f %8.0f" % (v, a[0], a[1], b[0], b[1]))
