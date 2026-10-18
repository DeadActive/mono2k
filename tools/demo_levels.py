#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Bank A's levels, by measurement, for the parts in docs/bank-a.md (their tracks' roles from its table):
#   drums (kick snare hat perc) by peak against the kick's: kick 0, snare -3, hat -10, perc -8 dB
#   the rest (bass chord pad lead voice) by rms while they sound against the kick's: the rms of the 23 ms blocks
#   within 20 dB of the track's loudest (a sparse line's rms over the whole loop is mostly its silence, and set by
#   it a bleep reached the kick's peak): bass -1, chord -9, pad -10, voice -10, lead -10 dB (brief v3: the bass a
#   main element, locked with the kick; the earlier targets made the parts weak beside the drums)
#   each track alone over two loops (m2k_host --pitch's renders)
#   a DIGI-WAV part wave by wave: each trig's level (its first 150 ms, VOL taken out) from the pitch renders, every
#   other wave's VOL (wave_span's) set so its trigs stand where the kit's wave's do (the owner's waves span 16 dB)
#   the mix, driven by the bank's master drive (BANK_DRIVE), at A01 HAZE's rms (the bank's template), by the
#   pattern's trim; its low-end share (30-120 Hz: under 30 Hz is not heard, and a sub there only takes headroom) and
#   crest reported beside HAZE's: a part's bass rises over its
#   target until its low end is at least HAZE's, a low end still under it flagged
# The mix's peak is over 30 s from the start (m2k_host --demo), later loops included. VOL is square law, so an error of e dB moves it by 10^(e / 40); past 127
# the rest goes to LEVL. Edits firmware/src/m2k_factory.c in place, then prints the result.
#   .venv/bin/python tools/demo_levels.py [ROUNDS] [--slots A01,A02,...]
import math, os, re, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "ref"))
from sweep import load, SR
from demo_pitch import bank_table, events, M_WAV, P_SYN

F = os.path.join(ROOT, "firmware", "src", "m2k_factory.c")
P_VOL = 13
PEAK = {"kick": 0, "snare": -3, "hat": -10, "perc": -8}
RMS = {"bass": -1, "chord": -9, "pad": -10, "lead": -10, "voice": -10}


def build():
    subprocess.run(["cc", "-O2", "-Wno-unused-function", "-Ifirmware/src", "-Ifirmware/gen", "-o", "build/host/m2k_host",
                    "host/m2k_host.c", "-lm"], cwd=ROOT, check=True)


