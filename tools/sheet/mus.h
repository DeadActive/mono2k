/* SPDX-License-Identifier: GPL-3.0-only */
/* Sheet 3's draw: instruments, not effects. Still random, but inside musical ranges: harmonic FM ratios (a bell's
 * now and then), the filter's highpass under the note and its lowpass a few octaves over it, little crush, slow
 * motion only (filter drift, depth, width), and an envelope by the instrument: 0 pad, 1 stab, 2 keys, 3 lead. */
enum { C_PAD, C_STAB, C_KEYS, C_LEAD };

static int pick_w(const int *v, const int *w, int n)
{
    int tw = 0; for (int i = 0; i < n; i++) tw += w[i];
    int r = ri(0, tw - 1);
    for (int i = 0; i < n; i++) { if (r < w[i]) return v[i]; r -= w[i]; }
    return v[0];
}
#define PICK(v, w) pick_w(v, w, (int)(sizeof v / sizeof *v))

/* a ratio on FM+'s 24 steps: harmonic mostly (1/4 1/2 1 2 3 4, 3/2), a bell's (5/4 7/4 5/2 7/2) at times */
static uint8_t ratio24(int bell_pct)
{
    static const int H[] = {7, 11, 15, 19, 21, 23, 17}, HW[] = {1, 4, 6, 4, 3, 2, 1};
    static const int B[] = {16, 18, 20, 22}, BW[] = {1, 1, 1, 1};
    int i = chance(bell_pct) ? PICK(B, BW) : PICK(H, HW);
    return (uint8_t)((i * 128 + 64) / 24);
}

