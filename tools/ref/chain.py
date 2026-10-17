#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The track chain, measured the same way on both instruments: every spec starts from sweep.py's clean start and moves
# one control. A sine (GND-SIN) for the levels, the distortion and the sample rate reduction; noise (GND-NOIS) for the
# filter and the EQ, whose responses are read as the ratio to the clean noise at the same note (the filter follows the
# note); a short sine blip for the delay.
#   .venv/bin/python tools/ref/chain.py specs|specs2|specs3 tools/ref   writes chain_*.spec (rounds one to three)
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sweep import CLEAN

SIN, NOIS = 1, 2
STEPS = list(range(0, 128, 8)) + [127]


def line(name, machine, note, cc, gate=1.2, tail=0.2, vel=100):
    extra = " ".join(f"{k}={v}" for k, v in cc.items())
    return f"{name} {machine} {note} {vel} {gate} {tail} {CLEAN} {extra}".rstrip()


def levels():
    out = ["# levels: VOL, LEV, velocity, PAN, on a C3 sine"]
    out += [line(f"vol_{v:03d}", SIN, 48, {61: v}) for v in STEPS]
    out += [line(f"lev_{v:03d}", SIN, 48, {7: v}) for v in STEPS]
    out += [line(f"vel_{v:03d}", SIN, 48, {}, vel=v) for v in (1, 32, 64, 100, 127)]
    out += [line(f"pan_{v:03d}", SIN, 48, {62: v}) for v in (0, 16, 32, 48, 64, 80, 96, 112, 127)]
    return out


def dist():
    out = ["# DIST from full headroom to full drive, on a C2 sine (harmonics well inside the band) and a C4 one"]
    out += [line(f"dist_n36_{v:03d}", SIN, 36, {60: v}) for v in range(0, 128, 4)] + [line("dist_n36_127", SIN, 36, {60: 127})]
    out += [line(f"dist_n60_{v:03d}", SIN, 60, {60: v}) for v in (0, 32, 48, 64, 80, 96, 112, 127)]
    # the drive on a quieter input: is it a fixed curve on the level, or does VOL come after it?
    out += [line(f"distvol_{d:03d}_{v:03d}", SIN, 36, {60: d, 61: v}) for d in (64, 96, 127) for v in (32, 127)]
    return out


def filt():
    out = ["# the filter on noise: the open reference at three notes, then BASE (HP), WDTH (LP), the Qs, both together"]
    for n in (36, 60, 84):
        out.append(line(f"f_open_n{n}", NOIS, n, {}, gate=3.0))
    for n in (36, 60, 84):
        out += [line(f"f_base_n{n}_{v:03d}", NOIS, n, {72: v}, gate=3.0) for v in range(0, 128, 16)]
        out += [line(f"f_wdth_n{n}_{v:03d}", NOIS, n, {73: v}, gate=3.0) for v in range(0, 128, 16)]
    out += [line(f"f_lpq_{q:03d}", NOIS, 60, {73: 48, 75: q}, gate=3.0) for q in range(0, 128, 16)] + [line("f_lpq_127", NOIS, 60, {73: 48, 75: 127}, gate=3.0)]
    out += [line(f"f_hpq_{q:03d}", NOIS, 60, {72: 48, 74: q}, gate=3.0) for q in range(0, 128, 16)] + [line("f_hpq_127", NOIS, 60, {72: 48, 74: 127}, gate=3.0)]
    out += [line(f"f_band_{b:03d}_{w:03d}", NOIS, 60, {72: b, 73: w}, gate=3.0) for b, w in ((32, 16), (32, 32), (48, 8), (64, 0))]
    # the Q's level and its headroom: a resonant LP on a sine at the cutoff, DIST at 64 and below
    out += [line(f"f_qsin_{q:03d}_{d:03d}", SIN, 48, {73: 16, 75: q, 60: d}) for q in (0, 64, 127) for d in (32, 64, 96)]
    return out


def eq():
    out = ["# the EQ on noise at C4: EQF at full boost and full cut, then EQG at one frequency"]
    out.append(line("eq_open", NOIS, 60, {}, gate=3.0))
    out += [line(f"eq_f_{f:03d}_up", NOIS, 60, {80: f, 81: 127}, gate=3.0) for f in range(0, 128, 16)] + [line("eq_f_127_up", NOIS, 60, {80: 127, 81: 127}, gate=3.0)]
    out += [line(f"eq_f_{f:03d}_dn", NOIS, 60, {80: f, 81: 0}, gate=3.0) for f in range(0, 128, 16)]
    out += [line(f"eq_g_{g:03d}", NOIS, 60, {80: 64, 81: g}, gate=3.0) for g in (0, 16, 32, 48, 56, 72, 80, 96, 112, 127)]
    out += [line(f"eq_f_064_up_d{d}", NOIS, 60, {80: 64, 81: 127, 60: d}, gate=3.0) for d in (32, 48)]
    return out


