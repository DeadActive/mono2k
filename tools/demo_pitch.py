#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Is bank A in tune? `m2k_host DIR --pitch` renders each track alone with every trig it plays (frame, note, the
# track's parameters as they stood); this measures each trig's pitch against the note it was given, and reads what
# the pattern means to sound:
#   tuning   each step's fundamental (and, for SWAV-ENS, each chord voice), cents from the note's equal-tempered pitch
#            (+ PTCH and the machine's TUNE), by a harmonic comb on the trig's steady part
#   filters  a resonant cutoff rings at the note x 2^((step - 16) / 8): steps of a tone and a half, so an odd step
#            rings a quarter tone off the scale
#   notes    each melodic track's pitch classes against the pattern's declared key; voices sounding together a
#            semitone or a tritone apart, on different tracks
#   .venv/bin/python tools/demo_pitch.py build/pitch [--steps] [--ea69d53]
# SWAV-ENS's acceptance is regress.py's ens_accept: bank A's parts as they stood (tools/ref/fixtures), converted,
# each voice against the Monomachine's (tools/ref/chain_ensacc.spec, ens_voices.py)
import math, os, sys
import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "ref"))
from sweep import load, SR

M_GSIN, M_GNOI, M_SID, M_FMS, M_FMD, M_SAW, M_ENS, M_WAV, M_VOX = range(9)
MNAME = ["GND-SIN", "GND-NOIS", "SID-8BIT", "FM+STAT", "FM+DYN", "SWAV-SAW", "SWAV-ENS", "DIGI-WAV", "VOX-6"]
P_SYN, P_DEC, P_PORT, P_BASE, P_WDTH, P_HPQ, P_LPQ, P_PTCH, P_LFO = 0, 10, 15, 16, 17, 18, 19, 36, 42
NAMES = "C C# D D# E F F# G G# A A# B".split()
# the keys and the tracks' roles each part declares: docs/bank-a.md's table; the old bank's (ea69d53, kept as a
# fixture) for the slots not in it, or for every slot with --ea69d53
KEYS_EA69D53 = {1: ("D", "minor"), 2: ("F", "minor"), 3: ("F", "major"), 4: ("A", "minor"), 5: ("C", "minor"),
                6: ("E", "dorian"), 7: ("F", "minor"), 8: ("C#", "major"), 9: ("E", "minor"), 10: ("G", "minor"),
                11: ("A", "major"), 12: ("C", "major"), 13: ("G", "minor"), 14: ("C", "minor"), 15: ("D", "dorian"),
                16: ("F#", "minor")}
MODES = {"major": [0, 2, 4, 5, 7, 9, 11], "minor": [0, 2, 3, 5, 7, 8, 10], "dorian": [0, 2, 3, 5, 7, 9, 10],
         "phrygian": [0, 1, 3, 5, 7, 8, 10], "lydian": [0, 2, 4, 6, 7, 9, 11]}
FLAT = {"Db": "C#", "Eb": "D#", "Gb": "F#", "Ab": "G#", "Bb": "A#"}
DRUMS = {"kick", "snare", "hat", "perc"}


def bank_table(path=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "docs", "bank-a.md")):
    """{part: dict(name, key, mode, bpm, roles)} from the notes' table, by its header (Slot Name ... Key ... Tracks)"""
    out, head = {}, None
    if not os.path.exists(path):
        return out
    for l in open(path, encoding="utf-8"):
        c = [x.strip() for x in l.strip().strip("|").split("|")]
        if c and c[0] == "Slot":
            head = [h.lower() for h in c]
        elif head and len(c) == len(head) and c[0][:1] == "A" and c[0][1:].isdigit():
            r = dict(zip(head, c))
            key, mode = r["key"].split()[:2]
            out[int(c[0][1:])] = dict(name=r["name"], key=FLAT.get(key, key), mode=mode, bpm=int(r["bpm"]),
                                      roles=r["tracks"].split())
    return out
