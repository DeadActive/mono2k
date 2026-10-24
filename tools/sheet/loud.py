# SPDX-License-Identifier: GPL-3.0-only
# BS.1770 loudness: K-weighting as analogue prototypes in the frequency domain, 400 ms blocks, the two gates
import numpy as np
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