def metrics(path):
    """a mix's (rms dBFS, low-end share: the energy in 30-120 Hz against the whole, %, crest factor dB)"""
    x = load(path).mean(axis=1)
    rms = math.sqrt(np.mean(x ** 2))
    n = 1 << 14
    segs = [x[i:i + n] * np.hanning(n) for i in range(0, len(x) - n, n // 2)]
    p = np.mean([np.abs(np.fft.rfft(s)) ** 2 for s in segs], axis=0)
    fr = np.fft.rfftfreq(n, 1 / SR)
    low = p[(fr >= 30) & (fr < 120)].sum() / p[fr > 0].sum()
    return 20 * math.log10(rms), 100 * low, 20 * math.log10(np.abs(x).max() / rms)


def solo(slot, t):
    """(its rms while it sounds, its peak), dB, from the track's render alone"""
    x = load(os.path.join(ROOT, "build", "pitch", f"p{slot:02d}_t{t + 1}.wav"))[:, 0]
    n = 1024
    m = len(x) // n
    e = np.mean(x[:m * n].reshape(m, n) ** 2, axis=1)
    if e.max() <= 0:
        return -150.0, -150.0
    return 10 * math.log10(np.mean(e[e > e.max() * 1e-2])), 20 * math.log10(np.abs(x).max())


def analyze(slots):
    """{slot: (peak, rms, low-end share, crest)} of each part over 30 s from its start, as heard (the master drive
    on); A01 HAZE's too, the target"""
    d = os.path.join(ROOT, "build", "levels")
    os.makedirs(d, exist_ok=True)
    out = subprocess.run([os.path.join(ROOT, "build/host/m2k_host"), d, "--demo", "30"] + [str(x) for x in sorted(set(slots) | {1})],
                         cwd=ROOT, capture_output=True, text=True, check=True).stdout
    res = {}
    for m in re.finditer(r"A(\d\d) \S+\s+30 s: peak (-?[\d.]+) dBFS, rms (-?[\d.]+) dBFS -> (\S+)", out):
        r, low, crest = metrics(os.path.join(ROOT, m.group(4)))
        res[int(m.group(1))] = (float(m.group(2)), r, low, crest)
    return res


def per_wave(slot, t):
    """{wave value: the trigs' mean level (dB, VOL taken out)} for a DIGI-WAV track, from the pitch renders"""
    d = os.path.join(ROOT, "build", "pitch")
    head, ev = events(os.path.join(d, f"p{slot:02d}_t{t + 1}.txt"))
    x = load(os.path.join(d, f"p{slot:02d}_t{t + 1}.wav"))[:, 0]
    lev = {}
    for i, e in enumerate(ev):
        a = e["frame"]
        b = min(a + int(0.15 * SR), ev[i + 1]["frame"] if i + 1 < len(ev) else len(x))
        seg = x[a:b]
        if len(seg) < 256:
            continue
        db = 10 * math.log10(np.mean(seg ** 2) + 1e-20) - 40 * math.log10(max(e["eff"][P_VOL], 1) / 127)
        lev.setdefault(e["eff"][P_SYN], []).append(10 ** (db / 10))
    return {w: 10 * math.log10(np.mean(v)) for w, v in lev.items()}


def block(s, slot, name):
    a = s.index(f'pat_v5({slot - 1}, "{name}"')
    e = s.index(");", s.index("trim(p, ", a)) + 2
    return a, e


def balance(s, slot, name, roles, res):
    rms_mix, target = res[slot][1], res[1][1]
    low, low_h = res[slot][2] / 100, res[1][2] / 100
    lv = [solo(slot, t) if r != "-" else (-150.0, -150.0) for t, r in enumerate(roles)]
    rms, peak = [x[0] for x in lv], [x[1] for x in lv]
    a, e = block(s, slot, name)
    seg = s[a:e]
    ref = roles.index("kick")
    errs = {}
    for t, role in enumerate(roles):
        if t == ref or role == "-":
            continue
        err = peak[ref] + PEAK[role] - peak[t] if role in PEAK else rms[ref] + RMS[role] - rms[t]
        if role == "bass":                         # the low end at least HAZE's: the bass carries the deficit
            if low < low_h:
                err = max(err, min(6.0, max(0.5, 10 * math.log10((low_h / (1 - low_h)) / (low / (1 - low))))))
            elif low < low_h + 0.05:
                err = max(err, 0.0)
        errs[t] = err
    # a part already at VOL and LEV 127 short of its target (a SWAV-SAW bass: its clip's ceiling is 10.5 dB under
    # GND-SIN's, as the Monomachine's): every other track comes down by the shortfall instead, the kick too, and the
    # trim takes the mix back to HAZE's level
    def vol_lev(t):
        st = re.search(r"SET\(p, %d,.*?\);" % t, seg, re.S).group(0)
        lm = re.search(r"P_LEVL, (\d+)", st)
        return int(re.search(r"P_VOL, (\d+)", st).group(1)), int(lm.group(1)) if lm else 100
    short = max([0.0] + [e - 40 * math.log10(127 * 127 / vol_lev(t)[1] / vol_lev(t)[0]) for t, e in errs.items()])
    if short > 0.1:
        errs = {t: e - short for t, e in errs.items()}
        errs[ref] = -short
    for t, err in sorted(errs.items()):
        m = re.search(r"SET\(p, %d,.*?\);" % t, seg, re.S)
        st = m.group(0)
        v, lv = vol_lev(t)
        lm = re.search(r"P_LEVL, (\d+)", st)
        want = v * 10 ** (err / 40)
        nv = max(1, min(127, round(want)))
        if want > 127.5:
            lv = max(1, min(127, round(lv * (want / 127))))
        elif lm and want < 127:
            lv = max(100, round(lv * min(1, want / 127)))
        st2 = re.sub(r"P_VOL, \d+", f"P_VOL, {nv}", st)
        if lm:
            st2 = re.sub(r"P_LEVL, \d+", f"P_LEVL, {lv}", st2)
        elif lv != 100:
            st2 = st2.replace(f"P_VOL, {nv}", f"P_VOL, {nv}, P_LEVL, {lv}")
        seg = seg[:m.start()] + st2 + seg[m.end():]
        # another wave in the part: its VOL from the kit's wave's level against its own
        spans = list(re.finditer(r"wave_span\(p, %d, (\d+), (\d+), OS\((\d+)\), (\d+)\);" % t, seg))
        if spans:
            kit_os = int(re.search(r"SA, OS\((\d+)\)", st2).group(1))
            lev = per_wave(slot, t)
            base = lev.get(2 * (kit_os - 1))
            for sp in reversed(spans):
                w = 2 * (int(sp.group(3)) - 1)
                if base is None or w not in lev:
                    continue
                nw = max(1, min(127, round(nv * 10 ** ((base - lev[w]) / 40))))
                rep = f"wave_span(p, {t}, {sp.group(1)}, {sp.group(2)}, OS({sp.group(3)}), {nw});"
                seg = seg[:sp.start()] + rep + seg[sp.end():]
    tm = re.search(r"trim\(p, (\d+)\);", seg)
    tr = int(tm.group(1))
    if abs(rms_mix - target) > 0.4 and slot != 1:  # HAZE is the template: its trim stays
        tr = max(30, min(127, round(tr * 10 ** ((target - rms_mix) / 40))))
    seg = seg[:tm.start()] + f"trim(p, {tr});" + seg[tm.end():]
    return s[:a] + seg + s[e:]


def main():
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 3
    table = full = bank_table()
    if "--slots" in sys.argv:
        want = {int(x[1:]) for x in sys.argv[sys.argv.index("--slots") + 1].split(",")}
        table = {k: v for k, v in table.items() if k in want}
    for _ in range(rounds):
        build()
        subprocess.run([os.path.join(ROOT, "build/host/m2k_host"), "build/pitch", "--pitch"], cwd=ROOT, check=True,
                       stdout=subprocess.DEVNULL)
        res = analyze(sorted(table))
        s = open(F).read()
        for slot, row in sorted(table.items()):
            s = balance(s, slot, row["name"], row["roles"], res)
        open(F, "w").write(s)
    build()
    subprocess.run([os.path.join(ROOT, "build/host/m2k_host"), "build/pitch", "--pitch"], cwd=ROOT, check=True,
                   stdout=subprocess.DEVNULL)
    res = analyze(sorted(table))
    h = res[1]
    print(f"{'':12s} {'rms':>6s} {'low end':>8s} {'crest':>6s} {'peak':>6s}   BPM  key            flavour")
    for slot in sorted(set(table) | {1}):
        row = full.get(slot, {})
        pk, r, low, crest = res[slot]
        flag = "  LOW END UNDER HAZE'S" if slot != 1 and low < h[2] else ""
        print(f"A{slot:02d} {row.get('name', ''):8s} {r:6.1f} {low:7.1f}% {crest:6.1f} {pk:6.1f}   {row.get('bpm', ''):>3} "
              f"{row.get('key', '') + ' ' + row.get('mode', ''):14s} {row.get('flavour', '')}{flag}")
    for slot, row in sorted(table.items()):
        roles = row["roles"]
        lvs = [solo(slot, t) if r != "-" else (-150.0, -150.0) for t, r in enumerate(roles)]
        rms, peak = [x[0] for x in lvs], [x[1] for x in lvs]
        k = roles.index("kick")
        cells = []
        for t, role in enumerate(roles):
            if role == "-":
                cells.append("    rests")
            elif role in PEAK:
                cells.append(f"{role:>5} {peak[t] - peak[k]:+5.1f}p")
            else:
                cells.append(f"{role:>5} {rms[t] - rms[k]:+5.1f}r {peak[t] - peak[k]:+4.1f}p")
        waves = ""
        for t, role in enumerate(roles):
            if role != "-" and os.path.exists(os.path.join(ROOT, "build", "pitch", f"p{slot:02d}_t{t + 1}.txt")):
                head, _ = events(os.path.join(ROOT, "build", "pitch", f"p{slot:02d}_t{t + 1}.txt"))
                if head["machine"] == M_WAV:
                    lv = per_wave(slot, t)
                    if len(lv) > 1:
                        vals = list(lv.values())
                        waves += f"  T{t + 1} waves within {max(vals) - min(vals):.1f} dB"
        print(f"A{slot:02d} {row['name']:8s} | " + " | ".join(cells) + waves)


if __name__ == "__main__":
    main()