def srr():
    out = ["# SRR on a C6 sine and on noise: the hold rate shows as the images' spacing"]
    out += [line(f"srr_sin_{v:03d}", SIN, 84, {82: v}) for v in range(0, 128, 8)] + [line("srr_sin_127", SIN, 84, {82: 127})]
    out += [line(f"srr_noi_{v:03d}", NOIS, 60, {82: v}, gate=2.0) for v in (0, 32, 64, 96, 127)]
    return out


def delay():
    blip = {56: 0, 57: 0, 58: 20, 59: 0}               # a 20 ms-ish blip: hold off, a short decay
    out = ["# the delay on a short C4 blip: the send law, the time, the feedback, the loop's filter"]
    out += [line(f"d_snd_{v:03d}", SIN, 60, {**blip, 83: 32, 85: 0, 84: v}, gate=0.1, tail=1.2) for v in (0, 16, 32, 48, 63, 64, 65, 80, 96, 112, 127)]
    out += [line(f"d_tim_{v:03d}", SIN, 60, {**blip, 84: 127, 85: 0, 83: v}, gate=0.1, tail=2.4) for v in (1, 2, 4, 8, 16, 32, 48, 64, 96, 127)]
    out += [line(f"d_fb_{v:03d}", SIN, 60, {**blip, 84: 127, 83: 16, 85: v}, gate=0.1, tail=3.0) for v in (0, 32, 64, 96, 112, 127)]
    out += [line(f"d_bas_{v:03d}", NOIS, 60, {**blip, 58: 40, 84: 127, 83: 32, 85: 0, 86: v}, gate=0.1, tail=1.0) for v in (0, 32, 64, 96)]
    out += [line(f"d_wid_{v:03d}", NOIS, 60, {**blip, 58: 40, 84: 127, 83: 32, 85: 0, 87: v}, gate=0.1, tail=1.0) for v in (0, 32, 64, 96, 127)]
    return out


# ---- round two: finer, with headroom where the filter's resonance or the EQ's boost would reach the clip


def dist2():
    out = ["# DIST step by step on a C2 sine; where the clip sits against the filter; where the track clips"]
    out += [line(f"dd_{v:03d}", SIN, 36, {60: v}, gate=0.8, tail=0.05) for v in range(0, 128)]
    out += [line(f"dclip_lp_{v:03d}", SIN, 36, {60: v, 73: 16}, gate=0.8, tail=0.05) for v in (64, 80, 96, 127)]
    out += [line(f"dclip_hp_{v:03d}", SIN, 36, {60: v, 72: 16}, gate=0.8, tail=0.05) for v in (64, 80, 96, 127)]
    out += [line(f"dtop_v127_l{l:03d}_p{p:03d}", SIN, 36, {61: 127, 7: l, 62: p}, gate=0.8, tail=0.05) for l, p in ((127, 64), (64, 64), (100, 0), (127, 0))]
    return out


def filt2():
    out = ["# the filter's magnitude, a sine stepped through it: the cutoff moves 1/8 octave a step past a fixed C3"]
    out += [line(f"hp_{v:03d}", SIN, 48, {72: v}, gate=0.6, tail=0.05) for v in range(0, 57)]
    out += [line(f"lp_{v:03d}", SIN, 48, {73: v}, gate=0.6, tail=0.05) for v in range(0, 65)]
    out += [line(f"lp4_{v:03d}", SIN, 48, {73: v, 60: 40}, gate=0.6, tail=0.05) for v in range(0, 41, 4)]   # the same with headroom: linear?
    out.append(line("qn_open", NOIS, 60, {60: 32}, gate=3.0))
    out += [line(f"qn_lp_{q:03d}", NOIS, 60, {60: 32, 73: 48, 75: q}, gate=3.0) for q in range(0, 128, 8)] + [line("qn_lp_127", NOIS, 60, {60: 32, 73: 48, 75: 127}, gate=3.0)]
    out += [line(f"qn_hp_{q:03d}", NOIS, 60, {60: 32, 72: 48, 74: q}, gate=3.0) for q in range(0, 128, 8)] + [line("qn_hp_127", NOIS, 60, {60: 32, 72: 48, 74: 127}, gate=3.0)]
    for n in (36, 84):
        out.append(line(f"qn_open_n{n}", NOIS, n, {60: 32}, gate=3.0))
        out.append(line(f"qn_lp_n{n}_064", NOIS, n, {60: 32, 73: 48, 75: 64}, gate=3.0))
    out += [line(f"osc_{q:03d}", 0, 60, {73: 48, 75: q}, gate=1.0, tail=0.05) for q in range(64, 128, 4)] + [line("osc_127", 0, 60, {73: 48, 75: 127}, gate=1.0, tail=0.05)]
    out += [line(f"osch_{q:03d}", 0, 60, {72: 48, 74: q}, gate=1.0, tail=0.05) for q in (96, 112, 127)]
    return out


