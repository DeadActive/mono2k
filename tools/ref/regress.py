#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The fidelity regression suite. Each case names a line of a tools/ref spec, a metric and a tolerance. `capture` reads
# the Monomachine's renders of those lines (tools/ref/mm_ref, an owner's OS image) and stores only the measured
# numbers in tools/ref/regress_ref.json: no audio, nothing from the OS image. `check` renders MONO2K with the same
# lines (host/m2k_ref), measures it the same way and prints PASS / FAIL / INFO per case, with a JSON result.
#   .venv/bin/python tools/ref/regress.py capture            from build/ref/mm_c* (the renders the chain specs made)
#   .venv/bin/python tools/ref/regress.py check [GROUP ...]  needs no OS image; exits 1 on a FAIL
#   .venv/bin/python tools/ref/regress.py gaps [GROUP ...] [--widen]   records the XFAIL cases' gaps (they only narrow)
# INFO cases are measured and reported, not asserted (below MONO2K's 16-bit floor, or past its RAM). XFAIL marks a
# known gap, with its reason. Its size is held in tools/ref/regress_gaps.json (MONO2K's error against the reference,
# per value, nothing else): an XFAIL whose error grows past that plus the tolerance FAILs, and so does one without a
# recorded gap. XPASS (the gap closed) fails too: the case is to be re-marked PASS. Recording gaps is a decision, as
# a capture is: the diff of regress_gaps.json shows what it accepted.
import json, os, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
from sweep import load, SR
from chain_anal import harm, psd

REF_JSON = os.path.join(HERE, "regress_ref.json")
GAPS_JSON = os.path.join(HERE, "regress_gaps.json")
SPECS = ["chain_levels", "chain_dist2", "chain_filt2", "chain_map3", "chain_clip3", "chain_eq2", "chain_srr2",
         "chain_delay", "chain_delay2", "chain_ftime5", "chain_res6", "chain_drive12", "chain_tune13", "chain_sidbend14"]
MM_DIR = {"chain_levels": "mm_clevels", "chain_dist2": "mm_cdist2", "chain_filt2": "mm_cfilt2", "chain_map3": "mm_cmap3",
          "chain_clip3": "mm_cclip3", "chain_eq2": "mm_ceq2", "chain_srr2": "mm_csrr2", "chain_delay": "mm_cdelay",
          "chain_delay2": "mm_cdelay2", "chain_ftime5": "mm_cftime5", "chain_res6": "mm_cres6",
          "chain_drive12": "mm_cdrive12", "chain_tune13": "mm_ctune13",
          "chain_sidbend14": "mm_csidbend14"}


def db(v):
    return float(20 * np.log10(max(abs(v), 1e-12)))


def f0_of(note):
    return 440.0 * 2 ** ((note - 69) / 12)


# ---------------------------------------------------------------------------------------------------- metrics
# each takes (render, args, loader) and returns a list of floats; loader(name) gives another render of the same set

def m_rms(x, a, ld):                      # dB per channel over the held part
    t0, t1 = a.get("t", (0.3, 1.1))
    s = x[int(t0 * SR):int(t1 * SR)]
    return [db(np.sqrt(np.mean(s[:, 0] ** 2))), db(np.sqrt(np.mean(s[:, 1] ** 2)))]


def m_h1(x, a, ld):                       # the fundamental, dB
    return [db(harm(x, f0_of(a["note"]), 1, 0.3, 16384)[0])]


def m_h1_rel(x, a, ld):                   # the fundamental against another render's (the open chain)
    f0 = f0_of(a["note"])
    return [db(harm(x, f0, 1, 0.3, 16384)[0]) - db(harm(ld(a["ref"]), f0, 1, 0.3, 16384)[0])]


def m_harm(x, a, ld):                     # h1 dB, then h3 and h5 against it (floored at -60: clean is clean)
    h = harm(x, f0_of(a["note"]), 5, a.get("t", 0.3), 16384)
    return [db(h[0]), max(-60.0, db(h[2]) - db(h[0])), max(-60.0, db(h[4]) - db(h[0]))]


def m_tone_rel(x, a, ld):                 # a sine wherever it sits (parabolic peak), dB against another render's
    def tone(y):
        s = y[int(0.4 * SR):int(0.9 * SR), 0]
        sp = np.abs(np.fft.rfft(s * np.hanning(len(s)), 8 * len(s)))
        k = int(np.argmax(sp))
        l0, l1, l2 = np.log(sp[k - 1]), np.log(sp[k]), np.log(sp[k + 1])
        return l1 - 0.25 * (l0 - l2) ** 2 / (l0 - 2 * l1 + l2) / 2
    return [float((tone(x) - tone(ld(a["ref"]))) * 20 / np.log(10))]


