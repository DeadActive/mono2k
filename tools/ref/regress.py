#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The fidelity regression suite. Each case names a line of a tools/ref spec, a metric and a tolerance. `capture` reads
# the Monomachine's renders of those lines (tools/ref/mm_ref, an owner's OS image) and stores only the measured
# numbers in tools/ref/regress_ref.json: no audio, nothing from the OS image. `check` renders MONO2K with the same
# lines (host/m2k_ref), measures it the same way and prints PASS / FAIL / INFO per case, with a JSON result.
#   .venv/bin/python tools/ref/regress.py capture            from build/ref/mm_c* (the renders the chain specs made)
#   .venv/bin/python tools/ref/regress.py check [GROUP ...]  needs no OS image; exits 1 on a FAIL
# INFO cases are measured and reported, not asserted (below MONO2K's 16-bit floor, or past its RAM). XFAIL marks a
# known gap, with its reason: reported, not failing the suite, and XPASS once it closes.
import json, os, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
from sweep import load, SR
from chain_anal import harm, psd

REF_JSON = os.path.join(HERE, "regress_ref.json")
SPECS = ["chain_levels", "chain_dist2", "chain_filt2", "chain_map3", "chain_clip3", "chain_eq2", "chain_srr2",
         "chain_delay", "chain_delay2", "chain_ftime5"]
MM_DIR = {"chain_levels": "mm_clevels", "chain_dist2": "mm_cdist2", "chain_filt2": "mm_cfilt2", "chain_map3": "mm_cmap3",
          "chain_clip3": "mm_cclip3", "chain_eq2": "mm_ceq2", "chain_srr2": "mm_csrr2", "chain_delay": "mm_cdelay",
          "chain_delay2": "mm_cdelay2", "chain_ftime5": "mm_cftime5"}


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
    h = harm(x, f0_of(a["note"]), 5, 0.3, 16384)
    return [db(h[0]), max(-60.0, db(h[2]) - db(h[0])), max(-60.0, db(h[4]) - db(h[0]))]


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


METRICS = {"rms": m_rms, "h1": m_h1, "h1_rel": m_h1_rel, "harm": m_harm, "thd": m_thd, "peak_hz": m_peak_hz,
           "res_db": m_res_db, "resp_at": m_resp_at, "srr_image": m_srr_image, "level_at": m_level_at, "echo_ms": m_echo_ms,
           "echoes": m_echoes, "echo_bands": m_echo_bands, "fenv_ms": m_fenv_ms}


# ------------------------------------------------------------------------------------------------------ cases
# (group, spec, render, metric, args, tolerance per value or one for all, "rel" for a fraction, info: not asserted)

