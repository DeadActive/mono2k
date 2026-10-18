#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The track chain, measured the same way on both instruments: every spec starts from sweep.py's clean start and moves
# one control. A sine (GND-SIN) for the levels, the distortion and the sample rate reduction; noise (GND-NOIS) for the
# filter and the EQ, whose responses are read as the ratio to the clean noise at the same note (the filter follows the
# note); a short sine blip for the delay.
#   .venv/bin/python tools/ref/chain.py specs|specs2|specs3|fm26|sid36 tools/ref   writes chain_*.spec (rounds one to three;
#   FM+; SID-6581)
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


# the track controls CLEAN leaves out, at MONO2K's INIT values: the Monomachine keeps a control from the line before,
# so every delay line sends them all (EQF, DTIM, DFB, DBAS, DWID)
DELAY_INIT = {80: 64, 83: 64, 85: 32, 86: 0, 87: 127}


def delay():
    blip = {**DELAY_INIT, 56: 0, 57: 0, 58: 20, 59: 0}  # a 20 ms-ish blip: hold off, a short decay
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
    blip = {**DELAY_INIT, 56: 0, 57: 0, 58: 20, 59: 0}
    out = ["# the delay: which side the echoes land on (the source hard left), their polarity, feedback above unity, the loop filter"]
    for snd in (0, 32, 96, 127):
        for fb in (0, 48):
            out.append(line(f"d2_pan_s{snd:03d}_f{fb:03d}", SIN, 60, {**blip, 62: 0, 83: 32, 85: fb, 84: snd}, gate=0.1, tail=1.4))
    out += [line(f"d2_pol_{snd:03d}", SIN, 60, {**blip, 83: 32, 85: 0, 84: snd}, gate=0.1, tail=0.5) for snd in (0, 127)]
    out += [line(f"d2_fb_{v:03d}", SIN, 60, {**blip, 84: 127, 83: 16, 85: v}, gate=0.1, tail=3.0) for v in (56, 64, 68, 72, 80, 88)]
    out += [line(f"d2_bas_{v:03d}", NOIS, 60, {**blip, 58: 30, 84: 127, 83: 48, 85: 0, 86: v, 87: 127}, gate=0.1, tail=0.8) for v in range(0, 128, 16)] + [line("d2_bas_127", NOIS, 60, {**blip, 58: 30, 84: 127, 83: 48, 85: 0, 86: 127, 87: 127}, gate=0.1, tail=0.8)]
    out += [line(f"d2_wid_{v:03d}", NOIS, 60, {**blip, 58: 30, 84: 127, 83: 48, 85: 0, 86: 0, 87: v}, gate=0.1, tail=0.8) for v in range(0, 128, 16)] + [line("d2_wid_127", NOIS, 60, {**blip, 58: 30, 84: 127, 83: 48, 85: 0, 86: 0, 87: 127}, gate=0.1, tail=0.8)]
    out += [line(f"d2_bw_{b:03d}_{w:03d}", NOIS, 60, {**blip, 58: 30, 84: 127, 83: 48, 85: 0, 86: b, 87: w}, gate=0.1, tail=0.8) for b, w in ((32, 32), (64, 16), (48, 48))]
    out.append(line("d2_default", NOIS, 60, {**blip, 58: 30, 84: 127, 83: 48, 85: 0}, gate=0.1, tail=0.8))   # INIT's DBAS / DWID
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


# ---- round six: the resonance at high Q, resolved: where its peak sits against the cutoff, how high, how narrow