def _table(name):
    import re
    h = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "firmware", "gen", "m2k_tables.h")).read()
    body = h[h.index(name + "["):]
    return [int(v) for v in re.findall(r"-?\d+", body[body.index("{") + 1:body.index("}")])]


ENS_PCH = _table("T_ENS_PCH")                      # 1/64 semitone; -32768 off
LPQ = [(0, .74), (8, .95), (16, 1.6), (24, 2.6), (32, 4.5), (40, 7.5), (48, 12.5), (56, 19), (64, 27.5)]
HPQ = [(0, .80), (8, 1.03), (16, 1.43), (24, 1.9), (32, 3.5), (40, 5.6), (48, 8.7), (56, 15.6), (64, 30)]


def q_of(tab, v):
    for (a, qa), (b, qb) in zip(tab, tab[1:]):
        if v <= b:
            return qa * (qb / qa) ** ((v - a) / (b - a))
    return tab[-1][1] * 2 ** ((v - 64) / 8)


def hz(n):
    return 440 * 2 ** ((n - 69) / 12)


def events(path):
    head, ev = None, []
    for l in open(path):
        if l.startswith("#"):
            w = l.split()
            head = dict(pat=int(w[1][1:]), name=w[2], trk=int(w[3][1:]), machine=int(w[5]), bpm=int(w[7]), steps=int(w[9]))
            continue
        v = [int(x) for x in l.split()]
        ev.append(dict(frame=v[0], note=v[1], vel=v[2], hits=v[3], eff=v[4:]))
    return head, ev


def voices(m, e):
    """the pitches (MIDI, fractional) a trig means to sound: the fundamental first"""
    k = e["eff"]
    n = e["note"] + (k[P_PTCH] - 64)
    if m == M_GSIN:
        return [n + (k[P_SYN] - 64) * 2 / 64]
    if m == M_SAW:
        return [n + (k[P_SYN + 7] - 64) * 2 / 64]
    if m == M_ENS:                     # the Monomachine's page (format 7): PCH2..4 off its table, TUNE on all
        n += (k[P_SYN + 7] - 64) * 2 / 64
        return [n] + [n + ENS_PCH[k[P_SYN + i]] / 64 for i in range(3) if ENS_PCH[k[P_SYN + i]] != -32768]
    return [n]


def spectrum(x):
    w = np.hanning(len(x))
    nf = 1 << int(math.ceil(math.log2(len(x) * 8)))
    return np.abs(np.fft.rfft(x * w, nf)), SR / nf


def comb(sp, df, f, hmax=10, span=120):
    """the cents (-span..span) whose harmonic series best fits the spectrum near f"""
    cs = np.arange(-span, span + 0.5, 0.5)
    hs = np.arange(1, hmax + 1)
    fr = f * 2 ** (cs[:, None] / 1200) * hs[None, :]
    ok = fr < min(9000, SR / 2 - 200)
    idx = np.clip(np.round(fr / df).astype(int), 0, len(sp) - 1)
    sc = np.where(ok, sp[idx], 0).sum(1)
    i = int(np.argmax(sc))
    if 0 < i < len(cs) - 1:
        a, b, c = sc[i - 1], sc[i], sc[i + 1]
        d = 0.5 * (a - c) / (a - 2 * b + c) if a - 2 * b + c else 0
        return float(cs[i] + d * 0.5), float(sc[i])
    return float(cs[i]), float(sc[i])


def peak_near(sp, df, f, span=80):
    """the strongest partial within span cents of f: its offset in cents"""
    lo, hi = int(f * 2 ** (-span / 1200) / df), int(f * 2 ** (span / 1200) / df) + 1
    k = lo + int(np.argmax(sp[lo:hi]))
    a, b, c = np.log(sp[k - 1] + 1e-12), np.log(sp[k] + 1e-12), np.log(sp[k + 1] + 1e-12)
    d = 0.5 * (a - c) / (a - 2 * b + c) if a - 2 * b + c else 0
    return 1200 * math.log2((k + d) * df / f)


