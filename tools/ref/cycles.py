#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# One cycle of each machine, the Monomachine's against MONO2K's: the steady part of each C4 cut into periods,
# averaged, both shifted to start where the fundamental rises through zero, scaled to the same peak; under it the
# first 16 harmonics of each, in dB from the strongest. The waveform match, made visible.
#   .venv/bin/python tools/ref/cycles.py build/ref/mm build/ref/m2k build/ref/cycles.png
import os, sys, wave
import numpy as np
from PIL import Image, ImageDraw

SR = 44100


def load(p):
    with wave.open(p) as w:
        x = np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(np.float64) / 32768
    return x.reshape(-1, 2)[:, 0]


def cycle(x, f0=261.6256):
    seg = x[int(0.15 * SR):int(0.45 * SR)]
    seg = seg - seg.mean()
    period = SR / f0
    n = int(len(seg) / period) - 1
    grid = np.arange(256) * period / 256
    cyc = np.mean([np.interp(k * period + grid, np.arange(len(seg)), seg) for k in range(n)], axis=0)
    spec = np.fft.rfft(cyc)
    ph = np.angle(spec[1])
    shift = int(round((-ph - np.pi / 2) / (2 * np.pi) * 256)) % 256       # the fundamental: a sine from zero
    cyc = np.roll(cyc, -shift)
    pk = np.max(np.abs(cyc)) or 1
    h = np.abs(np.fft.rfft(cyc))[1:17]
    return cyc / pk, 20 * np.log10(np.maximum(h / (h.max() or 1), 1e-6))


def main():
    a_dir, b_dir, out = sys.argv[1:4]
    names = sorted(n[:-4] for n in os.listdir(a_dir) if n.endswith(".wav") and os.path.exists(os.path.join(b_dir, n)))
    W, H, pad = 360, 210, 10
    cols = 3
    img = Image.new("RGB", (cols * (W + pad) + pad, ((len(names) + cols - 1) // cols) * (H + pad) + pad), "white")
    d = ImageDraw.Draw(img)
    for i, n in enumerate(names):
        x0, y0 = pad + (i % cols) * (W + pad), pad + (i // cols) * (H + pad)
        d.rectangle([x0, y0, x0 + W, y0 + H], outline="#bbb")
        d.text((x0 + 6, y0 + 4), n + "   black: Monomachine   red: MONO2K", fill="black")
        top, wh = y0 + 20, 100
        d.line([(x0, top + wh / 2), (x0 + W, top + wh / 2)], fill="#ddd")
        for k, (src, col) in enumerate(((a_dir, "black"), (b_dir, "red"))):
            cyc, harm = cycle(load(os.path.join(src, n + ".wav")))
            d.line([(x0 + j * W / 256, top + wh / 2 - v * wh / 2 * 0.95) for j, v in enumerate(cyc)], fill=col, width=2 if k == 0 else 1)
            by = top + wh + 10
            for j, v in enumerate(harm):
                bx = x0 + 8 + j * 21 + k * 9
                hh = max(0, (v + 60)) * 70 / 60
                d.rectangle([bx, by + 70 - hh, bx + 7, by + 70], fill=col)
    img.save(out)
    print("sheet", out)


main()
