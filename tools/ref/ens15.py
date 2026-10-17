#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# SWAVE-ENS's page measured (tools/ref/chain.py ens15): a held C4, one control at a time.
#   voices   for each render, the intervals whose partials it adds over the single voice (PCH2..4 OFF): each candidate
#            interval (-36..+36 semitones) scored by the energy its partials add, the best ones refined to cents
#   wave     the single voice's first 12 harmonics, dB from the first: its shape
#   chorus   CHRL / CHRW: the fundamental's line over 3 s (a split into copies, a widening), and its pitch over time
#            (the phase of the line demodulated: rate and depth of any movement), and its level over time
#   tune     the fundamental against C4
#   .venv/bin/python tools/ref/ens15.py build/ref/mm_cens15 [build/ref/m2k_cens15]
import os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep import load, SR

F0 = 261.6256


def spec(x, a=0.3, b=1.4, pad=8):
    s = x[int(a * SR):int(b * SR)]
    w = np.hanning(len(s))
    n = 1 << int(np.ceil(np.log2(len(s) * pad)))
    return np.abs(np.fft.rfft(s * w, n)) / (w.sum() / 2), SR / n


def at(sp, df, f, cents=15):
    lo, hi = int(f * 2 ** (-cents / 1200) / df), int(f * 2 ** (cents / 1200) / df) + 1
    return sp[lo:hi].max() if hi < len(sp) else 0.0


def peak_c(sp, df, f, cents=60):
    lo, hi = int(f * 2 ** (-cents / 1200) / df), int(f * 2 ** (cents / 1200) / df) + 1
    k = lo + int(np.argmax(sp[lo:hi]))
    a, b, c = np.log(sp[k - 1] + 1e-15), np.log(sp[k] + 1e-15), np.log(sp[k + 1] + 1e-15)
    d = 0.5 * (a - c) / (a - 2 * b + c) if a - 2 * b + c else 0
    return 1200 * np.log2((k + d) * df / f), 20 * np.log10(sp[k] + 1e-15)


def voices(sp, df, ref, nmax=3):
    """the intervals this spectrum adds over the reference (the single voice): semitones, cents off, dB of the voice"""
    found = []
    extra = np.maximum(sp - ref, 0)
    for _ in range(nmax):
        best = None
        for i in range(-36, 37):
            if i == 0 or any(i == f[0] for f in found):
                continue
            fi = F0 * 2 ** (i / 12)
            sc = sum(at(extra, df, h * fi) / h for h in range(1, 6) if h * fi < 9000)
            if best is None or sc > best[1]:
                best = (i, sc)
        if best is None or best[1] < 1e-4:
            break
        c, lev = peak_c(sp, df, F0 * 2 ** (best[0] / 12))
        found.append((best[0], c, lev))
        fi = F0 * 2 ** (best[0] / 12)                 # take its partials out before the next search
        for h in range(1, 6):
            lo, hi = int(h * fi * 2 ** (-30 / 1200) / df), int(h * fi * 2 ** (30 / 1200) / df) + 1
            extra[lo:hi] = 0
    return found


def demod(x, f, a=0.3, b=2.9, hop=220):
    t = np.arange(len(x)) / SR
    z = x * np.exp(-2j * np.pi * f * t)
    k = int(SR / f * 6)
    c = np.convolve(z, np.ones(k) / k, "same")[int(a * SR):int(b * SR):hop]
    ph = np.unwrap(np.angle(c))
    inst = np.diff(ph) / (2 * np.pi * hop / SR)                  # Hz off f
    return 1200 * np.log2((f + inst) / f), 20 * np.log10(np.abs(c[1:]) * 2 + 1e-12)


def report(d):
    ld = lambda n: load(os.path.join(d, n + ".wav"))[:, 0]
    single = ld("e15_pch2_000")
    rs, df = spec(single)
    print(f"== {os.path.basename(d)}")
    print(f"single voice (PCH2 0): fundamental {peak_c(rs, df, F0)[0]:+.1f} c, {peak_c(rs, df, F0)[1]:.1f} dB")
    sp, _ = spec(ld("e15_default"))
    print("default:", " ".join(f"{i:+d} st ({c:+.1f} c, {l:.1f} dB)" for i, c, l in voices(sp, df, rs)) or "one voice")
    for p in ("pch2", "pch3", "pch4"):
        row = []
        for n in sorted(x[:-4] for x in os.listdir(d) if x.startswith(f"e15_{p}_")):
            sp, _ = spec(ld(n))
            v = voices(sp, df, rs, 1)
            row.append(f"{n[-3:]}:" + (f"{v[0][0]:+d}{v[0][1]:+.0f}c" if v and v[0][2] > peak_c(rs, df, F0)[1] - 40 else "off"))
        print(f"{p}: " + " ".join(row))
    print("wave (h1..h12, dB from h1):")
    for n in sorted(x[:-4] for x in os.listdir(d) if x.startswith("e15_wave_") or x.startswith("e15_pw_")):
        sp, _ = spec(ld(n))
        h = [peak_c(sp, df, k * F0, 20)[1] for k in range(1, 13)]
        print(f"  {n[4:]:9s} " + " ".join(f"{v - h[0]:6.1f}" for v in h[1:]) + f"   h1 {h[0]:.1f} dB")
    print("chorus: the fundamental over 0.3..2.9 s (cents: median, spread p5..p95; level dB: spread), its line:")
    for n in sorted(x[:-4] for x in os.listdir(d) if x.startswith("e15_chr")):
        x = ld(n)
        cents, lev = demod(x, F0)
        sp3, df3 = spec(x, 0.3, 2.9, 4)
        lo, hi = int(F0 * 2 ** (-60 / 1200) / df3), int(F0 * 2 ** (60 / 1200) / df3) + 1
        seg = sp3[lo:hi] ** 2
        fr = np.arange(lo, hi) * df3
        cc = 1200 * np.log2(fr / F0)
        mean = np.sum(cc * seg) / np.sum(seg)
        sd = np.sqrt(np.sum((cc - mean) ** 2 * seg) / np.sum(seg))
        lines = cc[(seg > seg.max() * 0.1) & (np.r_[0, seg[1:-1] > seg[:-2], 0] > 0) & (np.r_[0, seg[1:-1] >= seg[2:], 0] > 0)]
        sp1 = np.abs(np.fft.rfft(cents - cents.mean()))
        fm = np.fft.rfftfreq(len(cents), 220 / SR)
        rate = fm[1 + int(np.argmax(sp1[1:]))]
        print(f"  {n[4:]:9s} pitch {np.median(cents):+6.1f} c, p5..p95 {np.percentile(cents, 5):+6.1f}..{np.percentile(cents, 95):+6.1f} c"
              f" (movement {rate:.2f} Hz); level p5..p95 {np.percentile(lev, 5):.1f}..{np.percentile(lev, 95):.1f} dB;"
              f" line spread {sd:.1f} c, lines at {' '.join(f'{v:+.0f}' for v in lines[:6])} c")
    print("tune:", " ".join(f"{n[-3:]}:{peak_c(spec(ld(n))[0], df, F0, 250)[0]:+.1f}c" for n in sorted(x[:-4] for x in os.listdir(d) if x.startswith("e15_tune_"))))


if __name__ == "__main__":
    for d in sys.argv[1:]:
        report(d)