def measure(x, ev, i, total, m):
    e = ev[i]
    end = ev[i + 1]["frame"] if i + 1 < len(ev) else total
    a = e["frame"] + int(0.015 * SR)
    b = min(end, e["frame"] + int(0.6 * SR))
    seg = x[a:b]
    if len(seg) < int(0.03 * SR):
        return None
    env = np.abs(seg)
    if env.max() < 1e-4:
        return None
    # the steady part: where the trig still stands within 30 dB of its peak
    sm = np.convolve(env, np.ones(256) / 256, "same")
    keep = np.nonzero(sm > sm.max() * 0.03)[0]
    seg = seg[keep[0]:keep[-1]] if len(keep) > 0.03 * SR else seg
    sp, df = spectrum(seg)
    vs = voices(m, e)
    c0, _ = comb(sp, df, hz(vs[0]))
    out = dict(step=e, cents=c0, dur=len(seg) / SR, voices=[])
    if m == M_ENS:
        out["voices"] = [peak_near(sp, df, hz(v)) for v in vs]
    # wobble: the comb on each half
    if len(seg) > 0.12 * SR:
        h = len(seg) // 2
        out["wobble"] = abs(comb(*spectrum(seg[:h]), hz(vs[0]))[0] - comb(*spectrum(seg[h:]), hz(vs[0]))[0])
    return out


def held(d, p, t, m, e0):
    """the held note on the track's kit: the engine's own tuning (fundamental, and each chord voice)"""
    import glob
    fs = glob.glob(os.path.join(d, f"p{p:02d}_t{t}_held*.wav"))
    if not fs:
        return None
    note = int(fs[0].rsplit("held", 1)[1][:-4])
    x = load(fs[0])[int(0.3 * SR):, 0]
    if np.abs(x).max() < 1e-4:
        return None
    e = dict(e0, note=note)
    vs = voices(m, e)
    sp, df = spectrum(x)
    c, _ = comb(sp, df, hz(vs[0]))
    return c, ([peak_near(sp, df, hz(v)) for v in vs] if m == M_ENS else []), note


def ring(m, e):
    """a resonant filter's ring against the note: (which, semitones over the note, Q)"""
    k = e["eff"]
    out = []
    if k[P_LPQ] >= 32 and k[P_BASE] + k[P_WDTH] < 112:
        out.append(("LP", 1.5 * (k[P_BASE] + k[P_WDTH] - 16), q_of(LPQ, k[P_LPQ])))
    if k[P_HPQ] >= 32 and k[P_BASE] > 0:
        out.append(("HP", 1.5 * (k[P_BASE] - 16), q_of(HPQ, k[P_HPQ])))
    return out


