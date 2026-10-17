#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Reads the chain renders (tools/ref/chain.py) of one instrument and prints what each control does: level laws, the
# distortion's curve and harmonics, the filter's cutoffs, slopes and peaks, the EQ's band, the SRR hold rate, the
# delay's echoes.
#   .venv/bin/python tools/ref/chain_anal.py levels|dist|filt|eq|srr|delay DIR [DIR2]
import os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep import load, SR

DB = lambda v: 20 * np.log10(max(v, 1e-12))


def rms(x, a=0.3, b=1.1):
    s = x[int(a * SR):int(b * SR)]
    return DB(np.sqrt(np.mean(s[:, 0] ** 2))), DB(np.sqrt(np.mean(s[:, 1] ** 2)))


def names(d, prefix):
    return sorted(n[:-4] for n in os.listdir(d) if n.startswith(prefix) and n.endswith(".wav"))


def psd(x, a=0.3, b=None, n=8192):
    s = x[int(a * SR):int(b * SR) if b else len(x), 0]
    w = np.hanning(n)
    segs = [s[i:i + n] * w for i in range(0, len(s) - n, n // 2)]
    p = np.mean([np.abs(np.fft.rfft(v)) ** 2 for v in segs], axis=0)
    return np.fft.rfftfreq(n, 1 / SR), p


def bands(f, p, lo=20, hi=20000, per_oct=6):
    edges = lo * 2 ** (np.arange(0, np.log2(hi / lo) * per_oct + 1) / per_oct)
    c, v = [], []
    for e0, e1 in zip(edges[:-1], edges[1:]):
        m = (f >= e0) & (f < e1)
        if m.sum():
            c.append(np.sqrt(e0 * e1)); v.append(p[m].mean())
    return np.array(c), np.array(v)


def response(x, ref, a=0.3, b=None):
    f, p = psd(x, a, b)
    _, pr = psd(ref, a, b)
    c, v = bands(f, p)
    _, vr = bands(f, pr)
    return c, 10 * np.log10(np.maximum(v, 1e-30) / np.maximum(vr, 1e-30))


def crossing(c, r, level, rising):
    """the first frequency where the response crosses level, interpolated in log f"""
    for i in range(len(r) - 1):
        if (rising and r[i] < level <= r[i + 1]) or (not rising and r[i] > level >= r[i + 1]):
            t = (level - r[i]) / (r[i + 1] - r[i])
            return float(np.exp(np.log(c[i]) + t * (np.log(c[i + 1]) - np.log(c[i]))))
    return None


def fund(x, a=0.3, n=65536):
    s = x[int(a * SR):int(a * SR) + n, 0]
    sp = np.abs(np.fft.rfft(s * np.hanning(len(s))))
    k = np.argmax(sp[5:]) + 5
    return k * SR / len(s), sp, len(s)


def harm(x, f0, h=10, a=0.3, n=65536):
    s = x[int(a * SR):int(a * SR) + n, 0]
    sp = np.abs(np.fft.rfft(s * np.hanning(len(s)))) / (len(s) / 4)
    out = []
    for k in range(1, h + 1):
        b = int(round(k * f0 * len(s) / SR))
        out.append(sp[max(b - 4, 0):b + 5].max() if b + 5 < len(sp) else 0)
    return np.array(out)


def curve(x, f0, a=0.3):
    """the memoryless curve: output against the fitted input sine, over whole cycles, as 21 points from -1 to 1"""
    n = int(round(SR / f0 * round(0.4 * f0)))
    s = x[int(a * SR):int(a * SR) + n, 0]
    t = np.arange(n) / SR
    c, d = np.cos(2 * np.pi * f0 * t), np.sin(2 * np.pi * f0 * t)
    A = np.vstack([d, c]).T
    (ks, kc), *_ = np.linalg.lstsq(A, s, rcond=None)
    ph = np.arctan2(kc, ks)
    u = np.sin(2 * np.pi * f0 * t + ph)
    grid = np.linspace(-1, 1, 21)
    return grid, np.array([s[np.abs(u - g) < 0.03].mean() if np.any(np.abs(u - g) < 0.03) else np.nan for g in grid])


def do_levels(d):
    for n in names(d, ""):
        l, r = rms(load(os.path.join(d, n + ".wav")))
        print(f"{n:10s} L {l:7.2f}  R {r:7.2f}")


def do_dist(d):
    for n in names(d, "dist"):
        x = load(os.path.join(d, n + ".wav"))
        f0, _, _ = fund(x)
        hs = harm(x, f0)
        l, _ = rms(x)
        pk = np.abs(x[int(0.3 * SR):int(1.1 * SR), 0]).max()
        thd = DB(np.sqrt(np.sum(hs[1:] ** 2)) / max(hs[0], 1e-12))
        print(f"{n:16s} rms {l:7.2f} peak {DB(pk):6.2f}  h1 {DB(hs[0]):6.1f}  thd {thd:6.1f}  h2..6", " ".join(f"{DB(v / max(hs[0], 1e-12)):5.0f}" for v in hs[1:6]))


def do_curve(d, n):
    x = load(os.path.join(d, n + ".wav"))
    f0, _, _ = fund(x)
    g, y = curve(x, f0)
    print(n, " ".join(f"{v:6.3f}" for v in y))


def do_filt(d):
    for n in names(d, "f_"):
        if n.startswith("f_open") or n.startswith("f_qsin"):
            continue
        note = 60
        if "_n" in n:
            note = int(n.split("_n")[1].split("_")[0])
        ref = load(os.path.join(d, f"f_open_n{note}.wav"))
        c, r = response(load(os.path.join(d, n + ".wav")), ref, 0.3, 3.2)
        k = np.argmax(r)
        hp3 = crossing(c, r, -3, True)
        lp3 = crossing(c[k:], r[k:], -3, False)
        f0 = 440 * 2 ** ((note - 69) / 12)
        def slope(at, rising):
            if at is None:
                return float("nan")
            fa, fb = (at / 4, at / 2) if rising else (at * 2, at * 4)
            ra, rb = np.interp(np.log([fa, fb]), np.log(c), r)
            return (rb - ra) if rising else (ra - rb)
        print(f"{n:18s} peak {r[k]:6.1f} dB at {c[k]:7.0f}  hp-3 {hp3 or 0:7.0f} ({(hp3 or 0) / f0:6.2f} f0) slope {slope(hp3, True):5.1f}"
              f"   lp-3 {lp3 or 0:7.0f} ({(lp3 or 0) / f0:6.2f} f0) slope {slope(lp3, False):5.1f}")
    for n in names(d, "f_qsin"):
        print(f"{n:18s} rms {rms(load(os.path.join(d, n + '.wav')))[0]:7.2f}")


def do_eq(d):
    ref = load(os.path.join(d, "eq_open.wav"))
    for n in names(d, "eq_"):
        if n == "eq_open":
            continue
        c, r = response(load(os.path.join(d, n + ".wav")), ref, 0.3, 3.2)
        k = np.argmax(np.abs(r))
        half = r[k] / 2
        lo = crossing(c[:k + 1], r[:k + 1], half, r[k] > 0)
        hi = crossing(c[k:], r[k:], half, r[k] < 0)
        bw = np.log2(hi / lo) if lo and hi else float("nan")
        print(f"{n:16s} extreme {r[k]:6.1f} dB at {c[k]:7.0f} Hz   half-gain width {bw:4.2f} oct   at 100 Hz {np.interp(np.log(100), np.log(c), r):5.1f}  10 kHz {np.interp(np.log(10000), np.log(c), r):5.1f}")


def do_srr(d):
    for n in names(d, "srr_sin"):
        x = load(os.path.join(d, n + ".wav"))
        f0, sp, N = fund(x)
        f = np.arange(len(sp)) * SR / N
        m = sp.max()
        pk = [i for i in range(2, len(sp) - 2) if sp[i] > sp[i - 1] and sp[i] >= sp[i + 1] and sp[i] > m * 1e-3 and abs(f[i] - f0) > 20]
        pk.sort(key=lambda i: -sp[i])
        rates = []
        for i in pk[:6]:
            rates += [f[i] + f0, f[i] - f0]
        print(f"{n:14s} f0 {f0:7.1f}  images", " ".join(f"{f[i]:7.0f}({DB(sp[i] / m):4.0f})" for i in sorted(pk[:6])))
    ref = load(os.path.join(d, "srr_noi_000.wav"))
    for n in names(d, "srr_noi"):
        c, r = response(load(os.path.join(d, n + ".wav")), ref, 0.3, 2.1)
        print(f"{n:14s}", " ".join(f"{v:5.1f}" for v in r[::3]))
    print("bands", " ".join(f"{v:5.0f}" for v in c[::3]))


def do_delay(d):
    for n in names(d, "d"):
        x = load(os.path.join(d, n + ".wav"))
        e = np.sqrt(np.convolve(x[:, 0] ** 2 + x[:, 1] ** 2, np.ones(220) / 220, "same") / 2)
        el = np.sqrt(np.convolve(x[:, 0] ** 2, np.ones(220) / 220, "same"))
        er = np.sqrt(np.convolve(x[:, 1] ** 2, np.ones(220) / 220, "same"))
        dry = e[:int(0.05 * SR)].max()
        pk = []
        i = int(0.06 * SR)
        while i < len(e) - 1 and len(pk) < 6:
            w = e[i:i + int(0.02 * SR)]
            j = i + int(np.argmax(w))
            if e[j] > dry * 0.01 and e[j] >= e[max(j - 300, 0):j + 300].max():
                pk.append(j)
                i = j + int(0.02 * SR)
            else:
                i += int(0.01 * SR)
        print(f"{n:10s} dry {DB(dry):6.1f}  echoes", "  ".join(f"{p / SR * 1000:6.1f}ms {DB(e[p] / dry):5.1f}dB L{DB(el[p] / dry):5.1f} R{DB(er[p] / dry):5.1f}" for p in pk[:4]))


if __name__ == "__main__":
    what, d = sys.argv[1], sys.argv[2]
    if what == "curve":
        for n in sys.argv[3:]:
            do_curve(d, n)
    else:
        {"levels": do_levels, "dist": do_dist, "filt": do_filt, "eq": do_eq, "srr": do_srr, "delay": do_delay}[what](d)