def cases():
    C = []
    def add(group, spec, name, metric, args, tol, rel=False, info=False, xfail=None):
        C.append(dict(group=group, spec=spec, name=name, metric=metric, args=args, tol=tol, rel=rel, info=info, xfail=xfail))
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
        add("res_level", "chain_filt2", f"qn_lp_{q:03d}", "res_db", {"ref": "qn_open"}, 2.5)
    for q in range(32, 121, 8):
        add("res_level", "chain_filt2", f"qn_hp_{q:03d}", "res_db", {"ref": "qn_open"}, 2.5)
    for d in (32, 48, 64, 72, 80, 96, 127):              # resonance and drive: the clip holds the LP's resonance
        add("clip", "chain_clip3", f"c3_res_{d:03d}", "harm", {"note": 48}, [0.7, 1.5, 2.0])
    for d in (80, 96, 126):
        add("clip", "chain_clip3", f"c3_lpf_{d:03d}", "harm", {"note": 48}, [0.7, 1.5, 2.0])
    for d in (80, 96, 124):
        add("clip", "chain_clip3", f"c3_lp24_{d:03d}", "harm", {"note": 48}, [0.7, 1.5, 2.0])
    for d in (80, 96, 127):
        add("clip", "chain_dist2", f"dclip_lp_{d:03d}", "harm", {"note": 36}, [0.7, 1.5, 2.0])
        add("clip", "chain_dist2", f"dclip_hp_{d:03d}", "harm", {"note": 36}, [0.7, 1.5, 2.0])
    for d in (16, 32, 40, 48, 64, 72, 80, 96, 127):   # the HP's resonance under drive: the known gap
        add("hp_drive", "chain_clip3", f"c3_hres_{d:03d}", "harm", {"note": 48}, [1.0, 3.0, 3.0],
            xfail="HP at HPQ 127, a sine at the cutoff: 2-7 dB louder than the reference (Phase 1)")
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
    add("delay_fb", "chain_delay2", "d2_fb_068", "echoes", {"at": [132, 257]}, 0.5,
        xfail="above unity the reference's first echo grows with DFB (+0.5 dB at 68); MONO2K's does not")
    for s in (0, 32):
        add("delay_pan", "chain_delay2", f"d2_pan_s{s:03d}_f048", "echoes", {"at": [258, 515]}, 1.0)
    for s in (96, 127):                                 # ping-pong: the sum and difference lines leak at -32 dB
        add("delay_pan", "chain_delay2", f"d2_pan_s{s:03d}_f048", "echoes", {"at": [258, 515]}, 1.0,
            xfail="ping-pong leaks ~-32 dB to the wrong side (the mu-law sum / difference lines)")
    for n in ("d2_bas_032", "d2_bas_064", "d2_bas_096", "d2_wid_032", "d2_wid_064", "d2_wid_096", "d2_bw_032_032", "d2_default"):
        add("delay_filter", "chain_delay2", n, "echo_bands", {"at": 0.379, "hz": [125, 250, 500, 1000, 2000, 4000]}, 3.0,
            xfail="375 ms ping-pong runs past MONO2K's half line: the slowed clock darkens 2-4 kHz" if n in ("d2_bas_064", "d2_bas_096") else None)
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


def check(groups):
    ref = json.load(open(REF_JSON))["cases"]
    cs = [c for c in cases() if not groups or c["group"] in groups]
    d = os.path.join(ROOT, "build", "ref", "regress_m2k")
    render_m2k(cs, d)
    rows, fails = [], 0
    for c in cs:
        r = ref.get(key(c))
        if r is None:
            rows.append((c, None, None, "BLOCKED"))
            continue
        m = measure(c, d)
        tol = c["tol"] if isinstance(c["tol"], list) else [c["tol"]] * len(r)
        ok = True
        for a, b, t in zip(r, m, tol):
            if np.isnan(a) or np.isnan(b):
                ok = ok and np.isnan(a) and np.isnan(b)
                continue
            rel = c["rel"] or isinstance(t, str)
            t = float(t.rstrip("%")) / 100 if isinstance(t, str) else t
            err = abs(b - a) / abs(a) if rel else abs(b - a)
            ok = ok and err <= t
        if c["info"]:
            st = "INFO"
        elif c["xfail"]:
            st = "XPASS" if ok else "XFAIL"
        else:
            st = "PASS" if ok else "FAIL"
        fails += st == "FAIL"
        rows.append((c, r, m, st))
    res = []
    last = None
    for c, r, m, st in rows:
        if c["group"] != last:
            print(f"== {c['group']}")
            last = c["group"]
        if r is None:
            print(f"  {st:5s} {c['name']:18s} no reference measurement")
        else:
            print(f"  {st:5s} {c['name']:18s} {c['metric']:10s} MM " + " ".join(f"{v:8.2f}" for v in r) +
                  "   M2K " + " ".join(f"{v:8.2f}" for v in m))
        res.append(dict(group=c["group"], name=c["name"], metric=c["metric"], args=c["args"], tol=c["tol"],
                        rel=c["rel"], mm=r, m2k=m, status=st, xfail=c["xfail"]))
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
            print(f"XPASS {x['group']}:{x['name']}: the known gap no longer shows ({x['xfail']})")
    print("summary:", summary)
    return 1 if fails else 0


if __name__ == "__main__":
    if sys.argv[1:2] == ["capture"]:
        capture()
    elif sys.argv[1:2] == ["check"]:
        sys.exit(check(set(sys.argv[2:])))
    else:
        print(__doc__ if __doc__ else "regress.py capture | check [GROUP ...]")