def main():
    d = sys.argv[1]
    show_steps = "--steps" in sys.argv
    table = bank_table() if "--ea69d53" not in sys.argv else {}
    for p in range(1, 17):                         # a slot not yet in the notes keeps the old bank's key
        key, mode = (table[p]["key"], table[p]["mode"]) if p in table else KEYS_EA69D53[p]
        roles = table[p]["roles"] if p in table else [None] * 6
        scale = {(NAMES.index(key) + s) % 12 for s in MODES[mode]}
        sounding = []                                  # (t0, t1, midi, track) for the clash check
        lines = []
        for t in range(1, 7):
            head, ev = events(os.path.join(d, f"p{p:02d}_t{t}.txt"))
            m = head["machine"]
            if m == M_GNOI or not ev:
                continue
            x = load(os.path.join(d, f"p{p:02d}_t{t}.wav"))[:, 0]
            drum = roles[t - 1] in DRUMS if roles[t - 1] else m == M_GSIN and ev[0]["eff"][P_SYN + 4] > 0 and ev[0]["eff"][P_DEC] < 40
            res = [measure(x, ev, i, len(x), m) for i in range(len(ev))]
            res = [r for r in res if r]
            if not res:
                continue
            cents = np.array([r["cents"] for r in res])
            pcs = sorted({round(v) % 12 for r in res for v in voices(m, r["step"])})
            out_key = [NAMES[c] for c in pcs if c not in scale]
            vtxt = ""
            if m == M_ENS:
                vv = np.array([r["voices"] for r in res])
                vtxt = " voices " + " ".join(f"{np.median(vv[:, j]):+.0f}" for j in range(vv.shape[1])) + " c"
            port = res[0]["step"]["eff"][P_PORT]
            lfo = [(res[0]["step"]["eff"][P_LFO + 8 * l + 1] - 1, res[0]["step"]["eff"][P_LFO + 8 * l + 7]) for l in range(3)]
            lfo = [f"LFO>{dst}" for dst, dp in lfo if dst >= 0 and dp != 64 and (dst == P_PTCH or dst < 8)]
            h = held(d, p, t, m, res[0]["step"])
            htxt = ""
            if h:
                htxt = f"  held n{h[2]}: {h[0]:+.1f} c" + (" voices " + " ".join(f"{v:+.0f}" for v in h[1]) + " c" if h[1] else "")
            ring_pcs = {}
            for r in res:
                for kind, semi, q in ring(m, r["step"]):
                    pc = round(r["step"]["note"] + semi) % 12 if semi % 1 == 0 else None
                    ring_pcs.setdefault((kind, semi, round(q)), []).append(pc)
            rtxt = []
            for (kind, semi, q), pl in sorted(ring_pcs.items()):
                bad = sum(1 for c in pl if c is None or c not in scale)
                rtxt.append(f"{kind} +{semi:g} st Q{q}" + (f" ({bad}/{len(pl)} trigs ring off the key)" if bad else ""))
            kick = roles[t - 1] == "kick" if roles[t - 1] else t == 1 and m == M_GSIN and len(pcs) == 1
            lines.append(f"  T{t} {MNAME[m]:9s}{' kick ' if kick else ' (drum)' if drum else '       '} {len(res):3d} trigs"
                         f"  in context {np.median(cents):+6.1f} c (worst {cents[np.argmax(np.abs(cents))]:+6.1f}){vtxt}{htxt}"
                         f"{'  PORT ' + str(port) if port else ''}{'  ' + ' '.join(lfo) if lfo else ''}")
            lines.append(f"     notes {' '.join(NAMES[c] for c in pcs)}" +
                         (f"  OUT OF KEY: {' '.join(out_key)}" if out_key and (kick or not drum) else "") +
                         (f"  rings {'; '.join(rtxt)}" if rtxt else ""))
            if show_steps:
                for r in res:
                    lines.append(f"       f{r['step']['frame']:7d} n{r['step']['note']:3d} {r['cents']:+6.1f} c " +
                                 (" ".join(f"{v:+.0f}" for v in r["voices"]) if r["voices"] else ""))
            if not drum and not kick:
                for i, r in enumerate(res):
                    t0 = r["step"]["frame"] / SR
                    for v in voices(m, r["step"]):
                        sounding.append((t0, t0 + r["dur"], v, t))
        # tracks sounding a semitone apart at once (a minor second or ninth: the clash a major seventh is not)
        clash = {}
        for a in sounding:
            for b in sounding:
                if a[3] < b[3] and a[0] < b[1] and b[0] < a[1] and round(abs(a[2] - b[2])) in (1, 13):
                    lo, hi = sorted((a, b), key=lambda v: v[2])
                    clash.setdefault((a[3], b[3]), set()).add(f"{NAMES[round(lo[2]) % 12]}/{NAMES[round(hi[2]) % 12]}")
        print(f"A{p:02d} {head['name']:8s} declared {key} {mode}, {head['bpm']} bpm")
        print("\n".join(lines))
        for (ta, tb), pairs in sorted(clash.items()):
            print(f"     semitone clash T{ta}/T{tb}: {' '.join(sorted(pairs))}")


if __name__ == "__main__":
    main()
