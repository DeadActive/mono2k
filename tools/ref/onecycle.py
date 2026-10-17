#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# One cycle, exactly: the pitch measured first (the fundamental's peak, interpolated), then the steady part averaged
# over that period; prints the harmonics against an ideal saw and returns the cycle.
#   .venv/bin/python tools/ref/onecycle.py file.wav [file.wav ...]
import sys, wave
import numpy as np
SR = 44100


def load(p):
    with wave.open(p) as w:
        return np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(float).reshape(-1, 2)[:, 0] / 32768


def f0_of(x, guess=None):
    seg = x[int(0.3 * SR):int(1.5 * SR)]
    sp = np.abs(np.fft.rfft(seg * np.blackman(len(seg))))
    f = np.fft.rfftfreq(len(seg), 1 / SR)
    if guess:
        m = (f > guess * 0.97) & (f < guess * 1.03)
    else:
        m = f > 20
    i = int(np.argmax(np.where(m, sp, 0)))
    a, b, c = np.log(sp[i - 1]), np.log(sp[i]), np.log(sp[i + 1])
    return f[i] + 0.5 * (a - c) / (a - 2 * b + c) * (f[1] - f[0])


def cycle(x, f0, n=512):
    seg = x[int(0.3 * SR):int(1.5 * SR)]
    per = SR / f0
    k = int(len(seg) / per) - 2
    grid = np.arange(n) * per / n
    return np.mean([np.interp(i * per + grid, np.arange(len(seg)), seg) for i in range(k)], axis=0)


if __name__ == "__main__":
    for p in sys.argv[1:]:
        x = load(p)
        f0 = f0_of(x)
        c = cycle(x, f0)
        sp = np.abs(np.fft.rfft(c))[1:25]
        print(f"{p.split('/')[-1]}: f0 {f0:.3f} Hz  peak {c.max():+.4f} min {c.min():+.4f} mean {c.mean():+.5f}")
        print("   harmonics re 1st, minus an ideal saw's:", " ".join("%+.1f" % (20 * np.log10(sp[h] / sp[0]) + 20 * np.log10(h + 1)) for h in range(1, 24)))
