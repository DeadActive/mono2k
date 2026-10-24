#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The sheet's scorer. Texture features per band per 0.37 s block (flatness, width, movement, flux, peakiness), a
# small model per band trained on the Model:Cycles demos (good) against every bank I rendered (rejected), then each
# candidate scored band by band where it has energy.
#   feat.py train MC_WAV_DIR MODEL.npz MINE_DIR...      feat.py score MODEL.npz CAND_DIR OUT.tsv
import sys, os, glob, wave, numpy as np
from numpy.lib.stride_tricks import sliding_window_view as swv

SR, N, H, B = 44100, 2048, 512, 32
EDGES = [20, 60, 120, 250, 500, 1000, 2000, 4000, 8000]      # nothing above 8 kHz: the demos are MP3s
NB = len(EDGES) - 1
F = np.fft.rfftfreq(N, 1 / SR)
BIDX = [np.where((F >= lo) & (F < hi))[0] for lo, hi in zip(EDGES[:-1], EDGES[1:])]
WIN = np.hanning(N).astype(np.float32)
FEATS = ["flat", "width", "mod", "flux", "peak"]

def load(p, secs=None):
    w = wave.open(p); n = w.getnframes(); ch = w.getnchannels()
    if secs: n = min(n, int(secs * SR))
    x = np.frombuffer(w.readframes(n), dtype='<i2').astype(np.float32).reshape(-1, ch) / 32768
    return np.repeat(x, 2, axis=1) if ch == 1 else x

def power(x):
    fr = swv(x, N)[::H] * WIN
    return (np.abs(np.fft.rfft(fr, axis=1)) ** 2).astype(np.float32)

def blocks(p, secs=None):
    """per block: (NB, 1 + len(FEATS)) [share dB, features], and the block's level in dB"""
    x = load(p, secs)
    if len(x) < N + H * B: return np.zeros((0, NB, 1 + len(FEATS))), np.zeros(0)
    M, S = power((x[:, 0] + x[:, 1]) / 2), power((x[:, 0] - x[:, 1]) / 2)
    nb = len(M) // B
    M, S = M[:nb * B].reshape(nb, B, -1) + 1e-12, S[:nb * B].reshape(nb, B, -1)
    tot = M.sum((1, 2))
    out = np.zeros((nb, NB, 1 + len(FEATS)), np.float32)
    for j, k in enumerate(BIDX):
        m, s = M[:, :, k], S[:, :, k]
        e = m.sum((1, 2))
        out[:, j, 0] = 10 * np.log10(e / tot)
        out[:, j, 1] = (np.exp(np.log(m).mean(2)) / m.mean(2)).mean(1)
        out[:, j, 2] = 10 * np.log10(s.sum((1, 2)) / e + 1e-9)
        out[:, j, 3] = np.log(m.sum(2)).std(1)
        out[:, j, 4] = np.abs(np.diff(np.log(m), axis=1)).mean((1, 2))
        out[:, j, 5] = 10 * np.log10((m.max(2) / m.mean(2)).mean(1))
    lev = 10 * np.log10(tot / (B * N * N / 8) + 1e-12)      # roughly dB re full scale
    return out, lev

def expand(X):                                          # the features, their squares, their pairs
    cols = [X, X ** 2]
    n = X.shape[1]
    cols += [X[:, [i]] * X[:, [j]] for i in range(n) for j in range(i + 1, n)]
    return np.hstack(cols)

def fit(X, y, lam=1.0, it=30):
    w = np.zeros(X.shape[1] + 1)
    A = np.hstack([X, np.ones((len(X), 1))])
    cw = np.where(y == 1, 0.5 / y.mean(), 0.5 / (1 - y.mean()))     # the classes weighed alike
    for _ in range(it):
        p = 1 / (1 + np.exp(-A @ w))
        g = A.T @ (cw * (p - y)) + lam * np.r_[w[:-1], 0]
        Hs = (A * (cw * p * (1 - p))[:, None]).T @ A + lam * np.diag(np.r_[np.ones(len(w) - 1), 0])
        w -= np.linalg.solve(Hs, g)
    return w

def auc(s, y):
    o = np.argsort(s); r = np.empty(len(s)); r[o] = np.arange(1, len(s) + 1)
    n1 = y.sum(); n0 = len(y) - n1
    return (r[y == 1].sum() - n1 * (n1 + 1) / 2) / (n1 * n0)

PRESENT = -22.0                                        # a band counts where it holds this share of the block or more