def res6():
    out = ["# the resonance resolved: noise at C6 with headroom (DIST 32), 4 s; BASE 40 / WDTH 40 put the cutoff at"
           " 8 x f0 = 8372 Hz; the same at C4 (BASE / WDTH 48, 16 x f0 = 4186 Hz); and GND-SIN's TUNE, to step a sine"]
    out.append(line("r6_open_n84", NOIS, 84, {60: 32}, gate=4.0))
    for q in (32, 48, 64, 80, 96, 112, 127):
        out.append(line(f"r6_hp_n84_{q:03d}", NOIS, 84, {60: 32, 72: 40, 74: q}, gate=4.0))
        out.append(line(f"r6_lp_n84_{q:03d}", NOIS, 84, {60: 32, 73: 40, 75: q}, gate=4.0))
    out.append(line("r6_open_n60", NOIS, 60, {60: 32}, gate=4.0))
    for q in (96, 127):
        out.append(line(f"r6_hp_n60_{q:03d}", NOIS, 60, {60: 32, 72: 48, 74: q}, gate=4.0))
        out.append(line(f"r6_lp_n60_{q:03d}", NOIS, 60, {60: 32, 73: 48, 75: q}, gate=4.0))
    out += [line(f"r6_tune_{v:03d}", SIN, 60, {55: v}, gate=0.6, tail=0.05) for v in (0, 32, 60, 63, 64, 65, 68, 96, 127)]
    return out


def hres8():
    out = ["# the resonance at low cutoffs, long enough to settle (Q ~1000 at 131 Hz builds over seconds): a sine on the"
           " cutoff (BASE / WDTH 16) and noise, 12 s, headroom (DIST 16 for the sine, 32 for noise); C3 and C5"]
    for n in (48, 72):
        out.append(line(f"h8_open_n{n}", NOIS, n, {60: 32}, gate=12.0))
        for kind, q in (("hp", 127), ("hp", 96), ("lp", 127)):
            filt = {72: 16, 74: q} if kind == "hp" else {73: 16, 75: q}
            out.append(line(f"h8_sin_{kind}{q:03d}_n{n}", SIN, n, {60: 16, **filt}, gate=12.0, tail=0.05))
            out.append(line(f"h8_noi_{kind}{q:03d}_n{n}", NOIS, n, {60: 32, **filt}, gate=12.0, tail=0.05))
    return out


def drive10():
    out = ["# drive apart from the build-up: a sine on the cutoff, notes long enough to settle. The HP at low Q (48, 64),"
           " where it settles in milliseconds, C3 and C5, DIST 64..127; HPQ 127 at C5 (settles in ~1 s), 6 s; the LP at"
           " LPQ 64 as the control"]
    for q in (48, 64):
        for n in (48, 72):
            out += [line(f"d10_hp{q:03d}_n{n}_d{d:03d}", SIN, n, {72: 16, 74: q, 60: d}, gate=3.0, tail=0.05) for d in (64, 72, 80, 96, 127)]
    out += [line(f"d10_hp127_n72_d{d:03d}", SIN, 72, {72: 16, 74: 127, 60: d}, gate=6.0, tail=0.05) for d in (16, 64, 80, 127)]
    out += [line(f"d10_lp064_n48_d{d:03d}", SIN, 48, {73: 16, 75: 64, 60: d}, gate=3.0, tail=0.05) for d in (64, 72, 80, 96, 127)]
    return out


def drive12():
    out = ["# where the HP saturates: a C3 sine against the HP's cutoff on the note (BASE 16), an octave under it (8) and"
           " over it (24); HPQ 0..96, DIST from 16 (-65 dB) up to 64, 2.5 s; C5 at HPQ 48"]
    for base in (16, 8, 24):
        for q in ((0, 16, 32, 48, 64, 96) if base == 16 else (48, 96)):
            out += [line(f"d12_b{base:02d}_q{q:03d}_n48_d{d:03d}", SIN, 48, {72: base, 74: q, 60: d}, gate=2.5, tail=0.05)
                    for d in (16, 32, 40, 48, 52, 56, 60, 64)]
    out += [line(f"d12_b16_q048_n72_d{d:03d}", SIN, 72, {72: 16, 74: 48, 60: d}, gate=2.5, tail=0.05) for d in (16, 32, 40, 48, 52, 56, 60, 64)]
    return out


