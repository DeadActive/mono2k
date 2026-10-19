#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# A pattern's profile, the numbers the owner's reference examples are measured by (the owner's brief v4, 2026-10-19),
# taken the same way on the reference audio (reference/audio/, kept out of the repository) and on MONO2K's renders. Long audio is
# cut into 20 s windows (silent ones skipped) and each number is the median over them.
#   .venv/bin/python tools/ref/profile.py [--json OUT] FILE[:LABEL] ...
# The numbers, per window:
#   bpm, swing      tempo from the onsets' autocorrelation; swing as the off-16th's place in its 8th (50 straight)
#   kick snare hat  hits per bar, from onsets in their bands (40-120 Hz; 1.5-5 kHz with 150-500 Hz; 7-15 kHz)
#   rolls           runs of snare or hat hits faster than 16ths (ratchets, rolls), per bar
#   sub low mid hi  the energy's share in 20-60, 60-250, 250-2000, 2000-16000 Hz
#   width           side against mid, dB (0: as wide as it is centred; -inf: mono)
#   bed             the 250-2000 Hz band's quiet level against its loud one (P10 - P90, dB): near 0, a sustained bed
#   tail            the level 100 ms after an onset against the onset's, dB (closer to 0: longer reverb and echo)
#   bass range      how far the bass's brightness moves (30-150 Hz against 150-600 Hz, P90 - P10, dB), kicks left out
#   bass speed      how fast: its median change from one 16th to the next, dB
#   events          onsets of anything, per bar
#   bar change      how much a bar differs from the bar before, slot by slot (mean dB over 24 bands): a loop is near 0
#   layers          sounds at once, an NMF estimate
# Checked on MONO2K's 16 patterns against their trigs and solo tracks: tempo exact on all 16 (half time included);
# swing within 3 but for one (9 off); kick, snare and hat counts typically 1.5-1.75x off in a full mix (a sustained bass
# hides kicks, a bass's notes pass for them); bass range within 4 dB of the bass alone on 11 of 16, its speed up to 3x
# high where other parts share its harmonics; layers +-0.5 at best. tools/ref/profile_ref.json: the examples' numbers
import json, math, os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep import load, SR

N, HOP = 2048, 512
FR = SR / HOP                                   # frames a second
WIN = 20.0


def db(v):
    return 10 * np.log10(np.maximum(v, 1e-20))


def stft(x):
    w = np.hanning(N).astype(np.float32)
    n = 1 + (len(x) - N) // HOP
    idx = np.arange(N)[None, :] + HOP * np.arange(n)[:, None]
    return np.abs(np.fft.rfft(x[idx] * w, axis=1)).T.astype(np.float32)   # bins x frames


F = np.fft.rfftfreq(N, 1 / SR)


def band(P, lo, hi):
    return P[(F >= lo) & (F < hi)].sum(axis=0)


def peaks(env, thr, mind):
    """frames where env has a local maximum over thr, at least mind frames apart (strongest kept)"""
    c = np.where((env[1:-1] > env[:-2]) & (env[1:-1] >= env[2:]) & (env[1:-1] > thr))[0] + 1
    keep = []
    for i in c[np.argsort(-env[c])]:
        if all(abs(i - j) >= mind for j in keep):
            keep.append(i)
    return np.array(sorted(keep), int)


def band_flux(S, lo, hi):
    """spectral flux in a band: the positive log-magnitude rise summed over its bins"""
    sel = (F >= lo) & (F < hi)
    lg = np.log1p(100 * S[sel])
    fl = np.maximum(0, np.diff(lg, axis=1)).sum(axis=0)
    return np.concatenate([[0], fl])


def adaptive_onsets(fl, mind_s, k=1.5, span_s=0.5):
    """peaks of a flux over its moving median plus k moving MADs (span_s around), at least mind_s apart"""
    h = max(2, int(span_s * FR / 2))
    pad = np.pad(fl, h, mode="edge")
    win = np.lib.stride_tricks.sliding_window_view(pad, 2 * h + 1)
    m = np.median(win, axis=1)
    thr = m + k * np.median(np.abs(win - m[:, None]), axis=1) + 1e-6
    c = np.where((fl[1:-1] > fl[:-2]) & (fl[1:-1] >= fl[2:]) & (fl[1:-1] > thr[1:-1]))[0] + 1
    c = c[fl[c] > 0.08 * np.percentile(fl, 99)]
    mind = max(1, int(mind_s * FR))
    keep = []
    for i in c[np.argsort(-fl[c])]:
        if all(abs(i - j) >= mind for j in keep):
            keep.append(i)
    return np.array(sorted(keep), int)


