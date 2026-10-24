#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Sheet 3: instruments. ~120 random instruments of each kind (mus.h) heard playing music: pads and stabs on chords
# (SWAV-ENS's four voices in one track, the others stacked on three tracks), keys and leads on lines. Kept: the ones
# whose energy sits on the harmonics of the notes they play (an effect's does not), then picked for variety, each at
# -18 LUFS.   mus.py MEL OUTDIR N_PER_KIND
import sys, os, subprocess, random, wave, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from loud import lufs                                        # noqa: E402
MEL, OUT, NPER = sys.argv[1], sys.argv[2], int(sys.argv[3])
os.makedirs(OUT, exist_ok=True)
SR, BPM = 44100, 103
STEP = 60 / BPM / 4
SC = {"aeolian": [0, 2, 3, 5, 7, 8, 10], "dorian": [0, 2, 3, 5, 7, 9, 10], "phrygian": [0, 1, 3, 5, 7, 8, 10],
      "harmonic minor": [0, 2, 3, 5, 7, 8, 11], "lydian": [0, 2, 4, 6, 7, 9, 11]}
SCW = {"aeolian": 3, "dorian": 3, "phrygian": 2, "harmonic minor": 1, "lydian": 1}
NN = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
KIND = ["pad", "stab", "keys", "lead"]
MNAME = {0: "GND-SIN", 2: "SID-6581", 3: "FM+STAT", 4: "FM+DYN", 5: "SWAV-SAW", 6: "SWAV-ENS", 7: "DIGI-WAV", 8: "VOX-6",
         9: "FM+PAR", 10: "SWAV-PUL"}
M_ENS = 6

def machine_of(kind, cid):
    """the machine mus.h draws for (kind, id): read back from a one-line render's kit dump would cost a render; the
    C side prints nothing, so ask it with a tiny score and the 'probe' trick: render nothing, read the kit via kits"""
    spec = os.path.join(OUT, "probe.txt")
    open(spec, "w").write(f"mtrack 0 {kind} {cid}\nprobe {os.path.join(OUT, 'probe.out')}\n")
    subprocess.run([MEL, spec], check=True)
    return int(open(os.path.join(OUT, "probe.out")).read().split()[0])