def tune13():
    out = ["# does the filter follow GND-SIN's TUNE? A C4 sine on a resonant LP / HP at the note (WDTH / BASE 16, Q 64:"
           " settles in ~30 ms), DIST 16 (linear), TUNE across +-100 c: the Monomachine's on CC 55, MONO2K's on A (CC 48),"
           " both 3.125 c a step. Tracking keeps the level; a fixed filter loses ~10 dB at 100 c"]
    for kind, filt in (("lp", {73: 16, 75: 64}), ("hp", {72: 16, 74: 64})):
        for t in (32, 40, 48, 56, 64, 72, 80, 88, 95):
            out.append(line(f"t13_{kind}_{t:03d}", SIN, 60, {60: 16, **filt}, gate=1.0, tail=0.05) + f" mm:55={t} m2k:48={t}")
    return out


def sidbend14():
    out = ["# is a resonant sweep's pitch bend the instrument's? A04's SID bass, its chain as written (WDTH 26, LPQ 50, FDEC 34,"
           " WOFS 84, DIST 72, DEC 34, gate 73 ms at 128 bpm), on SWAVE-SAW (the source both instruments share, calibrated"
           " within 0.2 c) at its eight most played notes; then without the resonance, then the filter open"]
    saw = {48: 0, 49: 0, 50: 0, 51: 0, 52: 0, 53: 0, 54: 0, 55: 64}
    amp = {56: 0, 57: 0, 58: 34, 59: 10, 60: 72, 61: 127}
    for kind, filt in (("res", {73: 26, 75: 50, 77: 34, 79: 84}), ("nores", {73: 26, 75: 0, 77: 34, 79: 84}),
                       ("open", {73: 127, 75: 0, 77: 34, 79: 64})):
        for n in (45, 43, 41, 40, 52, 48, 57, 55):
            out.append(line(f"s14_{kind}_n{n}", 4, n, {**saw, **amp, **filt}, gate=0.073, tail=0.3))
    return out


def ens15():
    out = ["# SWAVE-ENS (14) on its own page, one control at a time: a C4 held 1.5 s (3 s for CHRL / CHRW), DIST 48 (no clip"
           " for four voices), the filter open. The page: PCH2 PCH3 PCH4 WAVE PW CHRL CHRW TUNE on CC 48..55. The default"
           " first (no SYN controller sent); then each control swept with CHRL and CHRW at 0 unless they are the subject"]
    ens = 14
    base = {60: 48}
    quiet = {53: 0, 54: 0}
    out.append(line("e15_default", ens, 60, base, gate=1.5, tail=0.1))
    out += [line(f"e15_pch2_{v:03d}", ens, 60, {**base, **quiet, 48: v}, gate=1.5, tail=0.1) for v in list(range(0, 128, 4)) + [127]]
    out += [line(f"e15_pch3_{v:03d}", ens, 60, {**base, **quiet, 49: v}, gate=1.5, tail=0.1) for v in (8, 40, 64, 100, 127)]
    out += [line(f"e15_pch4_{v:03d}", ens, 60, {**base, **quiet, 50: v}, gate=1.5, tail=0.1) for v in (8, 40, 64, 100, 127)]
    out += [line(f"e15_wave_{v:03d}", ens, 60, {**base, **quiet, 51: v}, gate=1.5, tail=0.1) for v in list(range(0, 128, 8)) + [127]]
    out += [line(f"e15_pw_{v:03d}", ens, 60, {**base, **quiet, 52: v}, gate=1.5, tail=0.1) for v in (0, 32, 64, 96, 127)]
    out += [line(f"e15_chrl_{v:03d}", ens, 60, {**base, 54: 64, 53: v}, gate=3.0, tail=0.1) for v in (0, 16, 32, 64, 96, 127)]
    out += [line(f"e15_chrw_{v:03d}", ens, 60, {**base, 53: 127, 54: v}, gate=3.0, tail=0.1) for v in (0, 32, 64, 96, 127)]
    out += [line(f"e15_tune_{v:03d}", ens, 60, {**base, **quiet, 55: v}, gate=1.5, tail=0.1) for v in (0, 32, 63, 64, 65, 96, 127)]
    return out