def gather(files, secs=None):
    D = []
    for p in files:
        b, lev = blocks(p, secs)
        D.append(b[lev > -50])
    return D

def train(mc_dir, model, mine_dirs):
    good = sorted(glob.glob(os.path.join(mc_dir, "*.wav")))
    bad = sorted(sum([glob.glob(os.path.join(d, "a[0-9][0-9]_*.wav")) for d in mine_dirs], []))
    G, Bd = gather(good), gather(bad)
    print(f"demos {len(good)} files {sum(map(len, G))} blocks; mine {len(bad)} files {sum(map(len, Bd))} blocks")
    W, MU, SD, AU = [], [], [], []
    rng = np.random.default_rng(1)
    for j in range(NB):
        def take(D, lab):
            xs = [(d[d[:, j, 0] > PRESENT][:, j, 1:], np.full((d[:, j, 0] > PRESENT).sum(), i)) for i, d in enumerate(D)]
            X = np.vstack([a for a, _ in xs]); g = np.concatenate([b for _, b in xs])
            return X, np.full(len(X), lab), g
        Xg, yg, gg = take(G, 1); Xb, yb, gb = take(Bd, 0)
        X = np.vstack([Xg, Xb]); y = np.r_[yg, yb]; grp = np.r_[gg, gb + 1000]
        mu, sd = X.mean(0), X.std(0) + 1e-9
        Z = expand((X - mu) / sd)
        # held-out check: half the files of each side out
        ids = np.unique(grp); aa = []
        for _ in range(5):
            test = set(rng.choice(ids, len(ids) // 2, replace=False))
            te = np.array([g in test for g in grp])
            w = fit(Z[~te], y[~te])
            aa.append(auc(np.hstack([Z[te], np.ones((te.sum(), 1))]) @ w, y[te]))
        a = float(np.mean(aa))
        w = fit(Z, y)
        W.append(w); MU.append(mu); SD.append(sd); AU.append(a)
        print(f"  band {EDGES[j]:>5}-{EDGES[j+1]:<5} Hz: {len(Xg):5d} good {len(Xb):5d} rejected blocks, held-out AUC {a:.3f} (5 splits {min(aa):.2f}..{max(aa):.2f})")
    np.savez(model, W=np.array(W, dtype=object), MU=np.array(MU), SD=np.array(SD), AU=np.array(AU))

def score_blocks(b, W, MU, SD, AU):
    """the mean logit over the blocks' present bands, weighed by each band's share"""
    num = den = 0.0
    for j in range(NB):
        sel = b[:, j, 0] > PRESENT
        if not sel.any(): continue
        Z = expand((b[sel, j, 1:] - MU[j]) / SD[j])
        lg = np.hstack([Z, np.ones((len(Z), 1))]) @ W[j]
        wt = 10 ** (b[sel, j, 0] / 10) * max(0.0, AU[j] - 0.55)    # a band counts as far as its model told the sides apart
        num += (np.clip(lg, -8, 8) * wt).sum(); den += wt.sum()
    return num / den if den > 1e-9 else 0.0

def score(model, cdir, out):
    z = np.load(model, allow_pickle=True); W, MU, SD, AU = z["W"], z["MU"], z["SD"], z["AU"]
    rows = []
    for p in sorted(glob.glob(os.path.join(cdir, "c*.wav"))):
        b, lev = blocks(p, 4.66)
        keep = lev > -55
        x = load(p, 4.66); rms = 10 * np.log10((x ** 2).mean() + 1e-12); pk = 20 * np.log10(np.abs(x).max() + 1e-9)
        s = score_blocks(b[keep], W, MU, SD, AU) if keep.any() else -99.0
        prof = b[keep][:, :, 0].mean(0) if keep.any() else np.full(NB, -60.0)
        tex = b[keep][:, :, 1:].mean(0).ravel() if keep.any() else np.zeros(NB * len(FEATS))
        rows.append((os.path.basename(p)[1:6], s, rms, pk, prof, tex))
    with open(out, "w") as f:
        for i, s, rms, pk, prof, tex in rows:
            f.write(f"{i}\t{s:.3f}\t{rms:.1f}\t{pk:.1f}\t" + ",".join(f"{v:.1f}" for v in prof) + "\t" +
                    ",".join(f"{v:.3f}" for v in tex) + "\n")
    print(f"scored {len(rows)}")

if __name__ == "__main__":
    if sys.argv[1] == "train": train(sys.argv[2], sys.argv[3], sys.argv[4:])
    elif sys.argv[1] == "score": score(sys.argv[2], sys.argv[3], sys.argv[4])