def flatness(S, i, lo, hi):
    """a frame's spectral flatness in a band: near 1 noise (a drum), near 0 tones (a note)"""
    v = S[(F >= lo) & (F < hi), min(i, S.shape[1] - 1)] ** 2 + 1e-12
    return float(np.exp(np.mean(np.log(v))) / np.mean(v))


def tempo(flux):
    f = flux - flux.mean()
    ac = np.correlate(f, f, "full")[len(f) - 1:]
    ac /= ac[0] + 1e-12
    best, bpm = -1, 120.0
    for b in np.arange(70, 200.5, 0.5):
        lag = 60 * FR / b
        s = 0
        for m, wt in ((1, 1.0), (2, 0.5), (4, 0.25), (0.5, 0.25)):
            l = lag * m
            i = int(l)
            if 1 <= i < len(ac) - 1:
                s += wt * (ac[i] + (ac[i + 1] - ac[i]) * (l - i))
        if s > best:
            best, bpm = s, b
    return bpm


def grid_fit(on, p, phase=None):
    """how many onsets sit within a fifth of a 16th of the 16th grid (its best phase)"""
    if len(on) < 4:
        return 0.0
    best = 0.0
    for phi in np.linspace(0, p, 32, endpoint=False):
        r = ((on - phi) / p) % 1.0
        best = max(best, float(np.mean((r < 0.2) | (r > 0.8))))
    return best


def pick_tempo(t0, flux, on):
    """the autocorrelation's tempo, or the one of its relatives (x2 /2 x1.5 /1.5 x4/3 x3/4, within 70-200) whose 16th
    grid holds clearly more of the onsets: half time read as half the tempo, a triplet feel as two thirds"""
    best, fit0 = t0, grid_fit(on, 60 / t0 / 4 * FR)
    bf = fit0
    for m in (2, 0.5, 1.5, 1 / 1.5, 4 / 3, 3 / 4):
        t = t0 * m
        if 70 <= t <= 200:
            f = grid_fit(on, 60 / t / 4 * FR)
            if f > bf + 0.08:
                best, bf = t, f
    return best


def grid_phase(flux, p):
    k = np.arange(int((len(flux) - 1) / p))
    best, ph = -1, 0.0
    for phi in np.linspace(0, p, 48, endpoint=False):
        i = np.round(phi + k * p).astype(int)
        i = i[i < len(flux)]
        s = flux[i].sum()
        if s > best:
            best, ph = s, phi
    return ph


def nmf_layers(S, k=10, it=150, thr=0.08, seed=1):
    """sounds at once: NMF of a log-frequency magnitude spectrogram; a component counts in a frame when it makes more
    than thr of that frame's reconstruction"""
    edges = np.geomspace(40, 16000, 97)
    M = np.stack([S[(F >= edges[i]) & (F < edges[i + 1])].sum(axis=0) for i in range(96)]) + 1e-9
    rng = np.random.default_rng(seed)
    W = rng.random((96, k)) + 0.1
    H = rng.random((k, M.shape[1])) + 0.1
    for _ in range(it):
        H *= (W.T @ M) / (W.T @ W @ H + 1e-9)
        W *= (M @ H.T) / (W @ H @ H.T + 1e-9)
    C = W.sum(axis=0)[:, None] * H                # each component's share of each frame
    share = C / (C.sum(axis=0, keepdims=True) + 1e-9)
    loud = M.sum(axis=0) > 1e-3 * M.sum(axis=0).max()
    return float(np.mean((share > thr).sum(axis=0)[loud])) if loud.any() else 0.0