def ens16():
    out = ["# SWAVE-ENS, round two: every PCH2 value (CHRL 0, WAVE 0, PW 0), PW across at WAVE 0 and 127 (one voice), the"
           " chorus resolved: CHRL across at CHRW 64 and CHRW across at CHRL 127, 4 s, one voice and a fifth"]
    ens = 14
    clean = {60: 48, 51: 0, 52: 0, 53: 0, 54: 0}
    out += [line(f"e16_pch2_{v:03d}", ens, 60, {**clean, 48: v}, gate=1.2, tail=0.05) for v in range(128)]
    for w in (0, 127):
        out += [line(f"e16_pw_w{w:03d}_{v:03d}", ens, 60, {**clean, 51: w, 52: v}, gate=1.2, tail=0.05) for v in list(range(0, 128, 4)) + [127]]
    out += [line(f"e16_chrl_{v:03d}", ens, 60, {**clean, 54: 64, 53: v}, gate=4.0, tail=0.05) for v in (0, 8, 16, 32, 48, 64, 80, 96, 112, 127)]
    out += [line(f"e16_chrw_{v:03d}", ens, 60, {**clean, 53: 127, 54: v}, gate=4.0, tail=0.05) for v in (0, 16, 32, 48, 64, 80, 96, 112, 127)]
    out.append(line("e16_chr_fifth", ens, 60, {**clean, 48: 100, 53: 127, 54: 64}, gate=4.0, tail=0.05))
    return out


def ens17():
    out = ["# SWAVE-ENS, round three: what a voice is. The octave against the note (C3 C4 C5 at PW 64, WAVE 0: one saw, if"
           " the two resets sit half a period apart), WAVE x PW on a grid at C4, PCH2 voices at PW 64, the chorus there"]
    ens = 14
    clean = {60: 48, 53: 0, 54: 0}
    out += [line(f"e17_note{n}", ens, n, {**clean, 51: 0, 52: 64}, gate=1.2, tail=0.05) for n in (48, 60, 72)]
    out += [line(f"e17_w{w:03d}_p{p:03d}", ens, 60, {**clean, 51: w, 52: p}, gate=1.2, tail=0.05)
            for w in (0, 32, 64, 96, 127) for p in (0, 32, 64, 96, 127)]
    out += [line(f"e17_pch2_{v:03d}", ens, 60, {**clean, 51: 0, 52: 64, 48: v}, gate=1.2, tail=0.05) for v in (4, 40, 64, 100, 124)]
    out.append(line("e17_chorus", ens, 60, {60: 48, 51: 0, 52: 64, 53: 127, 54: 64}, gate=4.0, tail=0.05))
    return out


def ens18():
    out = ["# SWAVE-ENS, round four: the voices' level as they are added (one to four, PW 64), the page's defaults (each"
           " control left out in turn, against the same set explicitly), the chorus over 8 s at CHRL 127 (one voice,"
           " PW 64) for its movement, CHRW 0 and 127; a note played twice for the oscillators' phase at the trig"]
    ens = 14
    clean = {60: 48, 51: 0, 52: 64, 53: 0, 54: 0}
    for name, pch in (("v1", {}), ("v2", {48: 100}), ("v3", {48: 100, 49: 72}), ("v4", {48: 100, 49: 72, 50: 124}),
                      ("v2u", {48: 62}), ("v4u", {48: 62, 49: 62, 50: 62})):
        out.append(line(f"e18_{name}", ens, 60, {**clean, **pch}, gate=1.2, tail=0.05))
    for cc, nm in ((51, "wave"), (52, "pw"), (53, "chrl"), (54, "chrw"), (55, "tune")):
        rest = {k: v for k, v in {51: 0, 52: 64, 53: 0, 54: 0, 55: 64}.items() if k != cc}
        out.append(line(f"e18_def_{nm}", ens, 60, {60: 48, **rest}, gate=1.2, tail=0.05))
    for w in (0, 127):
        out.append(line(f"e18_chorus_w{w:03d}", ens, 60, {60: 48, 51: 0, 52: 64, 53: 127, 54: w}, gate=8.0, tail=0.05))
    out.append(line("e18_twice_a", ens, 60, clean, gate=0.6, tail=0.05))
    out.append(line("e18_twice_b", ens, 60, clean, gate=0.6, tail=0.05))
    return out


