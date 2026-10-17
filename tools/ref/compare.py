#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Compares MONO2K's renders with the Monomachine's, note by note: level, pitch, brightness, envelope and the first
# harmonics. Prints a table and draws each pair's envelope and spectrum in one sheet.
#   .venv/bin/python tools/ref/compare.py build/ref/mm build/ref/m2k build/ref/compare.png
import os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep import load as load2
from PIL import Image, ImageDraw

SR = 44100


def load(path):
    return load2(path).mean(1)


def db(v):
    return 20 * np.log10(max(v, 1e-9))


def envelope(x, hop=441):
    n = len(x) // hop
    return np.array([np.sqrt(np.mean(x[i * hop:(i + 1) * hop] ** 2)) for i in range(n)])


def f0(x):
    seg = x[int(0.3 * SR):int(0.3 * SR) + 8192]
    seg = seg - seg.mean()
    if np.max(np.abs(seg)) < 1e-4:
        return 0.0
    ac = np.correlate(seg, seg, "full")[len(seg) - 1:]
    lo, hi = int(SR / 2000), int(SR / 40)
    k = lo + int(np.argmax(ac[lo:hi]))
    return SR / k


def spectrum(x):
    seg = x[int(0.25 * SR):int(0.25 * SR) + 16384]
    seg = seg * np.hanning(len(seg))
    return np.abs(np.fft.rfft(seg)) / len(seg)


def harmonics(sp, f, n=12):
    out = []
    for h in range(1, n + 1):
        b = int(round(h * f * 16384 / SR))
        if b + 3 >= len(sp):
            out.append(-120)
            continue
        out.append(db(sp[b - 3:b + 4].max()))
    top = max(out)
    return [v - top for v in out]


def describe(x):
    env = envelope(x)
    peak_i = int(np.argmax(env))
    pk = env[peak_i]
    held = x[int(0.05 * SR):int(1.45 * SR)]
    sp = spectrum(x)
    freqs = np.arange(len(sp)) * SR / 16384
    centroid = float((sp * freqs).sum() / max(sp.sum(), 1e-12))
    rel = env[150:]                                   # after the note-off at 1.5 s (hop 10 ms)
    tail = next((i for i, v in enumerate(rel) if v < pk * 0.01), len(rel)) * 10
    pitch = f0(x)
    return {
        "peak": db(np.max(np.abs(x))), "rms": db(np.sqrt(np.mean(held ** 2))), "f0": pitch,
        "centroid": centroid, "attack_ms": peak_i * 10, "at_0.5s": db(env[50] / pk) if pk else -120,
        "at_1.4s": db(env[140] / pk) if pk else -120, "release_ms": tail,
        "harm": harmonics(sp, pitch) if pitch else None, "env": env, "sp": sp,
    }


def main():
    mm_dir, m2k_dir, out = sys.argv[1:4]
    names = sorted(n[:-4] for n in os.listdir(mm_dir) if n.endswith(".wav") and os.path.exists(os.path.join(m2k_dir, n)))
    rows = []
    print("%-10s %-4s %7s %7s %8s %9s %7s %7s %7s %8s" % ("", "", "peak", "rms", "f0", "centroid", "atk", "@0.5s", "@1.4s", "release"))
    for n in names:
        a, b = describe(load(os.path.join(mm_dir, n + ".wav"))), describe(load(os.path.join(m2k_dir, n + ".wav")))
        rows.append((n, a, b))
        for tag, d in (("MM", a), ("M2K", b)):
            print("%-10s %-4s %7.1f %7.1f %8.1f %9.0f %7d %7.1f %7.1f %8d" % (n if tag == "MM" else "", tag, d["peak"], d["rms"], d["f0"],
                  d["centroid"], d["attack_ms"], d["at_0.5s"], d["at_1.4s"], d["release_ms"]))
        if a["harm"] and b["harm"]:
            print("%-10s harm MM  " % "" + " ".join("%4.0f" % v for v in a["harm"]))
            print("%-10s harm M2K " % "" + " ".join("%4.0f" % v for v in b["harm"]))
    # the sheet: per machine, the two envelopes (dB, 2.5 s) and the two spectra (dB, 20 Hz..20 kHz, log)
    W, H, pad = 900, 150, 8
    img = Image.new("RGB", (W, len(rows) * (H + 24) + pad), "white")
    d = ImageDraw.Draw(img)
    for r, (n, a, b) in enumerate(rows):
        y0 = pad + r * (H + 24)
        d.text((pad, y0), n + "   black: Monomachine   red: MONO2K", fill="black")
        y0 += 14
        ew, sw = 300, W - 300 - 3 * pad
        d.rectangle([pad, y0, pad + ew, y0 + H], outline="#999")
        d.rectangle([2 * pad + ew, y0, 2 * pad + ew + sw, y0 + H], outline="#999")
        for dd, col in ((a, "black"), (b, "red")):
            env = 20 * np.log10(np.maximum(dd["env"], 1e-6))
            pts = [(pad + i * ew / len(env), y0 + H * (1 - (max(v, -72) + 72) / 72)) for i, v in enumerate(env)]
            d.line(pts, fill=col, width=1)
            sp = 20 * np.log10(np.maximum(dd["sp"], 1e-9))
            fr = np.arange(len(sp)) * SR / 16384
            sel = (fr > 20) & (fr < 20000)
            xs = 2 * pad + ew + (np.log10(fr[sel]) - np.log10(20)) / 3 * sw
            ys = y0 + H * (1 - (np.clip(sp[sel], -120, -10) + 120) / 110)
            d.line(list(zip(xs, ys)), fill=col, width=1)
    img.save(out)
    print("sheet", out)


main()
