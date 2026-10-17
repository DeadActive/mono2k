#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# SWAVE-ENS, round two (tools/ref/chain.py ens16):
#   pch      every PCH2 value: the second voice's interval in cents (or OFF) and its level against the root's
#   pw       the phase between the two saws a voice is made of, from its harmonics: at WAVE 127 their difference,
#            |sin(pi k d)|; at WAVE 0 their sum, |cos(pi k d)|; d in periods, fitted on h1..h10
#   chorus   the copies a voice is made into: the lines around partials 4, 8, 12 over 0.3..4 s (cents, dB from the
#            strongest), in each half of the note; the same cents on every partial means a fixed detune, cents
#            growing with the partial a movement
#   .venv/bin/python tools/ref/ens16.py build/ref/mm_cens16 [--json OUT]
import json, os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep import load, SR
from ens15 import F0, spec, peak_c, voices


def pch_table(d):
    ld = lambda n: load(os.path.join(d, n + ".wav"))[:, 0]
    rs, df = spec(ld("e16_pch2_000"))
    root = peak_c(rs, df, F0)[1]
    out = {}
    for v in range(128):
        sp, _ = spec(ld(f"e16_pch2_{v:03d}"))
        vs = voices(sp, df, rs, 1)
        if not vs or vs[0][2] < root - 40:
            out[v] = None
            continue
        i, c, lev = vs[0]
        # the root's own level in this render, for the voice's against it
        out[v] = (round(100 * i + c, 1), round(lev - peak_c(sp, df, F0)[1], 2))
    return out


def pw_fit(d):
    ld = lambda n: load(os.path.join(d, n + ".wav"))[:, 0]
    out = {}
    for w, form in ((127, np.sin), (0, np.cos)):
        for n in sorted(x[:-4] for x in os.listdir(d) if x.startswith(f"e16_pw_w{w:03d}_")):
            sp, df = spec(ld(n))
            h = np.array([10 ** (peak_c(sp, df, k * F0, 20)[1] / 20) * k for k in range(1, 11)])   # x k: undo the saw's 1/k
            best = None
            for dd in np.linspace(0, 1, 2001):
                m = np.abs(form(np.pi * np.arange(1, 11) * dd))
                a = np.dot(m, h) / max(np.dot(m, m), 1e-12)
                e = np.sum((a * m - h) ** 2) / np.sum(h ** 2)
                if best is None or e < best[0]:
                    best = (e, dd)
            out[n] = (round(best[1], 4), round(best[0], 4))
    return out


def lines(sp, df, f, span=40, floor_db=-25):
    lo, hi = int(f * 2 ** (-span / 1200) / df), int(f * 2 ** (span / 1200) / df) + 1
    seg = sp[lo:hi]
    db = 20 * np.log10(seg / seg.max() + 1e-12)
    pk = [i for i in range(1, len(seg) - 1) if seg[i] > seg[i - 1] and seg[i] >= seg[i + 1] and db[i] > floor_db]
    out = []
    for i in pk:
        a, b, c = np.log(seg[i - 1] + 1e-15), np.log(seg[i] + 1e-15), np.log(seg[i + 1] + 1e-15)
        dd = 0.5 * (a - c) / (a - 2 * b + c) if a - 2 * b + c else 0
        out.append((round(1200 * np.log2((lo + i + dd) * df / f), 1), round(float(db[i]), 1)))
    return out


def chorus(d):
    ld = lambda n: load(os.path.join(d, n + ".wav"))[:, 0]
    out = {}
    for n in sorted(x[:-4] for x in os.listdir(d) if x.startswith("e16_chr")):
        x = ld(n)
        r = {}
        for part, (a, b) in (("all", (0.3, 4.0)), ("first", (0.3, 2.15)), ("second", (2.15, 4.0))):
            sp, df = spec(x, a, b, 4)
            r[part] = {k: lines(sp, df, k * F0) for k in (4, 8, 12)}
        out[n] = r
    return out


if __name__ == "__main__":
    d = sys.argv[1]
    pt = pch_table(d)
    print("PCH2 -> the second voice (cents; dB against the root):")
    for v in range(128):
        print(f"  {v:3d}: " + ("OFF" if pt[v] is None else f"{pt[v][0]:+8.1f} c  {pt[v][1]:+5.2f} dB"))
    pw = pw_fit(d)
    print("PW -> the phase between the saws (periods; fit error):")
    print("  " + "  ".join(f"{k[-8:]}:{v[0]:.4f}({v[1]:.3f})" for k, v in pw.items()))
    ch = chorus(d)
    print("chorus lines (cents, dB) at partials 4 / 8 / 12, whole note:")
    for n, r in ch.items():
        print(f"  {n[4:]:10s} " + " | ".join(" ".join(f"{c:+.1f}({l:.0f})" for c, l in r["all"][k]) for k in (4, 8, 12)))
    print("chorus, partial 8, first half against second:")
    for n, r in ch.items():
        print(f"  {n[4:]:10s} " + " ".join(f"{c:+.1f}({l:.0f})" for c, l in r["first"][8]) + "  ||  " +
              " ".join(f"{c:+.1f}({l:.0f})" for c, l in r["second"][8]))
    if "--json" in sys.argv:
        with open(sys.argv[sys.argv.index("--json") + 1], "w") as f:
            json.dump({"pch": pt, "pw": pw, "chorus": ch}, f, indent=1)