def ens19():
    out = ["# SWAVE-ENS's defaults, each line alone in its own run (render one line a process: a control carries over"
           " from the line before): nothing sent; WAVE alone sent; PW alone sent; then the chorus line with every control"]
    ens = 14
    out.append(line("e19_defaults", ens, 60, {60: 48}, gate=1.2, tail=0.05))
    out.append(line("e19_pw_default", ens, 60, {60: 48, 51: 0, 53: 0, 54: 0}, gate=1.2, tail=0.05))
    out.append(line("e19_wave_default", ens, 60, {60: 48, 52: 64, 53: 0, 54: 0}, gate=1.2, tail=0.05))
    out.append(line("e19_chorus", ens, 60, {60: 48, 48: 0, 49: 0, 50: 0, 51: 0, 52: 64, 53: 127, 54: 64, 55: 64}, gate=8.0, tail=0.05))
    return out


def ens20():
    out = ["# SWAVE-ENS for the regression suite: every line sends the whole page (PCH2 PCH3 PCH4 WAVE PW CHRL CHRW TUNE)."
           " Each PCH group's first and last value (PW 64: the voices at their notes), WAVE x PW, TUNE, one to four"
           " voices, the chorus by CHRL and by CHRW; a C4, DIST 48, the filter open"]
    ens = 14
    page = {48: 62, 49: 62, 50: 62, 51: 0, 52: 64, 53: 0, 54: 64, 55: 64}     # PCH 62: OFF (0-1 is an octave down)
    def ln(name, gate=1.2, **kw):
        cc = dict(page)
        cc.update({{"pch2": 48, "pch3": 49, "pch4": 50, "wave": 51, "pw": 52, "chrl": 53, "chrw": 54, "tune": 55}[k]: v for k, v in kw.items()})
        return line(name, ens, 60, {60: 48, **cc}, gate=gate, tail=0.05)
    firsts = [0, 2, 6, 10, 14, 18, 22, 26, 30, 33, 37, 41, 45, 49, 53, 57, 61, 64, 68, 72, 76, 80, 84, 88, 92, 95, 99,
              103, 107, 111, 115, 119, 123, 128]
    vals = sorted({v for a, b in zip(firsts, firsts[1:]) for v in (a, b - 1)})
    out += [ln(f"e20_pch2_{v:03d}", pch2=v) for v in vals]
    out += [ln(f"e20_w{w:03d}_p{p:03d}", wave=w, pw=p) for w in (0, 32, 64, 96, 127) for p in (0, 16, 32, 48, 64, 96, 127)]
    out += [ln(f"e20_tune_{v:03d}", tune=v) for v in (0, 32, 63, 64, 65, 96, 127)]
    out += [ln("e20_v2", pch2=100), ln("e20_v3", pch2=100, pch3=72), ln("e20_v4", pch2=100, pch3=72, pch4=124)]
    out += [ln(f"e20_chrl_{v:03d}", gate=4.0, chrl=v) for v in (0, 16, 32, 64, 96, 127)]
    out += [ln(f"e20_chrw_{v:03d}", gate=4.0, chrl=127, chrw=v) for v in (0, 64, 127)]
    return out


def order11():
    out = ["# is a render alone? One run, in this order: a resonant C3 sine fresh, the same again, again with the Q moved,"
           " a long tail after the gate (REL 0), another machine between, fresh again, another note; then a decaying"
           " sine into the clip (is the envelope after the drive?). Render as ONE chunk: the order is the point"]
    hp = {60: 16, 72: 16, 74: 127}
    out.append(line("o11_a", SIN, 48, hp, gate=3.0, tail=0.05))
    out.append(line("o11_b", SIN, 48, hp, gate=3.0, tail=0.05))
    out.append(line("o11_q96", SIN, 48, {**hp, 74: 96}, gate=3.0, tail=0.05))
    out.append(line("o11_tail", SIN, 48, hp, gate=1.0, tail=2.5))
    out.append(line("o11_rst", NOIS, 48, {}, gate=0.05, tail=0.05))
    out.append(line("o11_c", SIN, 48, hp, gate=3.0, tail=0.05))
    out.append(line("o11_n55", SIN, 55, hp, gate=3.0, tail=0.05))
    out.append(line("o11_rst2", NOIS, 48, {}, gate=0.05, tail=0.05))
    out.append(line("o11_dec", SIN, 36, {60: 127, 57: 0, 58: 60}, gate=2.0, tail=0.05))
    out.append(line("o11_dec_lin", SIN, 36, {60: 16, 57: 0, 58: 60}, gate=2.0, tail=0.05))
    return out


