#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The owner's 64 waves audited, as they are (nothing is changed here): for each file its format, its length, DC offset,
# peak and RMS, the jump across the loop point (last sample to first) against the wave's typical step, and whether
# it is one cycle: by autocorrelation, not by its length. A wave that repeats within itself shows a strong peak at a
# lag that divides it (circular: k cycles in the file); one cycle shows none. A jump at the loop can be the wave's own
# (a saw's reset), so it is reported, never smoothed.
#   .venv/bin/python tools/wave_audit.py assets/waves/mono2k64 [--md OUT.md]
import os, re, struct, sys, wave
import numpy as np


def read(path):
    w = wave.open(path, "rb")
    ch, bits, rate, n = w.getnchannels(), w.getsampwidth() * 8, w.getframerate(), w.getnframes()
    raw = w.readframes(n)
    w.close()
    if bits == 16:
        x = np.frombuffer(raw, "<i2").astype(float) / 32768
    elif bits == 24:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3)
        x = ((b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) | (b[:, 2].astype(np.int32) << 16)) << 8 >> 8) / 8388608
    elif bits == 8:
        x = (np.frombuffer(raw, np.uint8).astype(float) - 128) / 128
    else:
        raise ValueError(f"{bits}-bit")
    return x.reshape(-1, ch)[:, 0], ch, bits, rate, n


def cycles(x):
    """(whole cycles in the file, the energy share they hold): the largest m whose harmonics (bins m, 2m, ...) hold
    99.5 % of the energy over DC. A smooth wave correlates with itself at a small shift anyway, so a lag's
    correlation alone says nothing; the spectrum does"""
    y = x - x.mean()
    e = np.abs(np.fft.rfft(y))[1:] ** 2
    tot = e.sum()
    if tot <= 0:
        return 0, 0.0
    best = (1, 1.0)
    for m in range(2, len(e) // 2 + 1):
        share = e[m - 1::m].sum() / tot
        if share >= 0.995:
            best = (m, float(share))
    return best


def repeat_inside(x):
    """a repeat that is not a whole number of cycles: the normalized correlation of the wave against itself shifted
    by a lag (over the overlap, at least a quarter of the file), its best peak from 1/16 of the length to 3/4, if it
    stands over 0.97"""
    n = len(x)
    y = x - x.mean()
    best = (None, 0.0)
    rs = []
    for l in range(max(4, n // 16), int(0.75 * n) + 1):
        a, b = y[:n - l], y[l:]
        den = np.sqrt(np.dot(a, a) * np.dot(b, b))
        rs.append((l, float(np.dot(a, b) / den) if den > 0 else 0.0))
    for i in range(1, len(rs) - 1):
        l, r = rs[i]
        if r >= rs[i - 1][1] and r >= rs[i + 1][1] and r > best[1]:
            best = (l, r)
    return best if best[1] > 0.97 else (None, best[1])


def audit(path):
    x, ch, bits, rate, n = read(path)
    peak = np.abs(x).max()
    step = np.median(np.abs(np.diff(x))) + 1e-12
    jump = abs(x[0] - x[-1])
    k, kv = cycles(x)
    lag, lv = repeat_inside(x)
    one = k == 1 and lag is None
    return dict(file=os.path.basename(path), channels=ch, bits=bits, rate=rate, length=n,
                dc=float(x.mean()), dc_rel=float(x.mean() / peak) if peak else 0.0,
                peak_db=float(20 * np.log10(peak + 1e-12)), rms_db=float(20 * np.log10(np.sqrt(np.mean(x ** 2)) + 1e-12)),
                jump=float(jump), jump_steps=float(jump / step), cycles=k, cycles_corr=kv,
                inner_period=lag, inner_corr=lv, one_cycle=bool(one))


def order(name):
    m = re.search(r"(\d+)", name)
    return int(m.group(1)) if m else 0


if __name__ == "__main__":
    d = sys.argv[1]
    files = sorted((f for f in os.listdir(d) if f.lower().endswith(".wav")), key=order)
    nums = [order(f) for f in files]
    gaps = sorted(set(range(1, max(nums) + 1)) - set(nums)) if nums else []
    rows = [audit(os.path.join(d, f)) for f in files]
    lines = [f"{len(files)} files, numbered {min(nums)}..{max(nums)}; gaps: {gaps if gaps else 'none'}", "",
             "| File | Format | Length | DC (of peak) | Peak | RMS | Loop jump (steps) | Cycles | One cycle |",
             "|---|---|---|---|---|---|---|---|---|"]
    for r in rows:
        cyc = f"{r['cycles']}" + (f" (repeats at {r['inner_period']}, r {r['inner_corr']:.2f})" if r['inner_period'] else "")
        lines.append(f"| {r['file']} | {r['bits']}-bit {'mono' if r['channels'] == 1 else str(r['channels']) + ' ch'} {r['rate']} Hz | "
                     f"{r['length']} | {r['dc_rel'] * 100:+.1f} % | {r['peak_db']:.1f} dBFS | {r['rms_db']:.1f} dBFS | "
                     f"{r['jump']:.3f} ({r['jump_steps']:.0f}) | {cyc} | {'yes' if r['one_cycle'] else 'NO'} |")
    text = "\n".join(lines)
    print(text)
    if "--md" in sys.argv:
        with open(sys.argv[sys.argv.index("--md") + 1], "w") as f:
            f.write(text + "\n")
