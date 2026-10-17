#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Parameter sweeps, the same on both instruments: writes a spec that sets one machine's parameter (by its CC) across
# its range, from a clean start (no attack, held, filters open, no EQ, no reduction, no distortion), so each render
# shows that parameter alone; then measures both sides' renders.
#   .venv/bin/python tools/ref/sweep.py spec MACHINE CC [VALUES] > tools/ref/x.spec
#   .venv/bin/python tools/ref/sweep.py measure build/ref/mm_x build/ref/m2k_x [--partials]
import os, sys, wave
import numpy as np

SR = 44100
CLEAN = "7=100 56=0 57=127 58=127 59=0 60=64 61=64 62=64 63=0 72=0 73=127 74=0 75=0 76=0 77=0 78=64 79=64 81=64 82=0 84=64"   # DIST and VOL at their 0 and init: 64


def spec(machine, cc, values, note=48, gate=1.2, extra=""):
    for v in values:
        print(f"m{machine}_cc{cc}_{v:03d} {machine} {note} 100 {gate} 0.2 {CLEAN} {extra} {cc}={v}".rstrip())


def load(p):
    with wave.open(p) as w:
        x = np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(np.float64) / 32768
    return x.reshape(-1, 2)


def partials(x, lo=20, hi=8000, top=12):
    seg = x[int(0.3 * SR):int(0.3 * SR) + 65536, 0]
    if len(seg) < 65536:
        seg = np.pad(seg, (0, 65536 - len(seg)))
    sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) / len(seg)
    f = np.fft.rfftfreq(len(seg), 1 / SR)
    pk = [i for i in range(2, len(sp) - 2) if sp[i] > sp[i - 1] and sp[i] >= sp[i + 1] and lo < f[i] < hi]
    pk.sort(key=lambda i: -sp[i])
    ref = sp[pk[0]] if pk else 1
    return [(round(f[i], 2), round(20 * np.log10(sp[i] / ref), 1)) for i in sorted(pk[:top])]


def harmonics(x, f0, n=12):
    seg = x[int(0.3 * SR):int(0.3 * SR) + 32768, 0]
    sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg))))
    out = []
    for h in range(1, n + 1):
        b = int(round(h * f0 * len(seg) / SR))
        out.append(sp[max(b - 3, 0):b + 4].max() if b + 4 < len(sp) else 0)
    m = max(out) or 1
    return [round(20 * np.log10(max(v / m, 1e-6)), 1) for v in out]


def describe(x):
    held = x[int(0.3 * SR):int(1.1 * SR)]
    rms = 20 * np.log10(max(np.sqrt(np.mean(held ** 2)), 1e-9))
    side = 20 * np.log10(max(np.sqrt(np.mean((held[:, 0] - held[:, 1]) ** 2)), 1e-9))
    return rms, side


def measure(a, b, show_partials):
    names = sorted(n[:-4] for n in os.listdir(a) if n.endswith(".wav") and os.path.exists(os.path.join(b, n)))
    for n in names:
        xa, xb = load(os.path.join(a, n + ".wav")), load(os.path.join(b, n + ".wav"))
        (ra, sa), (rb, sb) = describe(xa), describe(xb)
        print(f"{n:22s} rms MM {ra:6.1f} M2K {rb:6.1f}   side MM {sa:6.1f} M2K {sb:6.1f}")
        if show_partials:
            print("   MM ", partials(xa))
            print("   M2K", partials(xb))


if __name__ == "__main__":
    if sys.argv[1] == "spec":
        m, cc = int(sys.argv[2]), int(sys.argv[3])
        vals = [int(v) for v in sys.argv[4].split(",")] if len(sys.argv) > 4 else list(range(0, 128, 16)) + [127]
        spec(m, cc, vals, extra=" ".join(sys.argv[5:]))
    else:
        measure(sys.argv[2], sys.argv[3], "--partials" in sys.argv)
