#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Refinements of sheet 1's picks: each sound as drawn, A (cleaner: drier sends, tamer resonance, the random swings
# narrowed) and B (A with more body and presence). Every version levelled in the engine (its LEVL, then VOL) to
# -18 LUFS, K-weighted and gated as BS.1770, so a level is the sound's own and the bus's clip is the firmware's.
#   refine.py GEN OUTDIR
import sys, os, subprocess, wave, numpy as np
GEN, OUT = sys.argv[1], sys.argv[2]
P = {n: i for i, n in enumerate("SA SB SC SD SE SF SG SH ATK HOLD DEC REL DIST VOL PAN PORT BASE WDTH HPQ LPQ FATK FDEC "
                                "BOF WOF EQF EQG SRR DTIM DSND DFB DBAS DWID BITS FOLD XOR SPU PTCH LEVL".split())}
P.update({"L1DPTH": 49, "L2DPTH": 57, "L3DPTH": 65, "MODM": 69})
SR = 44100

def kweight(x):
    f = np.fft.rfftfreq(len(x), 1 / SR)
    s = 1j * f
    # the BS.1770 pre-filter as analogue prototypes: a +4 dB shelf at 1682 Hz, a high-pass at 38 Hz (Q 0.5)
    w0, w1 = 1681.97, 38.135
    G = 10 ** (3.99984 / 20)
    shelf = (s ** 2 * G + s * np.sqrt(G) * w0 / 0.70718 + w0 ** 2) / (s ** 2 + s * w0 / 0.70718 + w0 ** 2)
    hp = s ** 2 / (s ** 2 + s * w1 / 0.50033 + w1 ** 2)
    return np.fft.irfft(np.fft.rfft(x, axis=0) * (shelf * hp)[:, None], n=len(x), axis=0)

def lufs(x):
    y = kweight(x)
    b, h = int(0.4 * SR), int(0.1 * SR)
    ms = np.array([(y[i:i + b] ** 2).sum(1).mean() for i in range(0, len(y) - b, h)])
    l = -0.691 + 10 * np.log10(ms + 1e-12)
    g = ms[l > -70]
    if not len(g): return -99.0
    rel = -0.691 + 10 * np.log10(g.mean()) - 10
    g = ms[(l > -70) & (l > rel)]
    return -0.691 + 10 * np.log10(g.mean())

def render(cid, edits, tag):
    d = os.path.join(OUT, tag); os.makedirs(d, exist_ok=True)
    args = [GEN, d, str(cid), "1"] + edits
    subprocess.run(args, check=True)
    w = wave.open(os.path.join(d, f"c{cid:05d}.wav"))
    x = np.frombuffer(w.readframes(w.getnframes()), '<i2').reshape(-1, 2) / 32768
    return x[:int(4.66 * SR)]

def ed(d):
    out = []
    for k, v in d.items():
        if k.startswith("L") and k[1:] in P and isinstance(v, tuple): out.append(f"L{P[k[1:]]}={v[0]}:{v[1]}")
        elif k == "T16": out.append("T16")
        else: out.append(f"{P[k]}={v}")
    return out

def level(cid, edits, tag, target=-18.0):
    """the LEVL (then VOL) that brings the version to the target"""
    lo, hi = 0, 127
    for _ in range(7):
        mid = (lo + hi) // 2
        if lufs(render(cid, edits + [f"{P['LEVL']}={mid}"], tag)) < target: lo = mid + 1
        else: hi = mid
    e = edits + [f"{P['LEVL']}={lo}"]
    l = lufs(render(cid, e, tag))
    vol = 64
    while l < target - 0.5 and vol < 127:
        vol = min(127, vol + 8)
        e = edits + [f"{P['LEVL']}=127", f"{P['VOL']}={vol}"]
        l = lufs(render(cid, e, tag))
    return e, l

# sheet number: (candidate, A's edits, B's extra edits)
R = {
    6:  (3611, {"LDIST": (60, 90), "DIST": 72, "DSND": 94, "MODM": 60}, {"LPQ": 70, "EQG": 64, "WDTH": 95}),
    9:  (3419, {"LBITS": (0, 50), "LHPQ": (0, 60), "EQG": 64}, {"EQF": 80, "EQG": 74, "MODM": 40, "DIST": 80}),
    10: (3580, {"LDSND": (64, 84), "SPU": 20, "HPQ": 60}, {"L1DPTH": 89, "SRR": 20, "DIST": 80}),
    12: (4196, {"T16": 1, "L1DPTH": 39, "L2DPTH": 39, "L3DPTH": 45, "DSND": 90}, {"MODM": 50, "FOLD": 50, "SPU": 25}),
    15: (4514, {"ATK": 40, "HPQ": 50, "LPQ": 80}, {"WOF": 85, "L1DPTH": 95, "T16": 1}),
    18: (1933, {"WDTH": 60, "LSH": (30, 70), "EQG": 64, "DSND": 85}, {"DIST": 84, "LBITS": (0, 20), "LPQ": 60}),
    19: (2324, {"DIST": 88, "LPQ": 60, "LSH": (0, 60)}, {"MODM": 0, "DSND": 64, "L1DPTH": 46}),
    24: (4425, {"SPU": 40, "HPQ": 55, "FOLD": 30}, {"LSB": (40, 100), "EQG": 74, "DIST": 78}),
}
res = []
for n, (cid, a, b) in R.items():
    for tag, d in (("orig", {}), ("A", a), ("B", {**a, **b})):
        e, l = level(cid, ed(d), f"{n:02d}{tag}")
        x = render(cid, e, f"{n:02d}{tag}")
        res.append((n, tag, cid, e, l, 20 * np.log10(np.abs(x).max() + 1e-9)))
        print(f"{n:2d} {tag:4s} {l:6.1f} LUFS  peak {res[-1][5]:5.1f} dBFS  {' '.join(e)}", flush=True)
with open(os.path.join(OUT, "refine.tsv"), "w") as f:
    for n, tag, cid, e, l, pk in res: f.write(f"{n}\t{tag}\t{cid}\t{' '.join(e)}\t{l:.1f}\t{pk:.1f}\n")
