#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Mutation tests of the pattern format's conversions (fm_v9, sid_v10): each mutant is one small change to a conversion;
# the self-test must fail on every one. A survivor is a conversion the self-test does not hold.
#   .venv/bin/python tools/ref/mutate_formats.py [TREE]     (default: this checkout; it restores each file it touches)
import subprocess, sys, os
W = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
F = "firmware/src/m2k_factory.c"
P = "firmware/src/m2k_proj.c"
M = [
 ("FM  STAT ratio one step off", F, "    return (best * 128 + 23) / 24;", "    return ((best < 23 ? best + 1 : best) * 128 + 23) / 24;"),
 ("FM  STAT 1FIN ignored", F, "        *fin = CLAMP(f, 0, 127);", "        *fin = 64; (void)f;"),
 ("FM  STAT DEP1 on DYN's law (equivalent: the two laws are 0.4 % apart, every value lands alike)", F, "        return near_u32(T_FMB, 0, 64, T_FMK[v]);", "        return near_u32(T_FMDV, 0, 64, T_FMK[v]);"),
 ("FM  STAT 2VOL capped at 64", F, "    case 4: return dyn ? near_u32(T_FMDV, 0, 64, T_FMK[v]) : near_u32(T_FMB, 0, 127, T_FMK[v]);",
                                   "    case 4: return dyn ? near_u32(T_FMDV, 0, 64, T_FMK[v]) : near_u32(T_FMB, 0, 64, T_FMK[v]);"),
 ("FM  feedback not halved", F, "    case 6: return near_u32(T_FMFB, 0, 127, T_FMK[v] >> 1);", "    case 6: return near_u32(T_FMFB, 0, 127, T_FMK[v]);"),
 ("FM  DYN 1FRQ rounding", F, "(T_RATIO[(v * 32) >> 7] + 2) >> 2)", "(T_RATIO[(v * 32) >> 7] + 6) >> 2)"),
 ("FM  DYN 2FRQ not squared", F, "isqrt(T_RATIO[(v * 32) >> 7] * 16))", "isqrt(T_RATIO[(v * 32) >> 7] * 8))"),
 ("FM  DYN decay not to 1VEN", F, "            k[3] = (uint8_t)(o[2] < 127 ? 64 + near_u32(T_FMDEC, 1, 63, T_ENV_COEF[o[2]]) : 64);", "            k[3] = 64;"),
 ("FM  DYN SWEP sign", F, "        return sw >= 0 ? 64 + CLAMP(isqrt(12 * sw), 0, 63) : 64 - CLAMP(isqrt(48 * -sw), 0, 64);",
                         "        return sw >= 0 ? 64 - CLAMP(isqrt(12 * sw), 0, 63) : 64 + CLAMP(isqrt(48 * -sw), 0, 63);"),
 ("FM  DYN 1VOL ignores 1VEN's push", F, "            return CLAMP(near_u32(T_FMDV, 0, 256, T_FMK[v]) - e, 0, 127);", "            return CLAMP(near_u32(T_FMDV, 0, 256, T_FMK[v]), 0, 127); (void)e;"),
 ("FM  lock DEP1 to the wrong place", F, "static const int8_t FM_TO[2][8] = {{0, 2, -1, 4, 5, -1, 3, -1}, {0, 2, -1, 4, 5, -1, 6, 1}};",
                                       "static const int8_t FM_TO[2][8] = {{0, 3, -1, 4, 5, -1, 2, -1}, {0, 2, -1, 4, 5, -1, 6, 1}};"),
 ("FM  DEC1 lock kept", F, "static const int8_t FM_TO[2][8] = {{0, 2, -1, 4, 5, -1, 3, -1}, {0, 2, -1, 4, 5, -1, 6, 1}};",
                         "static const int8_t FM_TO[2][8] = {{0, 2, 2, 4, 5, -1, 3, -1}, {0, 2, -1, 4, 5, -1, 6, 1}};"),
 ("FM  DYN DEP2 lock moved wrong", F, "static const int8_t FM_TO[2][8] = {{0, 2, -1, 4, 5, -1, 3, -1}, {0, 2, -1, 4, 5, -1, 6, 1}};",
                                    "static const int8_t FM_TO[2][8] = {{0, 2, -1, 4, 5, -1, 3, -1}, {0, 2, -1, 4, 6, -1, 5, 1}};"),
 ("FM  lock values not converted", F, "            l->val = (uint8_t)fm_val(dyn, prm, l->val, o);", "            (void)fm_val;"),
 ("FM  LFO destination off by one", F, "            b[L_DEST] = (uint8_t)(n < 0 ? 0 : P_SYN + n + 1);\n        }\n}\n\n/* SID", "            b[L_DEST] = (uint8_t)(n < 0 ? 0 : P_SYN + n);\n        }\n}\n\n/* SID"),
 ("FM  LFO not followed", F, "            int n = FM_TO[m == M_FMD][dest - P_SYN];\n            b[L_DEST] = (uint8_t)(n < 0 ? 0 : P_SYN + n + 1);",
                           "            int n = FM_TO[m == M_FMD][dest - P_SYN];\n            (void)n;"),
 ("FM  not converted at all", P, "    if (ver < 10) fm_v9(p);", "    if (ver < 1) fm_v9(p);"),
 ("FM  converted twice (version 10 too)", P, "    if (ver < 10) fm_v9(p);", "    if (ver < 11) fm_v9(p);"),
 ("SID wave map: saw and pulse swapped", F, "static const uint8_t W[8] = {0, 40, 64, 115, 90, 90, 90, 90}", "static const uint8_t W[8] = {0, 64, 40, 115, 90, 90, 90, 90}"),
 ("SID combination not to MIX", F, "static const uint8_t W[8] = {0, 40, 64, 115, 90, 90, 90, 90}", "static const uint8_t W[8] = {0, 40, 64, 115, 0, 0, 0, 0}"),
 ("SID duty as it was", F, "    case 1: { int m = v < 128 - v ? v : 128 - v; return CLAMP(64 - m, 0, 127); }", "    case 1: return v;"),
 ("SID PWM not halved", F, "    case 2: return v >> 1;\n    case 4:", "    case 2: return v;\n    case 4:"),
 ("SID ring and sync swapped", F, "M[4] = {0, 64, 32, 100}", "M[4] = {0, 32, 64, 100}"),
 ("SID interval slope", F, "    case 4: return CLAMP(32 + ((v - 64) * 24 / 64) * 32 / 12, 0, 127);", "    case 4: return CLAMP(32 + ((v - 64) * 24 / 64) * 16 / 12, 0, 127);"),
 ("SID FINE lock kept", F, "static const int8_t SID_TO[8] = {3, 0, 1, -1, 6, -1, 4, -1};", "static const int8_t SID_TO[8] = {3, 0, 1, -1, 6, 7, 4, -1};"),
 ("SID WAV1 lock to PW", F, "static const int8_t SID_TO[8] = {3, 0, 1, -1, 6, -1, 4, -1};", "static const int8_t SID_TO[8] = {0, 3, 1, -1, 6, -1, 4, -1};"),
 ("SID PWRS not ON", F, "k[2] = 127; k[3] = (uint8_t)sid_val(0, o[0]);", "k[2] = 0; k[3] = (uint8_t)sid_val(0, o[0]);"),
 ("SID TUNE not centred", F, "        k[4] = (uint8_t)sid_val(6, o[6]); k[5] = 0; k[6] = (uint8_t)sid_val(4, o[4]); k[7] = 64;",
                           "        k[4] = (uint8_t)sid_val(6, o[6]); k[5] = 0; k[6] = (uint8_t)sid_val(4, o[4]); k[7] = o[7];"),
 ("SID MSRC not MFRQ", F, "k[5] = 0; k[6] = (uint8_t)sid_val(4, o[4]);", "k[5] = 127; k[6] = (uint8_t)sid_val(4, o[4]);"),
 ("SID LFO not followed", F, "            int n = SID_TO[dest - P_SYN];\n            b[L_DEST] = (uint8_t)(n < 0 ? 0 : P_SYN + n + 1);", "            int n = SID_TO[dest - P_SYN];\n            (void)n;"),
 ("SID not converted", P, "    if (ver < 11) sid_v10(p);", "    if (ver < 1) sid_v10(p);"),
 ("SID converted on a version-11 pattern", P, "    if (ver < 11) sid_v10(p);", "    if (ver < 12) sid_v10(p);"),
]
def run():
    r = subprocess.run("cc -O2 -w -Ifirmware/src -Ifirmware/gen -o build/host/m2k_mut host/m2k_host.c -lm && build/host/m2k_mut --selftest",
                       shell=True, cwd=W, capture_output=True, text=True)
    out = r.stdout + r.stderr
    fails = [l[6:] for l in out.splitlines() if l.startswith("FAIL:")]
    return "selftest passed" in out, fails
ok0, _ = run()
print("unmutated:", "passes" if ok0 else "FAILS")
survived = 0
for name, f, old, new in M:
    p = os.path.join(W, f)
    src = open(p).read()
    if old not in src:
        print(f"  ?? {name}: mutation site not found"); continue
    open(p, "w").write(src.replace(old, new, 1))
    try:
        passed, fails = run()
    finally:
        open(p, "w").write(src)
    if passed and "(equivalent" not in name: survived += 1
    print(f"  {'SURVIVED' if passed else 'killed  '} {name}" + ("" if passed else f"  ({fails[0][:90]}{' +%d more' % (len(fails) - 1) if len(fails) > 1 else ''})"))
print(f"{len(M) - survived} of {len(M)} killed or equivalent")
sys.exit(1 if survived or not ok0 else 0)