def m_pitch_c(x, a, ld):                  # the sine's pitch, cents from the note
    s = x[int(0.4 * SR):int(0.9 * SR), 0]
    sp = np.abs(np.fft.rfft(s * np.hanning(len(s)), 8 * len(s)))
    k = int(np.argmax(sp))
    l0, l1, l2 = np.log(sp[k - 1]), np.log(sp[k]), np.log(sp[k + 1])
    fq = (k + 0.5 * (l0 - l2) / (l0 - 2 * l1 + l2)) * SR / (8 * len(s))
    return [float(1200 * np.log2(fq / f0_of(a["note"])))]


def m_bend_c(x, a, ld):                  # a short note's pitch over its audible part (harmonic comb), cents from the note
    sys.path.insert(0, os.path.join(HERE, ".."))
    import demo_pitch as D
    y = x[:, 0]
    e = np.convolve(np.abs(y), np.ones(64) / 64, "same")
    o = int(np.argmax(e > e.max() * 0.05))
    seg = y[o + int(0.015 * SR):o + int(0.35 * SR)]
    sm = np.convolve(np.abs(seg), np.ones(256) / 256, "same")
    k = np.nonzero(sm > sm.max() * 0.03)[0]
    sp, df = D.spectrum(seg[k[0]:k[-1]])
    return [D.comb(sp, df, f0_of(a["note"]))[0]]


def m_thd(x, a, ld):
    h = harm(x, f0_of(a["note"]), 7, 0.3, 16384)
    return [db(np.sqrt(np.sum(h[1:] ** 2))) - db(h[0])]


def m_peak_hz(x, a, ld):                  # the resonance's peak on noise, parabolic between bins
    f, p = psd(x, 0.3, 2.2, 32768)
    k = int(np.argmax(np.where(f > 15, p, 0)))
    y0, y1, y2 = np.log(p[k - 1] + 1e-30), np.log(p[k] + 1e-30), np.log(p[k + 1] + 1e-30)
    return [float((k + 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2)) * SR / 32768)]


def _ratio(x, r, t1, smooth):
    f, p = psd(x, 0.3, t1, 16384)
    _, q = psd(r, 0.3, t1, 16384)
    k = np.ones(smooth) / smooth
    return f, 10 * np.log10(np.convolve(p, k, "same") / np.convolve(q, k, "same"))


def m_res_db(x, a, ld):                   # the response's peak against the open chain, noise with headroom
    f, r = _ratio(x, ld(a["ref"]), 3.2, 9)
    m = (f > 100) & (f < 16000)
    return [float(np.max(np.where(m, r, -999)))]


def m_resp_at(x, a, ld):                  # the response against the open chain at given frequencies
    f, r = _ratio(x, ld(a["ref"]), 3.2, 15)
    return [float(r[np.argmin(np.abs(f - q))]) for q in a["hz"]]


def m_srr_image(x, a, ld):                # the sine's first image (R - f0) near where it should be: Hz, dB under the sine
    s = x[int(0.15 * SR):int(0.15 * SR) + 16384, 0]
    sp = np.abs(np.fft.rfft(s * np.hanning(len(s))))
    f = np.fft.rfftfreq(len(s), 1 / SR)
    sel = np.abs(f / a["near"] - 1) < 0.03
    k = int(np.argmax(np.where(sel, sp, 0)))
    return [float(f[k]), db(sp[k] / sp.max())]


def m_level_at(x, a, ld):                 # dB under the strongest component, at given Hz (+-3 bins)
    s = x[int(0.15 * SR):int(0.15 * SR) + 32768, 0]
    sp = np.abs(np.fft.rfft(s * np.hanning(len(s))))
    f = np.fft.rfftfreq(len(s), 1 / SR)
    out = []
    for q in a["hz"]:
        k = int(np.argmin(np.abs(f - q)))
        out.append(max(-80.0, db(sp[k - 3:k + 4].max() / sp.max())))
    return out


def _env(x, n=44):
    return np.sqrt(np.convolve(x[:, 0] ** 2 + x[:, 1] ** 2, np.ones(n) / n, "same") / 2)


