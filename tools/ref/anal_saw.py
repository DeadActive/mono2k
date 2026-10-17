#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# SWAVE-SAW, from tools/ref/swave_saw.spec (every value given): the base saw's harmonics and shape, the unison
# pairs' detune and level (read at C6, where cents are wide in Hz), the subs' levels against the base.
#   .venv/bin/python tools/ref/anal_saw.py build/ref/mm_saw
import os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep import load as load2
SR = 44100
D = sys.argv[1]


def load(n):
    return load2(f"{D}/saw_{n}.wav")[:, 0]


def spec(x):
    seg = x[int(0.3 * SR):int(1.8 * SR)]
    n = len(seg)
    sp = np.abs(np.fft.rfft(seg * np.blackman(n))) / n * 4.2
    return np.fft.rfftfreq(n, 1 / SR), sp


def peak(f, sp, f_lo, f_hi):
    m = (f > f_lo) & (f < f_hi)
    i = np.argmax(np.where(m, sp, 0))
    a, b, c = np.log(sp[i - 1] + 1e-12), np.log(sp[i] + 1e-12), np.log(sp[i + 1] + 1e-12)
    p = 0.5 * (a - c) / (a - 2 * b + c)
    return f[i] + p * (f[1] - f[0]), 20 * np.log10(np.exp(b - 0.25 * (a - c) * p))


def cents(f, f0):
    return 1200 * np.log2(f / f0)


hz = lambda n: 440 * 2 ** ((n - 69) / 12)
for nm, note in (("base_c3", 48), ("base_c5", 72)):
    f, sp = spec(load(nm))
    f0 = hz(note)
    h = [peak(f, sp, f0 * k * 0.97, f0 * k * 1.03) for k in range(1, 17)]
    print(f"{nm}: f0 {h[0][0]:.3f} ({cents(h[0][0], f0):+.2f} c)  level {h[0][1]:.1f} dB  harmonics re 1/k:",
          " ".join("%+.1f" % (h[k][1] - h[0][1] + 20 * np.log10(k + 1)) for k in range(1, 16)))
f0 = hz(84)
print("UNIW (UNIL 127) at C6: the three partials around the fundamental (cents, dB re the base)")
for v in [1, 2, 4, 8, 16, 24, 32, 48, 64, 80, 96, 112, 127]:
    f, sp = spec(load(f"uniw{v:03d}"))
    b = peak(f, sp, f0 * 0.997, f0 * 1.003)
    lo = peak(f, sp, f0 * 0.80, f0 * 0.9985)
    hi = peak(f, sp, f0 * 1.0015, f0 * 1.25)
    print(f"  {v:3d}: lower {cents(lo[0], f0):+7.2f}c {lo[1] - b[1]:+5.1f}dB   base {cents(b[0], f0):+5.2f}c   upper {cents(hi[0], f0):+7.2f}c {hi[1] - b[1]:+5.1f}dB")
print("UNIL (UNIW 64): the pair's level re the base")
for v in [16, 32, 64, 96, 127]:
    f, sp = spec(load(f"unil{v:03d}"))
    b = peak(f, sp, f0 * 0.997, f0 * 1.003)
    lo = peak(f, sp, f0 * 0.80, f0 * 0.9985); hi = peak(f, sp, f0 * 1.0015, f0 * 1.25)
    print(f"  {v:3d}: lower {cents(lo[0], f0):+7.2f}c {lo[1] - b[1]:+5.1f}dB   upper {cents(hi[0], f0):+7.2f}c {hi[1] - b[1]:+5.1f}dB")
print("UNIX (UNIW 32): the outer pair re the base")
for tag, vals in (("UNIL 127", [16, 32, 64, 96, 127]), ("UNIL 64", [32, 64, 127])):
    for v in vals:
        f, sp = spec(load(f"unix{v:03d}" if tag == "UNIL 127" else f"unix_l64_{v:03d}"))
        b = peak(f, sp, f0 * 0.997, f0 * 1.003)
        parts = []
        for lo_c, hi_c in ((-200, -40), (-40, -3), (3, 40), (40, 200)):
            p = peak(f, sp, f0 * 2 ** (lo_c / 1200), f0 * 2 ** (hi_c / 1200))
            parts.append(f"{cents(p[0], f0):+7.2f}c {p[1] - b[1]:+5.1f}dB")
        print(f"  {tag} UNIX {v:3d}: " + "   ".join(parts))
f0 = hz(60)
for s in ("subx", "sub1", "sub2"):
    for v in [32, 64, 127]:
        f, sp = spec(load(f"{s}{v:03d}"))
        b = peak(f, sp, f0 * 0.99, f0 * 1.01)
        s1 = peak(f, sp, f0 / 2 * 0.99, f0 / 2 * 1.01)
        s2 = peak(f, sp, f0 / 4 * 0.98, f0 / 4 * 1.02)
        s3 = peak(f, sp, f0 * 1.5 * 0.99, f0 * 1.5 * 1.01)
        print(f"  {s.upper()} {v:3d}: f0/2 {s1[1] - b[1]:+6.1f}dB   f0/4 {s2[1] - b[1]:+6.1f}dB   3f0/2 {s3[1] - b[1]:+6.1f}dB")