static void draw_mus(int id, int cat, cand_t *c)
{
    rs = (uint32_t)id * 2246822519u + (uint32_t)cat * 3266489917u + 99u; for (int i = 0; i < 8; i++) rnd();
    m_cpy(c->glob, GLOB0, G_COUNT);
    static const int MP[] = {M_ENS, M_SAW, M_PUL, M_FMS, M_FMP, M_WAV, M_VOX}, WP[] = {5, 2, 2, 2, 2, 3, 1};
    static const int MS[] = {M_ENS, M_FMS, M_FMP, M_WAV, M_SAW}, WS[] = {7, 1, 1, 1, 1};
    static const int MK[] = {M_FMS, M_FMP, M_FMD, M_WAV, M_GSIN, M_PUL}, WK[] = {3, 3, 2, 2, 1, 1};
    static const int ML[] = {M_FMS, M_FMD, M_FMP, M_SAW, M_PUL, M_WAV, M_GSIN, M_SID, M_VOX}, WL[] = {2, 2, 1, 1, 2, 2, 1, 1, 1};
    int m = cat == C_PAD ? PICK(MP, WP) : cat == C_STAB ? PICK(MS, WS) : cat == C_KEYS ? PICK(MK, WK) : PICK(ML, WL);
    kit_default(&scratch, 0, m);
    uint8_t *k = scratch.kit[0];
    int pluck = cat == C_STAB || cat == C_KEYS;
    /* the machine, in its musical ranges */
    switch (m) {
    case M_FMS:
        k[SA] = ratio24(cat == C_KEYS ? 25 : 10); k[SB] = (uint8_t)(64 + ri(-4, 4));
        k[SC] = (uint8_t)(pluck ? ri(70, 110) : ri(20, 60)); k[SD] = (uint8_t)ri(0, 50);
        k[SE] = ratio24(cat == C_KEYS ? 25 : 10); k[SF] = (uint8_t)ri(0, pluck ? 60 : 45); k[SG] = (uint8_t)ri(30, 90); break;
    case M_FMP:
        for (int j = 0; j < 3; j++) {
            k[P_SYN + 2 * j] = ratio24(cat == C_KEYS ? 25 : 10);
            k[P_SYN + 2 * j + 1] = (uint8_t)(chance(30) ? 0 : pluck ? ri(70, 110) : ri(5, 55));
        }
        k[SG] = (uint8_t)ri(30, 90); break;
    case M_FMD: {
        static const int R1[] = {32, 64, 96, 127}, R1W[] = {2, 5, 1, 2}, R2[] = {45, 64, 91, 111, 127}, R2W[] = {2, 4, 3, 2, 1};
        k[SA] = (uint8_t)PICK(R1, R1W); k[SB] = (uint8_t)(chance(70) ? 64 : 64 + ri(-10, 10));
        k[SC] = (uint8_t)ri(20, 70); k[SD] = (uint8_t)(pluck && chance(60) ? ri(70, 100) : 64);
        k[SE] = (uint8_t)PICK(R2, R2W); k[SF] = (uint8_t)(pluck ? ri(70, 110) : ri(0, 50)); k[SG] = (uint8_t)ri(0, 40); break;
    }
    case M_SAW:
        k[SA] = (uint8_t)ri(40, 110); k[SB] = (uint8_t)ri(10, 60); k[SC] = (uint8_t)ri(0, 80); k[SD] = (uint8_t)ri(0, 40);
        k[SE] = (uint8_t)ri(0, 40); k[SF] = (uint8_t)ri(0, 60); k[SG] = (uint8_t)ri(0, 30); break;
    case M_PUL:
        k[SA] = (uint8_t)ri(0, 100); k[SB] = (uint8_t)ri(5, 50); k[SC] = (uint8_t)ri(0, 60); k[SD] = (uint8_t)ri(0, 30);
        k[SE] = (uint8_t)ri(30, 64); k[SF] = (uint8_t)ri(0, 20); k[SG] = (uint8_t)(chance(50) ? 127 : 0); break;
    case M_ENS:
        k[SD] = (uint8_t)ri(0, 127); k[SE] = (uint8_t)ri(20, 64); k[SF] = (uint8_t)ri(0, 110); k[SG] = (uint8_t)ri(0, 127); break;
    case M_WAV:
        k[SA] = (uint8_t)ri(0, 127); k[P_WBNK] = chance(50) ? 127 : 0; k[SB] = (uint8_t)(64 + ri(-20, 20));
        k[SC] = (uint8_t)ri(0, 127); k[SD] = (uint8_t)ri(0, 20); k[SE] = (uint8_t)ri(0, 10);
        k[SF] = (uint8_t)(64 + ri(-6, 8)); k[SG] = (uint8_t)ri(0, 80); k[SH] = 0; break;
    case M_GSIN:
        k[SA] = (uint8_t)ri(0, 40); k[SB] = (uint8_t)ri(0, 40); k[SC] = (uint8_t)ri(0, 10);
        k[SD] = (uint8_t)(cat == C_KEYS ? ri(0, 10) : 0); k[SE] = (uint8_t)ri(0, 127);
        k[SF] = (uint8_t)(cat == C_KEYS ? ri(0, 20) : 0); k[SG] = (uint8_t)ri(0, 70); break;
    case M_SID:
        k[SA] = (uint8_t)ri(30, 90); k[SB] = (uint8_t)ri(0, 20); k[SC] = (uint8_t)(chance(50) ? 127 : 0);
        k[SD] = enc(ri(0, 2), 5); k[SE] = chance(80) ? 0 : enc(ri(1, 2), 4); break;
    case M_VOX:
        k[SA] = (uint8_t)ri(0, 127); k[SB] = (uint8_t)ri(0, 127); k[SC] = (uint8_t)(chance(50) ? 127 : 0);
        k[SD] = (uint8_t)ri(0, 60); k[SE] = 0; k[SG] = 0; break;
    }
    /* the envelope */
    if (cat == C_PAD) { k[P_ATK] = (uint8_t)ri(30, 85); k[P_HOLD] = 127; k[P_DEC] = 127; k[P_REL] = (uint8_t)ri(85, 120); k[P_GATE] = 127; }
    else if (cat == C_STAB) { k[P_ATK] = (uint8_t)ri(0, 3); k[P_HOLD] = (uint8_t)ri(0, 12); k[P_DEC] = (uint8_t)ri(25, 70);
                              k[P_REL] = (uint8_t)ri(15, 60); k[P_GATE] = (uint8_t)ri(4, 20); }
    else if (cat == C_KEYS) { k[P_ATK] = 0; k[P_HOLD] = (uint8_t)ri(0, 16); k[P_DEC] = (uint8_t)ri(45, 95);
                              k[P_REL] = (uint8_t)ri(30, 80); k[P_GATE] = (uint8_t)ri(6, 30); }
    else { k[P_ATK] = (uint8_t)ri(0, 25); k[P_HOLD] = 127; k[P_DEC] = 127; k[P_REL] = (uint8_t)ri(30, 80);
           k[P_GATE] = (uint8_t)ri(60, 127); k[P_PORT] = (uint8_t)ri(0, 40); }
    /* the filter: HP under the note, LP 4 .. 30 x it */
    k[P_BASE] = (uint8_t)ri(0, 14);
    int top = cat == C_PAD ? ri(40, 76) : ri(44, 84);
    k[P_WDTH] = (uint8_t)(top - k[P_BASE]);
    k[P_HPQ] = (uint8_t)ri(0, 20); k[P_LPQ] = (uint8_t)ri(0, cat == C_STAB ? 90 : 70);
    k[P_FATK] = (uint8_t)ri(0, 20);
    k[P_FDEC] = (uint8_t)(cat == C_PAD ? ri(60, 127) : cat == C_STAB ? ri(20, 70) : cat == C_KEYS ? ri(30, 90) : ri(40, 127));
    k[P_BOF] = 64;
    k[P_WOF] = (uint8_t)(64 + (cat == C_PAD ? ri(-10, 25) : cat == C_STAB ? ri(5, 40) : cat == C_KEYS ? ri(0, 35) : ri(-5, 25)));
    /* a little dirt */
    k[P_DIST] = (uint8_t)(chance(60) ? 64 : ri(66, 90));
    if (chance(15)) k[P_BITS] = (uint8_t)ri(5, 25);
    if (chance(10)) k[P_SRR] = (uint8_t)ri(5, 20);
    if (chance(40)) { k[P_EQF] = (uint8_t)ri(0, 127); k[P_EQG] = (uint8_t)(64 + ri(-12, 12)); }
    /* the place */
    static const int DT[] = {48, 32, 96, 24, 64}, DTW[] = {3, 2, 2, 1, 2};
    int dly = cat == C_STAB ? 70 : cat == C_PAD ? 30 : 50;
    if (chance(dly)) { k[P_DSND] = (uint8_t)(cat == C_STAB ? ri(90, 112) : ri(80, 102)); k[P_DTIM] = (uint8_t)PICK(DT, DTW);
                       k[P_DFB] = (uint8_t)ri(20, cat == C_STAB ? 80 : 60); k[P_DWID] = (uint8_t)ri(60, 127); }
    k[P_SPU] = (uint8_t)(cat == C_PAD ? ri(50, 110) : cat == C_STAB ? ri(40, 100) : cat == C_KEYS ? ri(20, 80) : ri(15, 70));
    int mod = cat == C_PAD ? 60 : cat == C_KEYS ? 40 : 30;
    if (chance(mod)) { k[P_MODT] = enc(chance(70) ? 0 : 2, 3); k[P_MODR] = (uint8_t)ri(10, 60); k[P_MODD] = (uint8_t)ri(40, 100);
                       k[P_MODM] = (uint8_t)ri(30, 90); }
    k[P_LEVL] = 80;
    c->glob[G_VERB] = (uint8_t)ri(50, 110); c->glob[G_DAMP] = (uint8_t)ri(30, 90);
    if (chance(15)) c->glob[G_TAPE] = (uint8_t)ri(10, 50);
    /* slow motion: depth, tone, width, the filter's drift; a lead's vibrato */
    static const int8_t SAFE[M_COUNT][4] = {
        [M_FMS] = {5, 6, -1, -1}, [M_FMP] = {1, 3, 5, 6}, [M_FMD] = {2, 5, 6, -1}, [M_SAW] = {0, 1, 2, -1}, [M_PUL] = {4, 0, -1, -1},
        [M_ENS] = {3, 4, 5, -1}, [M_WAV] = {6, -1, -1, -1}, [M_GSIN] = {0, 6, -1, -1}, [M_SID] = {0, -1, -1, -1}, [M_VOX] = {0, 1, -1, -1},
    };
    int nl = cat == C_PAD ? ri(1, 2) : cat == C_STAB ? ri(0, 1) : cat == C_KEYS ? ri(0, 1) : 0, l = 0;
    for (; l < nl; l++) {
        uint8_t *b = &k[P_LFO + 8 * l];
        int ns = 0; while (ns < 4 && SAFE[m][ns] >= 0) ns++;
        int dest = chance(40) || !ns ? P_WDTH : P_SYN + SAFE[m][ri(0, ns - 1)];
        if (m == M_FMS && dest == SC && k[SC] > 64) dest = P_WDTH;
        b[L_DEST] = (uint8_t)(dest + 1); b[L_WAVE] = enc(chance(50) ? 0 : 1, 7); b[L_MULT] = enc(ri(0, 1), 8);
        b[L_SPD] = (uint8_t)ri(5, 40); b[L_DPTH] = (uint8_t)(64 + (chance(50) ? 1 : -1) * ri(8, 30)); b[L_TRIG] = enc(0, 5);
    }
    if (cat == C_LEAD && tune_on_h(m) && chance(60)) {
        uint8_t *b = &k[P_LFO + 8 * l];
        b[L_DEST] = (uint8_t)(SH + 1); b[L_WAVE] = enc(1, 7); b[L_MULT] = enc(4, 8);
        b[L_SPD] = (uint8_t)ri(42, 56); b[L_DPTH] = (uint8_t)ri(66, 68); b[L_TRIG] = enc(1, 5);
    }
    c->m = m; c->held = cat == C_PAD || cat == C_LEAD; c->note = 48; c->nmov = 0;
    m_cpy(c->kit, k, P_COUNT);
}