def eq2():
    out = ["# the EQ on noise with 24 dB of headroom (DIST 46), so its boost stays clear of the clip"]
    out.append(line("e2_open", NOIS, 60, {60: 46}, gate=3.0))
    out += [line(f"e2_f_{f:03d}_up", NOIS, 60, {60: 46, 80: f, 81: 127}, gate=3.0) for f in range(0, 128, 8)] + [line("e2_f_127_up", NOIS, 60, {60: 46, 80: 127, 81: 127}, gate=3.0)]
    out += [line(f"e2_f_{f:03d}_dn", NOIS, 60, {60: 46, 80: f, 81: 0}, gate=3.0) for f in range(4, 128, 8)]
    out += [line(f"e2_g_{g:03d}", NOIS, 60, {60: 46, 80: 64, 81: g}, gate=3.0) for g in range(0, 128, 8)] + [line("e2_g_127", NOIS, 60, {60: 46, 80: 64, 81: 127}, gate=3.0)]
    return out


def srr2():
    out = ["# SRR, every value, on a C6 sine"]
    out += [line(f"s2_{v:03d}", SIN, 84, {82: v}, gate=0.6, tail=0.05) for v in range(0, 128)]
    return out


def delay2():
    blip = {56: 0, 57: 0, 58: 20, 59: 0}
    out = ["# the delay: which side the echoes land on (the source hard left), their polarity, feedback above unity, the loop filter"]
    for snd in (0, 32, 96, 127):
        for fb in (0, 48):
            out.append(line(f"d2_pan_s{snd:03d}_f{fb:03d}", SIN, 60, {**blip, 62: 0, 83: 32, 85: fb, 84: snd}, gate=0.1, tail=1.4))
    out += [line(f"d2_pol_{snd:03d}", SIN, 60, {**blip, 83: 32, 85: 0, 84: snd}, gate=0.1, tail=0.5) for snd in (0, 127)]
    out += [line(f"d2_fb_{v:03d}", SIN, 60, {**blip, 84: 127, 83: 16, 85: v}, gate=0.1, tail=3.0) for v in (56, 64, 68, 72, 80, 88)]
    out += [line(f"d2_bas_{v:03d}", NOIS, 60, {**blip, 58: 30, 84: 127, 83: 48, 85: 0, 86: v, 87: 127}, gate=0.1, tail=0.8) for v in range(0, 128, 16)] + [line("d2_bas_127", NOIS, 60, {**blip, 58: 30, 84: 127, 83: 48, 85: 0, 86: 127, 87: 127}, gate=0.1, tail=0.8)]
    out += [line(f"d2_wid_{v:03d}", NOIS, 60, {**blip, 58: 30, 84: 127, 83: 48, 85: 0, 86: 0, 87: v}, gate=0.1, tail=0.8) for v in range(0, 128, 16)] + [line("d2_wid_127", NOIS, 60, {**blip, 58: 30, 84: 127, 83: 48, 85: 0, 86: 0, 87: 127}, gate=0.1, tail=0.8)]
    out += [line(f"d2_bw_{b:03d}_{w:03d}", NOIS, 60, {**blip, 58: 30, 84: 127, 83: 48, 85: 0, 86: b, 87: w}, gate=0.1, tail=0.8) for b, w in ((32, 32), (64, 16), (48, 48))]
    out.append(line("d2_default", NOIS, 60, {**blip, 58: 30, 84: 127, 83: 48, 85: 0}, gate=0.1, tail=0.8))
    return out


# ---- round three: the clip inside the filter, and the cutoff map read off the resonance


def clip3():
    out = ["# the clip: the resonance's ceiling against DIST; the drive at three input levels (the EQ cuts the sine before"
           " the filter); the HP's resonance against DIST"]
    out += [line(f"c3_res_{d:03d}", SIN, 48, {73: 16, 75: 127, 60: d}, gate=0.8, tail=0.05) for d in range(0, 128, 4)] + [line("c3_res_127", SIN, 48, {73: 16, 75: 127, 60: 127}, gate=0.8, tail=0.05)]
    out += [line(f"c3_lpf_{d:03d}", SIN, 48, {73: 16, 60: d}, gate=0.8, tail=0.05) for d in range(64, 128, 2)]
    out += [line(f"c3_lp24_{d:03d}", SIN, 48, {73: 24, 60: d}, gate=0.8, tail=0.05) for d in range(64, 128, 4)]
    for g in (48, 32):
        out.append(line(f"c3_eq{g}_ref", SIN, 48, {80: 28, 81: g}, gate=0.8, tail=0.05))
        out += [line(f"c3_eq{g}_{d:03d}", SIN, 48, {80: 28, 81: g, 60: d}, gate=0.8, tail=0.05) for d in range(64, 128, 4)]
    out += [line(f"c3_hres_{d:03d}", SIN, 48, {72: 16, 74: 127, 60: d}, gate=0.8, tail=0.05) for d in range(0, 128, 8)] + [line("c3_hres_127", SIN, 48, {72: 16, 74: 127, 60: 127}, gate=0.8, tail=0.05)]
    return out