# ---- round six: DIST on loud and complex signals (the power test found MONO2K's drive 10 dB hotter than the
# Monomachine's on a saw with its subs, where the sine of rounds one to three agreed)

SAW, ENS = 4, 14
D21 = (64, 72, 76, 80, 84, 88, 96, 104, 112, 127)


def dist21():
    out = ["# DIST on loud and complex signals, at several input levels: a C3 sine (full, and cut by the EQ before the"
           " filter, as clip3), SWAVE-SAW's saw at C2 alone, with SUB1 at three levels, with both subs (the power test's"
           " bass), with its unison; a SWAVE-ENS chord of one, two and four voices at C3; noise. Every line sends the"
           " machine's whole page"]
    saw = {48: 0, 49: 0, 50: 0, 51: 0, 52: 0, 53: 0, 54: 0, 55: 64}
    ens = {48: 62, 49: 62, 50: 62, 51: 0, 52: 64, 53: 0, 54: 64, 55: 64}
    sigs = [("sin", SIN, 48, {}), ("sin_eq48", SIN, 48, {80: 28, 81: 48}), ("sin_eq32", SIN, 48, {80: 28, 81: 32}),
            ("saw", SAW, 36, saw), ("saw_s1_040", SAW, 36, {**saw, 53: 40}), ("saw_s1_080", SAW, 36, {**saw, 53: 80}),
            ("saw_s1_127", SAW, 36, {**saw, 53: 127}), ("saw_subs", SAW, 36, {**saw, 53: 110, 54: 80}),
            ("saw_uni", SAW, 36, {**saw, 48: 127, 49: 20, 50: 127}),
            ("ens1", ENS, 48, ens), ("ens2", ENS, 48, {**ens, 49: 100}), ("ens4", ENS, 48, {**ens, 48: 85, 49: 100, 50: 120}),
            ("nois", NOIS, 60, {})]
    for name, m, n, cc in sigs:
        out += [line(f"d21_{name}_d{d:03d}", m, n, {**cc, 60: d}, gate=1.0, tail=0.05) for d in D21]
    out.append("# and each other machine at its init page, DIST 64 / 96 / 112 / 127")
    for name, m, n in (("sid", 3, 36), ("sid48", 3, 48), ("fms", 8, 48), ("fmd", 10, 48), ("wav", 6, 48), ("vox", 11, 48),
                       ("fmp", 9, 48)):
        out += [line(f"d21m_{name}_d{d:03d}", m, n, {60: d}, gate=1.0, tail=0.05) for d in (64, 96, 112, 127)]
    return out