def pitch(root, sc, d): return root + sc[d % len(sc)] + 12 * (d // len(sc))

def euclid(k, n, rot): return [((i - rot) % n * k) % n < k for i in range(n)]

def lead_voices(prev, ch):
    if prev is None: return sorted(ch)
    return sorted(min((p + 12 * o for o in range(-2, 3)), key=lambda x: abs(x - q)) for p, q in zip(sorted(ch), prev))

def phrase(kind, m, rng):
    """the score lines, the segments [(t0, t1, notes)], a description"""
    sc_name = rng.choices(list(SCW), weights=list(SCW.values()))[0]; sc = SC[sc_name]
    key = rng.randrange(12)
    lines, segs = [], []
    if kind in (0, 1):
        chordal = m == M_ENS
        if kind == 0: k = rng.choice([2, 4]); bars = [2, 2] if k == 2 else [1, 1, 1, 1]
        else: k = rng.choice([1, 2]); bars = [4] if k == 1 else [2, 2]
        prog = [0 if rng.random() < 0.6 else rng.randrange(7)]
        while len(prog) < k:
            c = rng.randrange(7)
            if c != prog[-1]: prog.append(c)
        if chordal: shape = rng.choice([[0, 2, 4, 6], [0, 2, 4, 8], [0, 3, 6, 9], [0, 2, 6, 10]])
        else: shape = rng.choice([[0, 2, 6], [0, 4, 8], [0, 3, 6], [0, 2, 4]])
        lo = 45 if kind == 0 else 47
        root = lo + ((key - lo) % 12)
        hits = [True] + [False] * 15 if kind == 0 else euclid(rng.randint(3, 5), 16, rng.randint(1, 3))
        nt = 1 if chordal else 3
        prev, step, names = None, 0, []
        for c, b in zip(prog, bars):
            ch = lead_voices(prev, [pitch(root, sc, c + i) for i in shape]); prev = ch
            names.append("-".join(NN[p % 12] for p in ch))
            for s in range(step, step + 16 * b):
                if not hits[s % 16]: continue
                if chordal:
                    bass = ch[0]; iv = []
                    for p in ch[1:]:
                        d = p - bass
                        while d > 12: d -= 12
                        iv.append(d if d else 12)
                    lines += [f"trig 0 {s} {bass}", f"ens 0 {s} {iv[0]} {iv[1]} {iv[2]}"]
                    segs.append((s, None, [bass] + [bass + d for d in iv]))
                else:
                    for t, p in enumerate(ch): lines.append(f"trig {t} {s} {p}")
                    segs.append((s, None, ch))
            step += 16 * b
        desc = f"{sc_name} on {NN[root % 12]}: " + " | ".join(names)
    else:
        nt = 1
        L = rng.choice([4, 5, 6]) if kind == 2 else rng.choice([3, 4, 5])
        while True:                                     # a line, not a note repeated: three notes at least
            d = rng.choice([0, 2, 4]); mot = [d]
            for _ in range(L - 1):
                st = rng.choice([-2, 2, -3, 3, 4]) if rng.random() < 0.12 else rng.choice([-1, 1, -1, 1, -2, 2, 0])
                d = max(-3, min(9, d + st)); mot.append(d)
            if len(set(mot)) >= 3: break
        lo = 55 if kind == 2 else 57
        root = lo + ((key - lo) % 12)
        hits = euclid(rng.randint(4, 7), 16, rng.randrange(16)) if kind == 2 else euclid(rng.randint(2, 4), 16, rng.randrange(16))
        j = 0
        for s in range(64):
            if not hits[s % 16]: continue
            p = pitch(root, sc, mot[j % L]); j += 1
            sl = " s" if kind == 3 and rng.random() < 0.3 else ""
            lines.append(f"trig 0 {s} {p}{sl}"); segs.append((s, None, [p]))
        desc = f"{sc_name} on {NN[root % 12]}: a {L}-note line (" + " ".join(NN[pitch(root, sc, x) % 12] for x in mot) + ")"
    segs = [(s, segs[i + 1][0] if i + 1 < len(segs) else 64, n) for i, (s, _, n) in enumerate(segs)]
    return lines, segs, nt, desc

def harmonicity(x, segs):
    m = x.mean(1); num = den = 0.0
    for s0, s1, notes in segs:
        a = int((s0 * STEP + 0.05) * SR); b = int(min(s1 * STEP, s0 * STEP + 0.8) * SR)
        if b - a < int(0.1 * SR): continue
        seg = m[a:b] * np.hanning(b - a)
        X = np.abs(np.fft.rfft(seg, 16384)) ** 2; f = np.fft.rfftfreq(16384, 1 / SR)
        band = (f >= 80) & (f <= 5000)
        mask = np.zeros(len(f), bool)
        for n in notes:
            f0 = 440 * 2 ** ((n - 69) / 12) / 2
            for h in np.arange(f0, 5000, f0):
                mask |= np.abs(f - h) <= max(4, 0.015 * h)
        e = X[band].sum()
        if e <= 0: continue
        share = X[band & mask].sum() / e; cov = (band & mask).sum() / band.sum()
        num += e * (share - cov) / (1 - cov + 1e-9); den += e
    return num / den if den else 0.0

def render(lines, path, trim=100, vol=None, nt=1):
    spec = os.path.join(OUT, "spec.txt")
    vl = "".join(f"\nkit {t} 13 {vol}" for t in range(nt)) if vol else ""
    open(spec, "w").write("bpm 103\nlen 64\nsecs 9.9\n" + "\n".join(lines) + f"\ntrim {trim}{vl}\nout {path}\n")
    subprocess.run([MEL, spec], check=True)
    w = wave.open(path); return np.frombuffer(w.readframes(w.getnframes()), '<i2').reshape(-1, 2) / 32768

rows = []
for kind in range(4):
    for i in range(NPER):
        cid = 1000 * kind + i
        rng = random.Random(cid * 7919 + 13)
        m = machine_of(kind, cid)
        lines, segs, nt, desc = phrase(kind, m, rng)
        head = [f"mtrack {t} {kind} {cid} 14={[34, 64, 94][t] if nt == 3 else 64}" for t in range(nt)]
        x = render(head + lines, os.path.join(OUT, "cand.wav"), nt=nt)
        l = lufs(x); h = harmonicity(x, segs)
        mm = x.mean(1); S = np.abs(np.fft.rfft(mm)) ** 2; f = np.fft.rfftfreq(len(mm), 1 / SR)
        cen = (S * f).sum() / (S.sum() + 1e-12)
        side = 10 * np.log10(((x[:, 0] - x[:, 1]) ** 2).mean() / (((x[:, 0] + x[:, 1]) ** 2).mean() + 1e-12) + 1e-12)
        rows.append(dict(kind=kind, cid=cid, m=m, nt=nt, lines=head + lines, desc=desc, lufs=l, h=h, cen=cen, side=side))
    print(f"{KIND[kind]}: {NPER} drawn", flush=True)

QUOTA = {0: 7, 1: 6, 2: 6, 3: 5}
CAP = {0: {M_ENS: 3}, 1: {M_ENS: 4}}
picks = []
for kind in range(4):
    R = [r for r in rows if r["kind"] == kind and r["lufs"] > -45 and r["cen"] < 3000]
    hs = np.array([r["h"] for r in R]); thr = np.percentile(hs, 50)
    R = [r for r in R if r["h"] >= thr]
    print(f"{KIND[kind]}: {len(R)} tonal enough (harmonicity >= {thr:.2f})")
    F = np.array([[np.log(r["cen"]), r["side"] / 10, r["h"] * 3] for r in R])
    F = (F - F.mean(0)) / (F.std(0) + 1e-9)
    chosen, per = [int(np.argmax([r["h"] for r in R]))], {}
    per[R[chosen[0]]["m"]] = 1
    while len(chosen) < QUOTA[kind]:
        best, bd = None, -1
        for j, r in enumerate(R):
            if j in chosen or per.get(r["m"], 0) >= CAP.get(kind, {}).get(r["m"], 2): continue
            d = min(((F[j] - F[c]) ** 2).sum() for c in chosen) + (1.5 if r["m"] not in per else 0)
            if d > bd: best, bd = j, d
        if best is None: break
        chosen.append(best); per[R[best]["m"]] = per.get(R[best]["m"], 0) + 1
    picks += [R[c] for c in chosen]

key = open(os.path.join(OUT, "key.txt"), "w")
for n, r in enumerate(picks, 1):
    path = os.path.join(OUT, f"{n:02d}.wav")
    lo, hi = 10, 400
    for _ in range(8):
        mid = (lo + hi) // 2
        if lufs(render(r["lines"], path, mid, nt=r["nt"])) < -18: lo = mid + 1
        else: hi = mid
    x = render(r["lines"], path, lo, nt=r["nt"]); l = lufs(x); vol = None
    while l < -18.5 and (vol or 64) < 127:
        vol = min(127, (vol or 64) + 6); x = render(r["lines"], path, 400, vol, r["nt"]); l = lufs(x); lo = 400
    tail = f"\ntrim {lo}" + ("".join(f"\nkit {t} 13 {vol}" for t in range(r["nt"])) if vol else "")
    open(os.path.join(OUT, f"{n:02d}.txt"), "w").write("\n".join(r["lines"]) + tail + "\n")
    stack = " x3" if r["nt"] == 3 else ""
    line = f"{n}\t{KIND[r['kind']]}\t{MNAME.get(r['m'], r['m'])}{stack}\t{r['desc']}\tid {r['cid']}\tharm {r['h']:.2f}"
    key.write(line + "\n"); print(f"{line}  {l:.1f} LUFS", flush=True)
