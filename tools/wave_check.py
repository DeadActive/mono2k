#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The wave banks as DIGI-WAV plays them: every wave of MONO2K's 32 and the owner's 64 at C2, C4 and C6 (host/m2k_ref:
# SCAN centred, no sync, bits, osc 2 or steps; DIST 48, the filter open), each render's pitch (a harmonic comb on its
# steady part, cents from the note), level (rms, dBFS) and aliasing (the energy off the note's harmonics, dB under the
# whole, 20 Hz..20 kHz). The owner's 64 are held to the bank MONO2K already plays: in tune within 1 c; their level
# within MONO2K's 32's range +-3 dB; their aliasing, note by note, under MONO2K's worst +3 dB, or reported. The
# numbers are kept (tools/ref/waves_m2k.json) and a later run must keep them (0.5 c, 0.3 dB, 2 dB): a regression of
# MONO2K's own, with no instrument to hold it to (the Monomachine's waves are not these).
#   .venv/bin/python tools/wave_check.py [--record]
import json, os, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "ref"))
from sweep import load, SR, CLEAN
import demo_pitch as D

OUT = os.path.join(ROOT, "build", "waves")
REF = os.path.join(HERE, "ref", "waves_m2k.json")
NOTES = (36, 60, 84)


def lines():
    out = []
    for bank, n in ((0, 32), (1, 64)):
        for k in range(n):
            v = min(126, 2 * k) if bank else round(127 * k / 31)
            for note in NOTES:
                syn = f"48={v} 49=64 50=127 51=0 52=0 53=64 54=0 55=0"
                out.append(f"w{bank}_{k:02d}_n{note} 6 {note} 100 1.2 0.05 {CLEAN} 60=48 {syn} m2k:p70={127 if bank else 0}")
    return out


def measure(path, note):
    x = load(path)[:, 0]
    seg = x[int(0.3 * SR):int(1.1 * SR)]
    f0 = D.hz(note)
    sp, df = D.spectrum(seg)
    c, _ = D.comb(sp, df, f0, hmax=16, span=60)
    p = sp ** 2
    fr = np.arange(len(p)) * df
    band = (fr > 20) & (fr < 20000)
    harm = np.zeros(len(p), bool)
    for k in range(1, int(20000 / f0) + 1):
        lo, hi = int((k * f0 - 1.5) / df), int((k * f0 + 1.5) / df) + 1
        harm[lo:hi] = True
    tot = p[band].sum()
    alias = 10 * np.log10(max(p[band & ~harm].sum(), 1e-30) / tot) if tot > 0 else -300.0
    return dict(cents=float(c), rms=float(20 * np.log10(np.sqrt(np.mean(seg ** 2)) + 1e-12)), alias=float(alias))


def main():
    os.makedirs(OUT, exist_ok=True)
    spec = os.path.join(OUT, "waves.spec")
    ls = lines()
    open(spec, "w").write("\n".join(ls) + "\n")
    subprocess.run(["cc", "-O2", "-Ifirmware/src", "-Ifirmware/gen", "-o", "build/host/m2k_ref", "host/m2k_ref.c", "-lm"], cwd=ROOT, check=True)
    subprocess.run([os.path.join(ROOT, "build/host/m2k_ref"), OUT], stdin=open(spec), stdout=subprocess.DEVNULL, cwd=ROOT, check=True)
    res = {}
    for l in ls:
        name, note = l.split()[0], int(l.split()[2])
        res[name] = measure(os.path.join(OUT, name + ".wav"), note)
    bad = []
    for note in NOTES:
        mine = [res[f"w0_{k:02d}_n{note}"] for k in range(32)]
        own = [res[f"w1_{k:02d}_n{note}"] for k in range(64)]
        lo, hi = min(m["rms"] for m in mine), max(m["rms"] for m in mine)
        worst = max(m["alias"] for m in mine)
        print(f"C{note // 12 - 1}: MONO2K's 32: level {lo:.1f}..{hi:.1f} dBFS, aliasing worst {worst:.1f} dB, pitch worst "
              f"{max(abs(m['cents']) for m in mine):.2f} c")
        print(f"     the owner's 64: level {min(m['rms'] for m in own):.1f}..{max(m['rms'] for m in own):.1f} dBFS, aliasing worst "
              f"{max(m['alias'] for m in own):.1f} dB, pitch worst {max(abs(m['cents']) for m in own):.2f} c")
        for k, m in enumerate(own):
            if abs(m["cents"]) > 1.0:
                bad.append(f"OS{k + 1:02d} at C{note // 12 - 1}: pitch {m['cents']:+.2f} c")
            if not lo - 3 <= m["rms"] <= hi + 3:
                bad.append(f"OS{k + 1:02d} at C{note // 12 - 1}: level {m['rms']:.1f} dBFS, outside {lo - 3:.1f}..{hi + 3:.1f}")
            if m["alias"] > worst + 3:
                bad.append(f"OS{k + 1:02d} at C{note // 12 - 1}: aliasing {m['alias']:.1f} dB, over MONO2K's worst {worst:.1f} + 3")
    if "--record" in sys.argv:
        json.dump(res, open(REF, "w"), indent=0, sort_keys=True)
        print(f"recorded {len(res)} renders into {os.path.relpath(REF, ROOT)}")
    elif os.path.exists(REF):
        ref = json.load(open(REF))
        for k, m in res.items():
            r = ref.get(k)
            if r is None or abs(m["cents"] - r["cents"]) > 0.5 or abs(m["rms"] - r["rms"]) > 0.3 or abs(m["alias"] - r["alias"]) > 2:
                bad.append(f"{k}: moved from the record ({r} -> {m})")
    print("\n".join(bad) if bad else "every wave: in tune, its level and aliasing as the bank's")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