def m_echo_ms(x, a, ld):                  # from the dry onset to the echo's, both at 10 % of their peaks
    e = _env(x)
    on = int(np.argmax(e > 0.1 * e.max()))
    lo, hi = int((a["at"] - 20) / 1000 * SR), int((a["at"] + 60) / 1000 * SR)
    seg = e[lo:hi]
    return [float((int(np.argmax(seg > 0.1 * seg.max())) + lo - on) / SR * 1000)]


def m_echoes(x, a, ld):                   # each echo's level against the dry blip, per channel, at its expected time
    n = 220
    el = np.sqrt(np.convolve(x[:, 0] ** 2, np.ones(n) / n, "same"))
    er = np.sqrt(np.convolve(x[:, 1] ** 2, np.ones(n) / n, "same"))
    e = np.sqrt((el ** 2 + er ** 2) / 2)
    dry = e[:int(0.05 * SR)].max()
    out = []
    for t in a["at"]:
        lo, hi = int((t - 20) / 1000 * SR), int((t + 30) / 1000 * SR)
        j = lo + int(np.argmax(e[lo:hi]))
        out += [max(-60.0, db(el[j] / dry)), max(-60.0, db(er[j] / dry))]     # silence floored
    return out


def m_echo_bands(x, a, ld):               # the first echo's spectrum against the dry blip's, in octave bands
    def spec(s):
        n = 4096
        w = np.hanning(n)
        return np.mean([np.abs(np.fft.rfft(s[i:i + n] * w)) ** 2 for i in range(0, len(s) - n, n // 4)], axis=0)
    s = x[:, 0]
    d, e = spec(s[:int(0.22 * SR)]), spec(s[int(a["at"] * SR):int((a["at"] + 0.22) * SR)])
    f = np.fft.rfftfreq(4096, 1 / SR)
    out = []
    for c in a["hz"]:
        m = (f >= c / 2 ** 0.5) & (f < c * 2 ** 0.5)
        out.append(float(10 * np.log10(e[m].mean() / d[m].mean())))
    return out


def m_fenv_ms(x, a, ld):                  # when the sine crosses -3.8 dB as the swept cutoff passes it
    n = 441
    e = 20 * np.log10(np.maximum(np.sqrt(np.convolve(x[:, 0] ** 2, np.ones(n) / n, "same")), 1e-9))
    if a["kind"] == "dec":
        lvl, start = e - e[int(0.003 * SR):int(0.02 * SR)].max(), int(0.02 * SR)
        idx = np.where(lvl[start:int(5.95 * SR)] < -3.8)[0]
    else:
        lvl, start = e - e[int(5.5 * SR):int(5.9 * SR)].mean(), int(0.005 * SR)
        idx = np.where(lvl[start:int(5.95 * SR)] > -3.8)[0]
    return [float((idx[0] + start) / SR * 1000) if len(idx) else float("nan")]


def m_res_fit(x, a, ld):                  # the resonance resolved (resfit.py): its peak against the cutoff, %, and height, dB
    from resfit import psd as rpsd
    f, p = rpsd(x)
    _, r = rpsd(ld(a["ref"]))
    ratio = p / np.maximum(r, 1e-30)
    fc = f0_of(a["note"]) * 2 ** ((a["step"] - 16) / 8)
    fp, q, g, e = _resfit(f, ratio, fc, a["hp"])
    return [(fp / fc - 1) * 100, g]


def _resfit(f, ratio, fc, hp):
    from resfit import N
    k = int(np.argmax(np.where(np.abs(f / fc - 1) < 0.15, np.convolve(ratio, np.ones(5) / 5, "same"), 0)))
    best = None
    for q in np.exp(np.linspace(np.log(2), np.log(3000), 160)):
        bw = f[k] / q
        sel = np.abs(f - f[k]) < max(6 * bw, 40)
        for fp in f[k] + np.linspace(-3, 3, 61) * (SR / N):
            m = 1 / (1 + (2 * q * (f[sel] - fp) / fp) ** 2)
            aa = np.dot(m, ratio[sel]) / np.dot(m, m)
            e = np.mean((np.log(np.maximum(aa * m, 1e-30)) - np.log(np.maximum(ratio[sel], 1e-30))) ** 2)
            if best is None or e < best[0]:
                best = (e, fp, q, aa)
    e, fp, q, aa = best
    pb = ratio[(f > fc * 1.6) & (f < min(fc * 2.1, 17000))] if hp else ratio[(f > fc / 4) & (f < fc / 2.5)]
    return fp, q, 10 * np.log10(aa / np.median(pb)), e


METRICS = {"res_fit": m_res_fit, "rms": m_rms, "h1": m_h1, "h1_rel": m_h1_rel, "harm": m_harm, "thd": m_thd, "peak_hz": m_peak_hz,
           "res_db": m_res_db, "resp_at": m_resp_at, "srr_image": m_srr_image, "level_at": m_level_at, "echo_ms": m_echo_ms,
           "echoes": m_echoes, "echo_bands": m_echo_bands, "fenv_ms": m_fenv_ms, "tone_rel": m_tone_rel, "pitch_c": m_pitch_c, "bend_c": m_bend_c}


# ------------------------------------------------------------------------------------------------------ cases
# (group, spec, render, metric, args, tolerance per value or one for all, "rel" for a fraction, info: not asserted)

def cases():
    C = []
    def add(group, spec, name, metric, args, tol, rel=False, info=False, xfail=None):
        C.append(dict(group=group, spec=spec, name=name, metric=metric, args=args, tol=tol, rel=rel, info=info, xfail=xfail))
    sat = ("the reference's HP saturates inside its resonance: from h1 ~-16 dBFS out (DIST <= 64, any Q) its output is a"
           " square through a high-pass, scaled by the DIST ceiling; MONO2K's clamped SVF stays near clean (chain_drive12)")
    vary = "the reference's Q at 96 / 127 varies with the cutoff (C6 against C4): one table fits C6"
    for v in (8, 32, 64, 96, 120, 127):
        add("levels", "chain_levels", f"vol_{v:03d}", "rms", {}, 0.1)
    for v in (16, 64, 127):
        add("levels", "chain_levels", f"lev_{v:03d}", "rms", {}, 0.1)
    for v in (0, 16, 32, 48, 64, 80, 96, 112, 127):
        add("levels", "chain_levels", f"pan_{v:03d}", "rms", {}, 0.1)
    for d in range(32, 64, 8):                          # the headroom, down to -60 dB
        add("dist", "chain_dist2", f"dd_{d:03d}", "h1", {"note": 36}, 0.3)
    for d in (8, 16, 24):                               # below: MONO2K's machines are 16-bit, a step is ~-90 dB
        add("dist", "chain_dist2", f"dd_{d:03d}", "h1", {"note": 36}, 0.3, info=True)
    for d in (64, 68, 72):                              # clean
        add("dist", "chain_dist2", f"dd_{d:03d}", "h1", {"note": 36}, 0.1)
    for d in range(76, 128, 4):                         # the clip
        add("dist", "chain_dist2", f"dd_{d:03d}", "harm", {"note": 36}, [0.4, 1.5, 1.5])
    for v in range(0, 31, 2):
        add("filter_mag", "chain_filt2", f"hp_{v:03d}", "h1_rel", {"note": 48, "ref": "hp_000"}, 0.5)
    for v in range(32, 45, 2):                          # the HP's far stopband: MONO2K's 1-2 dB steeper
        add("filter_mag", "chain_filt2", f"hp_{v:03d}", "h1_rel", {"note": 48, "ref": "hp_000"}, 0.5,
            xfail="HP stopband below -40 dB is 0.5-2 dB steeper than the reference")
    for v in range(46, 57, 2):
        add("filter_mag", "chain_filt2", f"hp_{v:03d}", "h1_rel", {"note": 48, "ref": "hp_000"}, 0.5, info=True)
    for v in range(0, 65, 2):
        add("filter_mag", "chain_filt2", f"lp_{v:03d}", "h1_rel", {"note": 48, "ref": "lp_064"}, 0.5)
    for v in range(0, 57, 8):
        add("res_map", "chain_map3", f"m3_lp_{v:03d}", "peak_hz", {}, 0.01, rel=True)
        add("res_map", "chain_map3", f"m3_hp_{v:03d}", "peak_hz", {}, 0.01, rel=True)
    for n in (24, 36, 48, 72, 96):
        add("res_map", "chain_map3", f"m3_note_{n:03d}", "peak_hz", {}, 0.01, rel=True)
        add("res_map", "chain_map3", f"m3_hnote_{n:03d}", "peak_hz", {}, 0.01, rel=True)
    for q in range(16, 121, 8):
        add("res_level", "chain_filt2", f"qn_lp_{q:03d}", "res_db", {"ref": "qn_open"}, 2.5,
            xfail="LPQ 120 on C4 noise: MONO2K ~4 dB under (" + vary + ")" if q == 120 else None)
    for q in range(32, 121, 8):
        add("res_level", "chain_filt2", f"qn_hp_{q:03d}", "res_db", {"ref": "qn_open"}, 2.5)
    for kind, step in (("hp", 40), ("lp", 40)):          # the resonance resolved: where its peak sits, how high
        for q in (64, 80, 96, 112, 127):
            add("res_fit", "chain_res6", f"r6_{kind}_n84_{q:03d}", "res_fit",
                {"ref": "r6_open_n84", "note": 84, "step": step, "hp": kind == "hp"}, [0.05, 1.5] if q >= 96 else [0.6, 2.0],
                xfail="HPQ 96's peak 0.07 % over the reference's (C6)" if (kind, q) == ("hp", 96) else None)
    for kind in ("hp", "lp"):
        for q in (96, 127):
            add("res_fit", "chain_res6", f"r6_{kind}_n60_{q:03d}", "res_fit",
                {"ref": "r6_open_n60", "note": 60, "step": 48, "hp": kind == "hp"}, [0.05, 1.5],
                xfail=None if (kind, q) == ("hp", 127) else vary)
    for d in (32, 48, 64, 72, 80, 96, 127):              # resonance and drive: the clip holds the LP's resonance; at 32
        # LPQ 127 at 131 Hz is still building at 0.8 s, and the reference's run carried it from the line before
        add("clip", "chain_clip3", f"c3_res_{d:03d}", "harm", {"note": 48}, [0.7, 1.5, 2.0], info=d == 32)
    for d in (80, 96, 126):
        add("clip", "chain_clip3", f"c3_lpf_{d:03d}", "harm", {"note": 48}, [0.7, 1.5, 2.0])
    for d in (80, 96, 124):
        add("clip", "chain_clip3", f"c3_lp24_{d:03d}", "harm", {"note": 48}, [0.7, 1.5, 2.0])
    for d in (80, 96, 127):
        add("clip", "chain_dist2", f"dclip_lp_{d:03d}", "harm", {"note": 36}, [0.7, 1.5, 2.0])
        add("clip", "chain_dist2", f"dclip_hp_{d:03d}", "harm", {"note": 36}, [0.7, 1.5, 2.0], xfail=sat)
    for d in (16, 32, 40, 48, 64, 72, 80, 96, 127):   # 0.8 s into a 2.9 s build-up, and rendered one after another:
        # the reference's resonance carries from the line before (chain_order11), so these are not comparable
        add("hp_drive", "chain_clip3", f"c3_hres_{d:03d}", "harm", {"note": 48}, [1.0, 3.0, 3.0], info=True)
    floor = dict(info=True)                           # h3 / h5 near -105 dBFS: under MONO2K's 16-bit floor
    def d12(name, note=48, **kw):
        add("hp_sat", "chain_drive12", name, "harm", {"note": note, "t": 1.5}, [1.0, 2.0, 2.0], **kw)
    for q, d in ((0, 32), (32, 32)):
        d12(f"d12_b16_q{q:03d}_n48_d{d:03d}", **floor)
    d12("d12_b16_q000_n48_d064")
    for q in (16,):
        for d in (32, 64):
            d12(f"d12_b16_q{q:03d}_n48_d{d:03d}", xfail="HPQ 16 at the cutoff: MONO2K 1.3-1.6 dB quieter")
    d12("d12_b16_q032_n48_d056", xfail="HPQ 32 at the cutoff, C3: MONO2K 1.35 dB quieter (C4 noise wants no more Q)")
    for q, d in ((48, 32), (48, 48), (64, 32), (64, 40)):               # linear: the level at the cutoff
        d12(f"d12_b16_q{q:03d}_n48_d{d:03d}")
    for d in (32, 48):
        d12(f"d12_b16_q048_n72_d{d:03d}", note=72)
    d12("d12_b16_q096_n48_d016")
    d12("d12_b16_q096_n48_d032", xfail="HPQ 96 at the cutoff, C3, DIST 32: MONO2K 2 dB louder, near the saturation")
    for q, ds in ((32, (60, 64)), (48, (52, 56, 64)), (64, (48, 56, 64)), (96, (40, 48, 64))):   # saturated
        for d in ds:
            d12(f"d12_b16_q{q:03d}_n48_d{d:03d}", xfail=sat)
    for d in (56, 64):
        d12(f"d12_b16_q048_n72_d{d:03d}", note=72, xfail=sat)
    for q in (48, 96):                                # the cutoff an octave under the note (passband), over it (stopband)
        d12(f"d12_b08_q{q:03d}_n48_d064", xfail="the passband at HPQ 48 / 96, DIST 64: MONO2K 1.8 dB louder")
        d12(f"d12_b24_q{q:03d}_n48_d064", xfail="an octave under a resonant HP's cutoff the reference falls 7.6-9.4 dB further")
    for kind in ("lp", "hp"):                         # the filter follows GND-SIN's TUNE (chain_tune13); the reference
        for t in (32, 48, 56, 72, 80, 95):            # itself scatters +-0.5 dB, a fixed filter loses 2 dB at 25 c
            add("tune", "chain_tune13", f"t13_{kind}_{t:03d}", "tone_rel", {"ref": f"t13_{kind}_064"}, 1.0)
        for t in (32, 48, 80, 95):
            add("tune", "chain_tune13", f"t13_{kind}_{t:03d}", "pitch_c", {"note": 60}, 1.0)
    for kind, tol in (("res", 6.0), ("nores", 1.5), ("open", 1.5)):   # a resonant sweep bends a short note flat:
        for n in (45, 43, 41, 40, 52, 48, 57, 55):                    # the Monomachine's too (chain_sidbend14)
            add("bend", "chain_sidbend14", f"s14_{kind}_n{n}", "bend_c", {"note": n}, tol)
    hz = [60, 150, 400, 800, 1600, 3200, 6400]
    top = "the EQ's top band: the reference's skirts stay wide toward Nyquist, MONO2K's warp narrow"
    for v in (0, 16, 32, 48, 64, 80, 96, 112, 127):
        add("eq", "chain_eq2", f"e2_f_{v:03d}_up", "resp_at", {"ref": "e2_open", "hz": hz}, 2.5, xfail=top if v >= 112 else None)
    for v in (12, 28, 44, 60, 76, 92, 108):
        add("eq", "chain_eq2", f"e2_f_{v:03d}_dn", "resp_at", {"ref": "e2_open", "hz": hz}, 2.5, xfail=top if v >= 108 else None)
    for v in (0, 16, 32, 48, 80, 96, 112, 127):
        add("eq", "chain_eq2", f"e2_g_{v:03d}", "resp_at", {"ref": "e2_open", "hz": hz}, 2.5)
    period = {32: 4.706, 48: 6.589, 64: 8.471, 80: 10.182, 96: 12.003, 112: 14.0, 127: 15.677}   # measured (README)
    for v, p in period.items():
        add("srr", "chain_srr2", f"s2_{v:03d}", "srr_image", {"near": round(SR / p - f0_of(84))}, ["1%", 1.5])
    add("srr", "chain_srr2", "s2_032", "level_at", {"hz": [1709]}, 3.0,
        xfail="the hold's fractional period leaves a spur (1709 Hz on a C6 at SRR 32) 10 dB stronger than the reference's")
    for v, T in ((8, 62.5), (16, 125), (32, 250), (48, 375), (64, 500), (96, 750), (127, 992.2)):
        add("delay_time", "chain_delay", f"d_tim_{v:03d}", "echo_ms", {"at": T}, 1.0)
    for v in (0, 16, 32, 48, 80, 96, 112, 127):
        add("delay_send", "chain_delay", f"d_snd_{v:03d}", "echoes", {"at": [258]}, 0.5)
    for v in (56, 64):
        add("delay_fb", "chain_delay2", f"d2_fb_{v:03d}", "echoes", {"at": [132, 257]}, 0.5)
    add("delay_fb", "chain_delay2", "d2_fb_068", "echoes", {"at": [132, 257]}, 0.5)   # the "growth" was a control
                                                                                     # carried from the line before
    for s in (0, 32):
        add("delay_pan", "chain_delay2", f"d2_pan_s{s:03d}_f048", "echoes", {"at": [258, 515]}, 1.0)
    for s in (96, 127):                                 # ping-pong: the sum and difference lines leak at -32 dB
        add("delay_pan", "chain_delay2", f"d2_pan_s{s:03d}_f048", "echoes", {"at": [258, 515]}, 1.0,
            xfail="ping-pong leaks ~-32 dB to the wrong side (the mu-law sum / difference lines)")
    for n in ("d2_bas_032", "d2_bas_064", "d2_bas_096", "d2_wid_032", "d2_wid_064", "d2_wid_096", "d2_bw_032_032", "d2_default"):
        add("delay_filter", "chain_delay2", n, "echo_bands", {"at": 0.379, "hz": [125, 250, 500, 1000, 2000, 4000]}, 3.0,
            xfail="375 ms ping-pong runs past MONO2K's half line: the slowed clock darkens 2-4 kHz" if n == "d2_bas_096" else None)
        add("delay_filter", "chain_delay2", n, "echo_bands", {"at": 0.379, "hz": [8000, 16000]}, 3.0, info=True)
    for d in (32, 48, 64, 80, 96):
        add("fenv", "chain_ftime5", f"ft_dec_{d:03d}", "fenv_ms", {"kind": "dec"}, 0.05, rel=True)
    for v in (80, 96, 112):
        add("fenv", "chain_ftime5", f"ft_atk_{v:03d}", "fenv_ms", {"kind": "atk"}, 0.08, rel=True)
    add("fenv", "chain_ftime5", "ft_atk_064", "fenv_ms", {"kind": "atk"}, 0.08, rel=True,
        xfail="the reference's attack starts slower than a straight line (crossing 15 % of it at 26 ms, MONO2K 19 ms)")
    return C


def key(c):
    return f'{c["group"]}:{c["name"]}:{c["metric"]}:{json.dumps(c["args"], sort_keys=True)}'


def measure(c, d):
    cache = {}
    def ld(n):
        if n not in cache:
            cache[n] = load(os.path.join(d, n + ".wav"))
        return cache[n]
    return METRICS[c["metric"]](ld(c["name"]), c["args"], ld)


def capture():
    out = {"source": "Monomachine OS 1.32B in gearmulator-md-mm (tools/ref/mm_ref), MIDI only; measured numbers only",
           "cases": {}}
    for c in cases():
        d = os.path.join(ROOT, "build", "ref", MM_DIR[c["spec"]])
        out["cases"][key(c)] = [round(v, 4) for v in measure(c, d)]
    with open(REF_JSON, "w") as f:
        json.dump(out, f, indent=0, sort_keys=True)
    print(f"captured {len(out['cases'])} cases into {os.path.relpath(REF_JSON, ROOT)}")


def render_m2k(cs, d):
    os.makedirs(d, exist_ok=True)
    want = {(c["spec"], c["name"]) for c in cs} | {(c["spec"], c["args"]["ref"]) for c in cs if "ref" in c["args"]}
    lines = []
    for spec in sorted({s for s, _ in want}):
        for l in open(os.path.join(HERE, spec + ".spec")):
            if l.strip() and not l.startswith("#") and (spec, l.split()[0]) in want:
                lines.append(l)
    subprocess.run(["cc", "-O2", "-Ifirmware/src", "-Ifirmware/gen", "-o", "build/host/m2k_ref", "host/m2k_ref.c", "-lm"],
                   cwd=ROOT, check=True)
    subprocess.run([os.path.join(ROOT, "build/host/m2k_ref"), d], input="".join(lines), text=True, cwd=ROOT,
                   check=True, stdout=subprocess.DEVNULL)


def errors(c, r, m):
    """MONO2K's error against the reference, one per value (absolute, or a fraction where the case is relative);
    NaN where both are NaN, inf where only one is"""
    tol = c["tol"] if isinstance(c["tol"], list) else [c["tol"]] * len(r)
    out = []
    for a, b, t in zip(r, m, tol):
        if np.isnan(a) or np.isnan(b):
            out.append(float("nan") if np.isnan(a) and np.isnan(b) else float("inf"))
            continue
        rel = c["rel"] or isinstance(t, str)
        out.append(float(abs(b - a) / abs(a) if rel else abs(b - a)))
    return out, [float(t.rstrip("%")) / 100 if isinstance(t, str) else t for t in tol]


def run(groups):
    ref = json.load(open(REF_JSON))["cases"]
    cs = [c for c in cases() if not groups or c["group"] in groups]
    d = os.path.join(ROOT, "build", "ref", "regress_m2k")
    render_m2k(cs, d)
    out = []
    for c in cs:
        r = ref.get(key(c))
        out.append((c, r, None if r is None else measure(c, d)))
    return out


def gaps(groups, widen=False):
    """a new known gap is recorded as it stands; a recorded one only narrows, unless widen (--widen) says otherwise"""
    old = json.load(open(GAPS_JSON))["cases"] if os.path.exists(GAPS_JSON) else {}
    new = {k: v for k, v in old.items()}
    n, kept = 0, []
    for c, r, m in run(groups):
        if not c["xfail"] or r is None:
            continue
        e, tol = errors(c, r, m)
        g = [round(v, 4) if np.isfinite(v) else None for v in e]
        was = old.get(key(c))
        if was is not None and not widen:
            g = [w if w is not None and (v is None or v > w) else v for v, w in zip(g, was)]
            if g != [round(v, 4) if np.isfinite(v) else None for v in e]:
                kept.append(c["name"])
        new[key(c)] = g
        n += 1
    if kept:
        print("held at their recorded size (grown since; --widen to accept):", " ".join(kept))
    live = {key(c) for c in cases() if c["xfail"]}
    new = {k: v for k, v in new.items() if k in live}
    with open(GAPS_JSON, "w") as f:
        json.dump({"note": "MONO2K's error against the reference per known gap (regress.py gaps); differences only",
                   "cases": new}, f, indent=0, sort_keys=True)
    print(f"recorded {n} gaps into {os.path.relpath(GAPS_JSON, ROOT)} ({len(new)} held)")


def check(groups):
    known = json.load(open(GAPS_JSON))["cases"] if os.path.exists(GAPS_JSON) else {}
    rows, fails = [], 0
    for c, r, m in run(groups):
        if r is None:
            rows.append((c, None, None, "BLOCKED", ""))
            continue
        e, tol = errors(c, r, m)
        ok = all(np.isnan(v) or v <= t for v, t in zip(e, tol))
        why = ""
        if c["info"]:
            st = "INFO"
        elif c["xfail"]:
            g = known.get(key(c))
            if ok:
                st, why = "XPASS", "the known gap closed: re-mark the case PASS"
            elif g is None:
                st, why = "FAIL", "a known gap without a recorded size (regress.py gaps)"
            elif any(not np.isnan(v) and v > (float("inf") if gv is None else gv) + t for v, gv, t in zip(e, g, tol)):
                st, why = "FAIL", "the known gap grew: " + " ".join(f"{v:.3g}>{gv}+{t:.3g}" for v, gv, t in zip(e, g, tol)
                                                                     if not np.isnan(v) and gv is not None and v > gv + t)
            else:
                st = "XFAIL"
        else:
            st = "PASS" if ok else "FAIL"
        fails += st in ("FAIL", "XPASS")
        rows.append((c, r, m, st, why))
    res = []
    last = None
    for c, r, m, st, why in rows:
        if c["group"] != last:
            print(f"== {c['group']}")
            last = c["group"]
        if r is None:
            print(f"  {st:5s} {c['name']:18s} no reference measurement")
        else:
            print(f"  {st:5s} {c['name']:18s} {c['metric']:10s} MM " + " ".join(f"{v:8.2f}" for v in r) +
                  "   M2K " + " ".join(f"{v:8.2f}" for v in m) + (f"   ({why})" if why else ""))
        res.append(dict(group=c["group"], name=c["name"], metric=c["metric"], args=c["args"], tol=c["tol"],
                        rel=c["rel"], mm=r, m2k=m, status=st, xfail=c["xfail"], why=why))
    ST = ("PASS", "FAIL", "XFAIL", "XPASS", "INFO", "BLOCKED")
    summary = {s: sum(1 for x in res if x["status"] == s) for s in ST}
    by_group = {}
    for x in res:
        g = by_group.setdefault(x["group"], {s: 0 for s in ST})
        g[x["status"]] += 1
    with open(os.path.join(ROOT, "build", "ref", "regress_result.json"), "w") as f:
        json.dump({"summary": summary, "groups": by_group, "cases": res}, f, indent=1)
    print("groups:", ", ".join(f"{g} {v['PASS']}/{v['PASS'] + v['FAIL']}" + "".join(f" {v[s]} {s.lower()}" for s in ("XFAIL", "XPASS", "INFO") if v[s])
                                for g, v in by_group.items()))
    for x in res:
        if x["status"] == "XPASS":
            print(f"XPASS {x['group']}:{x['name']}: the known gap no longer shows, re-mark it PASS ({x['xfail']})")
        elif x["status"] == "FAIL" and x["why"]:
            print(f"FAIL  {x['group']}:{x['name']}: {x['why']} ({x['xfail']})")
    print("summary:", summary)
    return 1 if fails else 0


if __name__ == "__main__":
    if sys.argv[1:2] == ["capture"]:
        capture()
    elif sys.argv[1:2] == ["check"]:
        sys.exit(check(set(sys.argv[2:])))
    elif sys.argv[1:2] == ["gaps"]:
        gaps({a for a in sys.argv[2:] if a != "--widen"}, "--widen" in sys.argv[2:])
    else:
        print("regress.py capture | check [GROUP ...] | gaps [GROUP ...]")
