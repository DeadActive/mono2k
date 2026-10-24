#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The sheet's pick from the scored candidates: sane levels, moving (not a static tone), the best third of each
# machine by the score, then 24 as far from each other as they go, three a machine at most, eight held at least.
import sys, numpy as np
NAMES = ["GND-SIN", "GND-PERC", "SID-6581", "FM+STAT", "FM+DYN", "SWAV-SAW", "SWAV-ENS", "DIGI-WAV", "VOX-6", "FM+PAR",
         "SWAV-PUL", "FX-CHORS", "FX-RING", "FX-DYNMX", "GND-NOIS", "DRM-KICK", "DRM-SNAR", "DRM-HATS", "DRM-CYMB"]
sc, kt = sys.argv[1], sys.argv[2]
S = {}
for ln in open(sc):
    i, s, rms, pk, prof, tex = ln.rstrip("\n").split("\t")
    S[int(i)] = (float(s), float(rms), float(pk), np.array(prof.split(","), float), np.array(tex.split(","), float))
K = {}
for ln in open(kt):
    f = ln.rstrip("\n").split("\t")
    K[int(f[0])] = (int(f[1]), int(f[2]), int(f[3]))
ids = sorted(S)
NBD, NF = 8, 5
tex = np.array([S[i][4].reshape(NBD, NF) for i in ids])           # flat width mod flux peak
prof = np.array([S[i][3] for i in ids])
sh = 10 ** (prof / 10); sh /= sh.sum(1, keepdims=True)
motion = (sh * tex[:, :, 2]).sum(1) + (sh * tex[:, :, 3]).sum(1)  # how much the sound moves inside a block
peaky = (sh * tex[:, :, 4]).sum(1)
score = np.array([S[i][0] for i in ids]); rms = np.array([S[i][1] for i in ids]); pk = np.array([S[i][2] for i in ids])
mach = np.array([K[i][0] for i in ids]); held = np.array([K[i][1] for i in ids])
ok = (rms > -40) & (rms < -8) & (pk > -20)
ok &= motion > np.percentile(motion[ok], 20)                       # the static fifth out
ok &= ~((peaky > np.percentile(peaky[ok], 80)) & (motion < np.percentile(motion[ok], 50)))   # static and peaky: toy
print(f"{ok.sum()} of {len(ids)} pass the level, motion and toy filters")
keep = np.zeros(len(ids), bool)
for m in np.unique(mach):
    sel = np.where(ok & (mach == m))[0]
    if len(sel) == 0: continue
    top = sel[np.argsort(-score[sel])][:max(1, len(sel) // 3)]
    keep[top] = True
print(f"{keep.sum()} kept (the best third of each machine by the score)")
X = np.hstack([prof, tex.reshape(len(ids), -1)])
X = (X - X[keep].mean(0)) / (X[keep].std(0) + 1e-9)
pool = list(np.where(keep)[0])
chosen, per = [], {}
for m in np.unique(mach[pool]):                                    # every machine once: its best by the score
    sel = [c for c in pool if mach[c] == m]
    chosen.append(sel[int(np.argmax(score[sel]))]); per[m] = 1
while len(chosen) < 24:
    nheld = held[chosen].sum(); need_held = 8 - nheld; left = 24 - len(chosen)
    cand = [c for c in pool if c not in chosen and per.get(mach[c], 0) < 3 and (need_held < left or held[c])]
    d = np.min(((X[cand][:, None, :] - X[chosen][None, :, :]) ** 2).sum(2), axis=1)
    c = cand[int(np.argmax(d))]
    chosen.append(c); per[mach[c]] = per.get(mach[c], 0) + 1
order = sorted(chosen, key=lambda c: (mach[c] >= 15 or mach[c] == 1, -K[ids[c]][2] if False else K[ids[c]][2]))
for n, c in enumerate(order, 1):
    i = ids[c]
    print(f"{n:2d}\t{i}\t{NAMES[mach[c]]}\t{'held' if held[c] else 'hits'}\tnote {K[i][2]}\tscore {score[c]:+.2f}\trms {rms[c]:.1f}")
