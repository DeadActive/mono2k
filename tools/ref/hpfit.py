#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The HP's linear shape at each HPQ: the Monomachine's response on noise at C4 (chain_filt2 qn_hp_*, fc 4186 Hz,
# DIST 32) fitted, as the digital sections see it (bilinear), by a resonant two-pole and a one-pole (MONO2K's), two
# two-poles, or a two-pole and two one-poles, over 0.3 .. 3.5 fc where it stands above -40 dB. From HPQ 32 up the
# four-pole fits win at every Q (rms 1.5 against 1.9 dB, the noise's own spread ~1.5): a pole near the cutoff that
# the resonant settings add, steepening the skirt (as a C3 sine an octave under a resonant cutoff confirms: 7.6-9.4 dB
# under MONO2K's); at HPQ 0 it is not there.
#   .venv/bin/python tools/ref/hpfit.py
import os, sys, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from sweep import load, SR
from resfit import psd
fc = 261.6256 * 2 ** (32 / 8)
def resp(d, n, ref="qn_open"):
    f, p = psd(load(f"build/ref/{d}/{n}.wav")); _, r = psd(load(f"build/ref/{d}/{ref}.wav"))
    return f, 10 * np.log10(np.maximum(p, 1e-30) / np.maximum(r, 1e-30))
def W(f, f0):                       # bilinear: the analog frequency ratio a TPT section sees
    return np.tan(np.pi * f / SR) / np.tan(np.pi * f0 / SR)
def hp2(f, f0, q):
    w = W(f, f0); s = 1j * w
    return s * s / (s * s + s / q + 1)
def hp1(f, f0):
    s = 1j * W(f, f0); return s / (s + 1)
MODELS = {
    "2p+1p (now)": (lambda f, p: hp2(f, fc * p[0], p[1]) * hp1(f, fc * p[2]), [1.0, 8.0, 1.75]),
    "2p+2p": (lambda f, p: hp2(f, fc * p[0], p[1]) * hp2(f, fc * p[2], p[3]), [1.0, 8.0, 1.5, 0.7]),
    "2p+1p+1p": (lambda f, p: hp2(f, fc * p[0], p[1]) * hp1(f, fc * p[2]) * hp1(f, fc * p[3]), [1.0, 8.0, 1.75, 1.0]),
}
def fit(model, p0, f, r, sel):
    best = None
    p = np.array(p0, float)
    def err(p):
        if np.any(p <= 0.05): return 1e9
        m = 20 * np.log10(np.abs(model(f[sel], p)) + 1e-12)
        return np.sqrt(np.mean((m - r[sel]) ** 2))
    e = err(p); step = np.array([0.2] * len(p))
    for it in range(4000):           # a plain coordinate search, in log steps
        improved = False
        for i in range(len(p)):
            for sgn in (1, -1):
                q = p.copy(); q[i] *= math.exp(sgn * step[i])
                eq = err(q)
                if eq < e: p, e, improved = q, eq, True
        if not improved:
            step *= 0.5
            if step.max() < 1e-4: break
    return p, e
if __name__ == "__main__":
    for q in (0, 16, 32, 48, 64, 80, 96, 112, 120):
        f, r = resp("mm_cfilt2", f"qn_hp_{q:03d}")
        r = np.convolve(r, np.ones(5) / 5, "same")
        sel = (f > 0.3 * fc) & (f < 3.5 * fc) & (r > -40)
        out = []
        for name, (m, p0) in MODELS.items():
            p0 = list(p0); p0[1] = max(0.6, [0.8, 1.4, 3.5, 8.7, 30, 85, 190, 486, 800][[0, 16, 32, 48, 64, 80, 96, 112, 120].index(q)])
            p, e = fit(m, p0, f, r, sel)
            out.append(f"{name}: rms {e:4.2f} dB [{' '.join(f'{v:.3g}' for v in p)}]")
        print(f"HPQ {q:3d}  " + "  |  ".join(out))