def bass_movement(x, bpm, kick):
    """the bass's brightness on its own harmonics: each frame's fundamental in 30-130 Hz (the strongest harmonic sum),
    then harmonics 2..8 against it, dB; kicks left out. Returns how far it moves (P90 - P10) and how fast (median change
    from one 16th to the next)"""
    n4, hop = 4096, HOP
    w = np.hanning(n4).astype(np.float32)
    m = 1 + (len(x) - n4) // hop
    if m < 8:
        return float("nan"), float("nan")
    idx = np.arange(n4)[None, :] + hop * np.arange(m)[:, None]
    S = np.abs(np.fft.rfft(x[idx] * w, axis=1)) ** 2            # frames x bins
    df = SR / n4
    cands = np.arange(30, 130, 1.0)
    def at(f):
        i = np.round(f / df).astype(int)
        return np.maximum(S[:, np.clip(i - 1, 0, S.shape[1] - 1)], np.maximum(S[:, i], S[:, np.clip(i + 1, 0, S.shape[1] - 1)]))
    hs = np.stack([sum(at(c * k) for k in range(1, 5)) for c in cands], axis=1)
    f0 = cands[np.argmax(hs, axis=1)]
    fund = np.empty(m); harm = np.zeros(m)
    for j in range(m):
        def a(f):
            i = int(round(f / df))
            return S[j, max(0, i - 1):i + 2].max()
        fund[j] = a(f0[j])
        harm[j] = sum(a(f0[j] * k) for k in range(2, 9))
    b = 10 * np.log10((harm + 1e-20) / (fund + 1e-20))
    lv = 10 * np.log10(fund + harm + 1e-20)
    mask = lv > np.percentile(lv, 95) - 25
    off = (n4 - N) // (2 * hop)                                  # this window's frames against the 2048 one's
    for i in kick:
        mask[max(0, i - off - 4):max(0, i - off + 5)] = False
    if mask.sum() < 20:
        return float("nan"), float("nan")
    bm = np.where(mask, b, np.nan)
    step = max(1, int(round(60 / bpm / 4 * FR)))
    return (float(np.nanpercentile(bm, 90) - np.nanpercentile(bm, 10)),
            float(np.nanmedian(np.abs(bm[step:] - bm[:-step]))))


def window(y):
    x = y.mean(axis=1).astype(np.float32)
    S = stft(x)
    P = S ** 2
    out = {}
    lg = np.log1p(100 * S)
    flux = np.maximum(0, np.diff(lg, axis=1)).sum(axis=0)
    flux = np.concatenate([[0], flux])
    allon = adaptive_onsets(flux, 0.04)
    bpm = pick_tempo(tempo(flux), flux, allon)
    out["bpm"] = bpm
    p = 60 / bpm / 4 * FR                         # a 16th, in frames
    bar = 16 * p
    nbars = len(flux) / bar
    # swing: the off-16th's place inside each 8th
    ph = grid_phase(flux, p)
    med = np.median(flux) + 1e-9
    ratios = []
    for k8 in range(int((len(flux) - ph) / (2 * p)) - 1):
        a = ph + k8 * 2 * p
        lo, hi = int(a + 0.3 * 2 * p), int(a + 0.75 * 2 * p) + 1
        seg = flux[lo:hi]
        if len(seg) and seg.max() > 3 * med:
            ratios.append((lo + int(np.argmax(seg)) - a) / (2 * p))
    out["swing"] = 100 * float(np.median(ratios)) if ratios else float("nan")
    # drums: a kick is a low onset that falls 6 dB within 150 ms (a bass note holds); a snare a 1.5-5 kHz onset with a
    # 150-600 Hz one beside it; a hat a 7-15 kHz onset
    # (thresholds tuned on MONO2K's 16 patterns against their trigs: typically 1.5-1.75x off in a full mix, a sustained
    # bass hiding kicks and a bass's notes passing for them)
    def on(lo, hi, mind, k, frac):
        fl = band_flux(S, lo, hi)
        o = adaptive_onsets(fl, mind, k=k)
        return o[fl[o] > frac * np.percentile(fl, 99)]
    low = db(band(P, 40, 150))
    kick = np.array([i for i in on(40, 150, 0.07, 4.5, 0.4)
                     if low[i:i + int(0.15 * FR)].min() < low[max(0, i - 1):i + 3].max() - 6], int)
    body = adaptive_onsets(band_flux(S, 150, 600), 0.03, k=1.0)
    sn = np.array([i for i in on(1500, 5000, 0.06, 3.0, 0.4)
                   if len(body) and np.min(np.abs(body - i)) <= 2 and flatness(S, i + 1, 1500, 8000) > 0.15], int)
    hat = on(7000, 15000, 0.025, 3.0, 0.1)
    out["kick"], out["snare"], out["hat"] = len(kick) / nbars, len(sn) / nbars, len(hat) / nbars
    rolls = 0
    for on in (sn, hat):
        short = np.diff(on) < 0.6 * p
        rolls += int(np.sum(short[1:] & ~short[:-1]) + (short[0] if len(short) else 0))
    out["rolls"] = rolls / nbars
    # spectrum, width
    tot = P[(F >= 20) & (F < 16000)].sum()
    for name, lo, hi in (("sub", 20, 60), ("low", 60, 250), ("mid", 250, 2000), ("high", 2000, 16000)):
        out[name] = 100 * P[(F >= lo) & (F < hi)].sum() / tot
    M_ = (y[:, 0] + y[:, 1]) / 2
    S_ = (y[:, 0] - y[:, 1]) / 2
    out["width"] = float(db(np.sum(S_ ** 2) / (np.sum(M_ ** 2) + 1e-20)))
    mid = db(band(P, 250, 2000))
    out["bed"] = float(np.percentile(mid, 10) - np.percentile(mid, 90))
    # tail: in 1-8 kHz (above the bass), 100 ms after each onset that has 110 ms to itself
    L = db(band(P, 1000, 8000))
    d = int(round(0.1 * FR))
    gap = int(round(0.11 * FR))
    drops = [L[i + d] - L[i:i + 3].max() for k, i in enumerate(allon)
             if i + gap < len(L) and (k + 1 == len(allon) or allon[k + 1] - i >= gap)]
    out["tail"] = float(np.median(drops)) if drops else float("nan")
    # bass: how far and how fast its brightness moves
    out["bass_range"], out["bass_speed"] = bass_movement(x, bpm, kick)
    out["events"] = len(allon) / nbars
    # bar change: each 16th slot's spectrum in 24 log bands, against the same slot a bar before
    edges = np.geomspace(40, 16000, 25)
    B = np.stack([band(P, edges[i], edges[i + 1]) for i in range(24)])
    slots = []
    s = ph
    while s + p < B.shape[1]:
        slots.append(db(B[:, int(s):max(int(s) + 1, int(s + p))].mean(axis=1)))
        s += p
    slots = np.array(slots)
    if len(slots) > 16:
        diff = np.abs(slots[16:] - slots[:-16])
        loud = np.maximum(slots[16:], slots[:-16]) > slots.max() - 60
        out["bar_change"] = float(np.mean(diff[loud]))
    else:
        out["bar_change"] = float("nan")
    out["layers"] = nmf_layers(S)
    out["rms"] = float(10 * np.log10(np.mean(x ** 2) + 1e-20))
    return out