def map3():
    out = ["# the cutoff map: the resonance's peak on noise (headroom DIST 32) for every 8th step, and at five notes"]
    out += [line(f"m3_lp_{w:03d}", NOIS, 60, {60: 32, 73: w, 75: 96}, gate=2.0) for w in range(0, 128, 8)]
    out += [line(f"m3_hp_{b:03d}", NOIS, 60, {60: 32, 72: b, 74: 96}, gate=2.0) for b in range(0, 128, 8)]
    out += [line(f"m3_note_{n:03d}", NOIS, n, {60: 32, 73: 40, 75: 96}, gate=2.0) for n in (24, 36, 48, 72, 96)]
    out += [line(f"m3_hnote_{n:03d}", NOIS, n, {60: 32, 72: 40, 74: 96}, gate=2.0) for n in (24, 36, 48, 72, 96)]
    return out


# ---- round four: the filter envelope, read off the resonance's peak (noise, headroom DIST 32, Q 96)


def fenv4():
    out = ["# the filter envelope: WOFS and BOFS against the cutoff, then the attack and decay times"]
    out += [line(f"fe_wofs_{v:03d}", NOIS, 60, {60: 32, 73: 32, 75: 96, 76: 0, 77: 127, 79: v}, gate=1.0) for v in range(0, 128, 8)] + [line("fe_wofs_127", NOIS, 60, {60: 32, 73: 32, 75: 96, 76: 0, 77: 127, 79: 127}, gate=1.0)]
    out += [line(f"fe_bofs_{v:03d}", NOIS, 60, {60: 32, 72: 32, 74: 96, 76: 0, 77: 127, 78: v}, gate=1.0) for v in range(0, 128, 8)] + [line("fe_bofs_127", NOIS, 60, {60: 32, 72: 32, 74: 96, 76: 0, 77: 127, 78: 127}, gate=1.0)]
    out += [line(f"fe_dec_{d:03d}", NOIS, 60, {60: 32, 73: 0, 75: 96, 76: 0, 77: d, 79: 127}, gate=3.0) for d in range(0, 128, 16)] + [line("fe_dec_127", NOIS, 60, {60: 32, 73: 0, 75: 96, 76: 0, 77: 127, 79: 127}, gate=3.0)]
    out += [line(f"fe_atk_{a:03d}", NOIS, 60, {60: 32, 73: 0, 75: 96, 76: a, 77: 127, 79: 127}, gate=3.0) for a in range(0, 128, 16)] + [line("fe_atk_127", NOIS, 60, {60: 32, 73: 0, 75: 96, 76: 127, 77: 127, 79: 127}, gate=3.0)]
    return out


def ftime5():
    out = ["# the filter envelope's times: WOFS full on WDTH 0 (an offset of 108.5 steps) sweeps the cutoff across a C4"
           " sine; the sine crosses -3.8 dB where the offset is 16 steps (0.147 of the envelope)"]
    out += [line(f"ft_dec_{d:03d}", SIN, 60, {73: 0, 76: 0, 77: d, 79: 127}, gate=6.0, tail=0.05) for d in range(0, 128, 8)]
    out += [line(f"ft_atk_{a:03d}", SIN, 60, {73: 0, 76: a, 77: 127, 79: 127}, gate=6.0, tail=0.05) for a in range(0, 128, 8)]
    return out


if __name__ == "__main__":
    if sys.argv[1] == "specs":
        d = sys.argv[2]
        for name, f in (("levels", levels), ("dist", dist), ("filt", filt), ("eq", eq), ("srr", srr), ("delay", delay)):
            with open(os.path.join(d, f"chain_{name}.spec"), "w") as fh:
                fh.write("\n".join(f()) + "\n")
    elif sys.argv[1] == "specs3":
        d = sys.argv[2]
        for name, f in (("clip3", clip3), ("map3", map3), ("fenv4", fenv4), ("ftime5", ftime5)):
            with open(os.path.join(d, f"chain_{name}.spec"), "w") as fh:
                fh.write("\n".join(f()) + "\n")
    elif sys.argv[1] == "specs2":
        d = sys.argv[2]
        for name, f in (("dist2", dist2), ("filt2", filt2), ("eq2", eq2), ("srr2", srr2), ("delay2", delay2)):
            with open(os.path.join(d, f"chain_{name}.spec"), "w") as fh:
                fh.write("\n".join(f()) + "\n")