def fm26():
    """FM+STAT (8) and FM+DYN (10) on the Monomachine's pages: the carrier, each depth law, the ratio laws, FM+STAT's
    modulators' lowpass up the keyboard, the decays, DYN's 1VEN push and cut and its 1FEN sweep, the feedback, TUNE.
    Each line sends the whole page (a line keeps the controls of the one before)"""
    S = {48: 61, 49: 64, 50: 0, 51: 0, 52: 82, 53: 0, 54: 64, 55: 64}
    D = {48: 64, 49: 64, 50: 0, 51: 0, 52: 74, 53: 0, 54: 0, 55: 64}
    out = ["# FM+STAT: 1FRQ 1FIN 1ENV 1FB | 2FRQ 2VOL TONE TUNE (init 61 64 80 32 82 64 64 64)"]
    out.append(line("f26s_carrier", 8, 48, S))
    out += [line(f"f26s_i1_{v:03d}", 8, 48, {**S, 48: 125, 50: v}) for v in (16, 32, 48, 64)]
    out += [line(f"f26s_i2_{v:03d}", 8, 48, {**S, 52: 125, 53: v}) for v in (80, 112)]
    out += [line(f"f26s_lp_n{n}", 8, n, {**S, 48: 125, 50: 32}) for n in (60, 72, 84)]
    out += [line(f"f26s_r1_{i:02d}", 8, 48, {**S, 48: (i * 128 + 23) // 24, 50: 32}) for i in (6, 13, 17, 22)]
    out.append(line("f26s_fin_096", 8, 48, {**S, 48: 82, 49: 96, 50: 32}))
    out += [line(f"f26s_fb_{v:03d}", 8, 48, {**S, 48: 82, 50: 64, 51: v}) for v in (16, 32, 48, 64)]
    out += [line(f"f26s_env_{v:03d}", 8, 48, {**S, 48: 125, 50: v}, gate=1.6) for v in (88, 104)]
    out += [line(f"f26s_tone_{v:03d}", 8, 48, {**S, 48: 125, 50: 40, 54: v}) for v in (16, 32)]
    out.append("# FM+DYN: 1FRQ 1FEN 1VOL 1VEN | 2FRQ 2ENV 2FB TUNE (init 64 64 64 0 74 80 32 64)")
    out.append(line("f26d_carrier", 10, 48, D))
    out += [line(f"f26d_v1_{v:03d}", 10, 48, {**D, 48: 96, 50: v}) for v in (32, 64, 96, 120)]
    out += [line(f"f26d_e2_{v:03d}", 10, 48, {**D, 52: 127, 53: v}) for v in (32, 64)]
    out.append(line("f26d_d2_096", 10, 48, {**D, 52: 127, 53: 96}, gate=1.6))
    out += [line(f"f26d_r1_{v:03d}", 10, 48, {**D, 48: v, 50: 32}) for v in (16, 112)]
    out += [line(f"f26d_r2_{v:03d}", 10, 48, {**D, 52: v, 53: 32}) for v in (48, 111)]
    out.append(line("f26d_ven_088", 10, 48, {**D, 48: 127, 50: 16, 51: 88}, gate=1.6))
    out.append(line("f26d_ven_032", 10, 48, {**D, 48: 127, 50: 64, 51: 32}, gate=1.6))
    out.append(line("f26d_vp_088", 10, 48, {**D, 48: 96, 50: 16, 51: 88}, gate=1.6))
    out.append(line("f26d_vn_032", 10, 48, {**D, 48: 96, 50: 64, 51: 32}, gate=1.6))
    out += [line(f"f26d_fen_{v:03d}", 10, 48, {**D, 48: 96, 49: v, 50: 32}, gate=1.6) for v in (32, 96)]
    out.append(line("f26d_fen60_096", 10, 60, {**D, 48: 96, 49: 96, 50: 32}, gate=1.6))
    out += [line(f"f26d_fb_{v:03d}", 10, 48, {**D, 52: 64, 53: 64, 54: v}) for v in (16, 32, 48, 96)]
    out += [line(f"f26d_tune_{v:03d}", 10, 48, {**D, 55: v}) for v in (0, 127)]
    P = {48: 61, 49: 0, 50: 82, 51: 0, 52: 104, 53: 0, 54: 64, 55: 64}
    out.append("# FM+PAR: 1FRQ 1ENV 2FRQ 2ENV | 3FRQ 3ENV TONE TUNE (init 61 64 82 64 104 80 64 64)")
    out.append(line("f26p_carrier", 9, 48, P))
    for j in range(3):
        out += [line(f"f26p_i{j + 1}_{v:03d}", 9, 48, {**P, 48 + 2 * j: 125, 49 + 2 * j: v}) for v in (32, 64)]
    out += [line(f"f26p_r{j + 1}_13", 9, 48, {**P, 48 + 2 * j: (13 * 128 + 23) // 24, 49 + 2 * j: 32}) for j in range(3)]
    out += [line(f"f26p_e3_{v:03d}", 9, 48, {**P, 52: 125, 53: v}, gate=1.6) for v in (88, 104)]
    out.append(line("f26p_sum", 9, 48, {**P, 48: 82, 49: 64, 50: 82, 51: 64}))
    out += [line(f"f26p_tone_{v:03d}", 9, 48, {**P, 52: 125, 53: 40, 54: v}) for v in (16, 48, 96)]
    out += [line(f"f26s_tone_{v:03d}", 8, 72, {**S, 48: 125, 50: 40, 54: v}) for v in (56, 80)]
    return out


def sid36():
    """SID-6581 (3) on the Monomachine's page: the waves up the keyboard, PW's duty, PWAD's sweep, RING / SYNC / R+S
    against MFRQ (and MFRQ the same at every note), TUNE. Each line sends the page"""
    I = {48: 0, 49: 0, 50: 127, 51: 0, 52: 0, 53: 0, 54: 64, 55: 64}
    W = {"tri": 0, "saw": 40, "pul": 64, "mix": 90, "noi": 115}
    out = ["# SID-6581: PW PWAD PWRS WAVE | MOD MSRC MFRQ TUNE (init 0 0 127 0 0 0 64 64)"]
    out += [line(f"s36_w_{w}_n{n}", 3, n, {**I, 51: v}, gate=1.0) for w, v in W.items() for n in (36, 48, 72)]
    out += [line(f"s36_pw_{v:03d}", 3, 48, {**I, 51: 64, 48: v}, gate=1.0) for v in (16, 32, 48, 56, 96)]
    out += [line(f"s36_pwad_{v:03d}", 3, 48, {**I, 51: 64, 48: 64, 49: v}, gate=1.6) for v in (16, 64, 127)]
    for m, mv in (("ring", 32), ("sync", 64), ("rs", 100)):
        out += [line(f"s36_{m}_{f:03d}", 3, 48, {**I, 51: 40, 52: mv, 54: f}, gate=1.0) for f in (48, 80, 112)]
    out += [line(f"s36_ringtri_080", 3, 48, {**I, 51: 0, 52: 32, 54: 80}, gate=1.0)]
    out += [line(f"s36_mfrq_n{n}", 3, n, {**I, 51: 40, 52: 64, 54: 80}, gate=1.0) for n in (36, 60)]
    out += [line(f"s36_tune_{v:03d}", 3, 48, {**I, 55: v}, gate=1.0) for v in (0, 127)]
    return out


if __name__ == "__main__":
    if sys.argv[1] == "sid36":
        with open(os.path.join(sys.argv[2], "chain_sid36.spec"), "w") as fh:
            fh.write("\n".join(sid36()) + "\n")
    elif sys.argv[1] == "fm26":
        with open(os.path.join(sys.argv[2], "chain_fm26.spec"), "w") as fh:
            fh.write("\n".join(fm26()) + "\n")
    elif sys.argv[1] == "specs":
        d = sys.argv[2]
        for name, f in (("levels", levels), ("dist", dist), ("filt", filt), ("eq", eq), ("srr", srr), ("delay", delay)):
            with open(os.path.join(d, f"chain_{name}.spec"), "w") as fh:
                fh.write("\n".join(f()) + "\n")
    elif sys.argv[1] == "specs3":
        d = sys.argv[2]
        for name, f in (("clip3", clip3), ("map3", map3), ("fenv4", fenv4), ("ftime5", ftime5), ("res6", res6), ("hres8", hres8), ("drive10", drive10), ("order11", order11), ("drive12", drive12), ("tune13", tune13), ("sidbend14", sidbend14), ("ens15", ens15), ("ens16", ens16), ("ens17", ens17), ("ens18", ens18), ("ens19", ens19), ("ens20", ens20), ("dist21", dist21)):
            with open(os.path.join(d, f"chain_{name}.spec"), "w") as fh:
                fh.write("\n".join(f()) + "\n")
    elif sys.argv[1] == "specs2":
        d = sys.argv[2]
        for name, f in (("dist2", dist2), ("filt2", filt2), ("eq2", eq2), ("srr2", srr2), ("delay2", delay2)):
            with open(os.path.join(d, f"chain_{name}.spec"), "w") as fh:
                fh.write("\n".join(f()) + "\n")