def profile(path):
    y = load(path)
    if y.ndim == 1:
        y = np.stack([y, y], axis=1)
    n = int(WIN * SR)
    rows = []
    starts = range(0, max(1, len(y) - n // 2), n) if len(y) > n else [0]
    for a in starts:
        seg = y[a:a + n]
        if len(seg) < 4 * SR:
            continue
        if 10 * np.log10(np.mean(seg ** 2) + 1e-20) < -40:
            continue
        rows.append(window(seg))
    keys = rows[0].keys()
    med = {k: float(np.nanmedian([r[k] for r in rows])) for k in keys}
    med["windows"] = len(rows)
    med["bpm_lo"] = float(min(r["bpm"] for r in rows))
    med["bpm_hi"] = float(max(r["bpm"] for r in rows))
    return med


COLS = [("bpm", "BPM", "%5.0f"), ("swing", "swing", "%5.0f"), ("kick", "kick", "%5.1f"), ("snare", "snare", "%5.1f"),
        ("hat", "hat", "%5.1f"), ("rolls", "rolls", "%5.2f"), ("sub", "sub%", "%5.1f"), ("low", "low%", "%5.1f"),
        ("mid", "mid%", "%5.1f"), ("high", "high%", "%5.1f"), ("width", "width", "%6.1f"), ("bed", "bed", "%6.1f"),
        ("tail", "tail", "%6.1f"), ("bass_range", "b.rng", "%6.1f"), ("bass_speed", "b.spd", "%6.1f"),
        ("events", "events", "%6.1f"), ("bar_change", "barchg", "%6.1f"), ("layers", "layers", "%6.1f")]


def main():
    args = sys.argv[1:]
    out_json = None
    if args[:1] == ["--json"]:
        out_json, args = args[1], args[2:]
    res = {}
    print("%-14s" % "" + " ".join("%6s" % h for _, h, _ in COLS))
    for a in args:
        path, _, lab = a.partition(":")
        lab = lab or os.path.basename(path)
        r = profile(path)
        res[lab] = r
        print("%-14s" % lab[:14] + " ".join("%6s" % (f % r[k]) for k, _, f in COLS))
    if out_json:
        json.dump(res, open(out_json, "w"), indent=1)


if __name__ == "__main__":
    main()
