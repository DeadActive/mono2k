/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's factory patterns: sixteen sound designs in bank A to show what six mono machines can do (the blank kit
 * fills the other banks).
 * Each pattern is a whole song part: its six machines and every parameter, its bus, its tempo, its steps.
 * Steps are written as a grid, one character a step ('|' and ' ' only separate):
 *   .  rest          x  trig        X  accent        s  slide         S  accent + slide
 *   Q  75 %          ?  50 %        q  25 %          e  12 %
 *   2 3 4  ratchet   h  1:2         H  2:2           k  4:4           f  FILL     n  not FILL    p  PRE   o  1ST
 * Notes are given in order and cycle over the trigs. */

static void beat(m2k_pattern_t *p, int t, const char *g, const uint8_t *notes, int nn)
{
    int s = 0, k = 0;
    for (const char *c = g; *c && s < M2K_STEPS; c++) {
        if (*c == '|' || *c == ' ') continue;
        int on = 1, fl = 0, cond = C_ALWAYS, rt = 0;
        switch (*c) {
        case '.': on = 0; break;
        case 'x': break;
        case 'X': fl = ST_ACC; break;
        case 's': fl = ST_SLD; break;
        case 'S': fl = ST_ACC | ST_SLD; break;
        case 'Q': cond = C_P75; break;
        case '?': cond = C_P50; break;
        case 'q': cond = C_P25; break;
        case 'e': cond = C_P12; break;
        case '2': rt = 2; break;
        case '3': rt = 3; break;
        case '4': rt = 4; break;
        case 'h': cond = C_1_2; break;
        case 'H': cond = C_2_2; break;
        case 'k': cond = C_4_4; break;
        case 'f': cond = C_FILL; break;
        case 'n': cond = C_NFILL; break;
        case 'p': cond = C_PRE; break;
        case 'o': cond = C_1ST; break;
        default: on = 0; break;
        }
        if (on) {
            m2k_step_t *st = &p->step[t][s];
            st->flags = (uint8_t)(ST_TRIG | fl);
            st->note = notes[k % nn];
            st->cond = (uint8_t)cond;
            st->rtrg = (uint8_t)rt;
            k++;
        }
        s++;
    }
}
#define NOTES(...) (const uint8_t[]){__VA_ARGS__}, (int)sizeof((const uint8_t[]){__VA_ARGS__})
#define BEAT(p, t, g, ...) beat(p, t, g, NOTES(__VA_ARGS__))

/* a pattern from before version 2 timed ATK HOLD DEC REL on MONO2K's first curve: each value moves to the one whose
 * time on the Monomachine's curve is nearest, in the kit and in the locks */
static uint8_t amp_v1_val(int prm, int v) { return prm >= P_ATK && prm <= P_REL ? T_AMPV1[prm - P_ATK][v & 127] : (uint8_t)v; }
static void amp_v1(m2k_pattern_t *p)
{
    for (int t = 0; t < M2K_TRACKS; t++)
        for (int k = P_ATK; k <= P_REL; k++) p->kit[t][k] = amp_v1_val(k, p->kit[t][k]);
    for (int i = 0; i < M2K_LOCKS; i++)
        if (p->lock[i].trk != 0xFF) p->lock[i].val = amp_v1_val(p->lock[i].param, p->lock[i].val);
}

/* patterns before version 5 ran MONO2K's own chain; from 5 it is the Monomachine's (measured): every chain setting
 * goes to the one that gives the same Hz, dB or time (tools/gen_tables.py, T_V5_*). The filter now follows the
 * note, so its old cutoffs are placed against the note the track plays most */
static int v5_base(int pos, int note) { return pos <= 0 ? 0 : CLAMP(16 + (pos - 8 * note + (pos >= 8 * note ? 6 : -6)) / 12, 0, 127); }

static uint8_t v5_val(int prm, int v, const uint8_t *k, int note)
{
    switch (prm) {
    case P_DIST: return T_V5_DIST[v];
    case P_VOL: return T_V5_VOL[v];
    case P_BASE: return (uint8_t)(v ? v5_base(T_V5_FPOS[v], note) : 0);
    case P_WDTH: {
        int b = k[P_BASE], lp = b + v;
        if (lp >= 127 && k[P_LPQ] == 0) return 127;
        int nb = b ? v5_base(T_V5_FPOS[b], note) : 0;
        return (uint8_t)CLAMP(v5_base(T_V5_FPOS[CLAMP(lp, 0, 127)], note) - nb, 0, 127);
    }
    case P_HPQ: return T_V5_HPQ[v];
    case P_LPQ: return T_V5_LPQ[v];
    case P_FATK: return T_V5_FATK[v];
    case P_FDEC: return T_V5_FDEC[v];
    case P_BOF: case P_WOF: return T_V5_OFS[v];
    case P_EQF: return T_V5_EQF[v];
    case P_EQG: return T_V5_EQG[v];
    case P_SRR: return T_V5_SRR[v];
    case P_DTIM: return (uint8_t)(v * 4 > 127 ? 127 : v * 4);
    case P_DSND: return (uint8_t)(64 - (v + 1) / 2);
    case P_DFB: return (uint8_t)((v * 270 * 64 + 16384) >> 15);
    case P_DBAS: return T_V5_DHP[v];
    case P_DWID: {
        int nb = T_V5_DHP[k[P_DBAS]];
        return (uint8_t)CLAMP(T_V5_DLP[CLAMP(k[P_DBAS] + v, 0, 127)] - nb, 0, 127);
    }
    default: return (uint8_t)v;
    }
}

static void chain_v4(m2k_pattern_t *p)
{
    for (int t = 0; t < M2K_TRACKS; t++) {
        int count[128] = {0}, note = 60, best = 0;
        for (int s = 0; s < M2K_STEPS; s++)
            if ((p->step[t][s].flags & ST_TRIG) && ++count[p->step[t][s].note & 127] > best) { best = count[p->step[t][s].note & 127]; note = p->step[t][s].note & 127; }
        uint8_t old[P_COUNT], *k = p->kit[t];
        m_cpy(old, k, P_COUNT);
        for (int i = 0; i < M2K_LOCKS; i++) {          /* the locks against the old kit, before it changes */
            m2k_lock_t *l = &p->lock[i];
            if (l->trk == t) l->val = v5_val(l->param, l->val, old, note);
        }
        for (int prm = P_ATK; prm < P_BITS; prm++) k[prm] = v5_val(prm, old[prm], old, note);
    }
}

/* SWAV-ENS took the Monomachine's page (version 7): an old interval (0..24 semitones over the root) folds into the
 * table's -11..+12 by octaves, and one that would fold to 0 is +12 (the voice stays and keeps its note); always the
 * equal-tempered value in the middle of its group, so never OFF (61-63) nor the octave down (0-1). DTUN's fixed spread
 * is not the chorus's moving copy: CHRL 0. SHPE, PW, LEV and SUB have no counterpart: one saw at the note, the voices
 * at full level. LEV's silence is kept step by step: a trig where the old level was 0 (the kit's, or a lock's) gets
 * PCH2..4 locked OFF, the chord keeping its intervals where a lock raised it (a pattern without room for those locks
 * keeps the voices). Interval locks convert, the rest go. An LFO aimed at an ENS track's page is cleared, its
 * intervals too: the new axis has OFF at 61-63 and the octave down at 0-1, so a sweep would drop voices or jump an
 * octave (no factory pattern had one); so is a step's lock of an LFO's destination onto it */
static uint8_t ens_fold(int v)
{
    int i = (v * 25) >> 7;
    while (i > 12) i -= 12;
    if (i == 0) i = 12;
    return T_ENS_ET[i + 10];
}

static int ens_lock(m2k_pattern_t *p, int t, int st, int prm, int v)
{
    int free = -1;
    for (int i = 0; i < M2K_LOCKS; i++) {
        if (p->lock[i].trk == t && p->lock[i].step == st && p->lock[i].param == prm) { p->lock[i].val = (uint8_t)v; return 1; }
        if (free < 0 && p->lock[i].trk == 0xFF) free = i;
    }
    if (free < 0) return 0;
    p->lock[free] = (m2k_lock_t){(uint8_t)t, (uint8_t)st, (uint8_t)prm, (uint8_t)v};
    return 1;
}

static int ens_target(int trck) { return (trck * M2K_TRACKS) >> 7; }        /* the track an LFO reaches */

static void ens_v6(m2k_pattern_t *p)
{
    for (int t = 0; t < M2K_TRACKS; t++) {
        if (p->machine[t] != M_ENS) continue;
        uint8_t *k = p->kit[t];
        /* which trigs the old level silenced, before its locks go */
        uint8_t mute[M2K_STEPS];
        int any_up = k[P_SYN + 6] > 0;
        for (int st = 0; st < M2K_STEPS; st++) mute[st] = k[P_SYN + 6] == 0;
        for (int i = 0; i < M2K_LOCKS; i++)
            if (p->lock[i].trk == t && p->lock[i].param == P_SYN + 6) {
                mute[p->lock[i].step] = p->lock[i].val == 0;
                any_up |= p->lock[i].val > 0;
            }
        for (int i = 0; i < M2K_LOCKS; i++) {
            m2k_lock_t *l = &p->lock[i];
            if (l->trk != t || l->param >= 8) continue;
            if (l->param < 3) l->val = ens_fold(l->val);
            else l->trk = 0xFF;
        }
        for (int i = 0; i < 3; i++) k[P_SYN + i] = any_up ? ens_fold(k[P_SYN + i]) : 62;
        k[P_SYN + 3] = 0; k[P_SYN + 4] = 64; k[P_SYN + 5] = 0; k[P_SYN + 6] = 64; k[P_SYN + 7] = 64;
        if (any_up)
            for (int st = 0; st < M2K_STEPS; st++)
                if (mute[st] && (p->step[t][st].flags & ST_TRIG))
                    for (int i = 0; i < 3; i++) ens_lock(p, t, st, P_SYN + i, 62);
    }
    for (int t = 0; t < M2K_TRACKS; t++)
        for (int l = 0; l < 3; l++) {
            uint8_t *b = &p->kit[t][P_LFO + 8 * l];
            int to = ens_target(b[L_TRCK]), dest = b[L_DEST] - 1;
            if (p->machine[to] == M_ENS && dest >= P_SYN && dest < P_SYN + 8) b[L_DEST] = 0;
            for (int i = 0; i < M2K_LOCKS; i++) {          /* a step's lock of this LFO's destination */
                m2k_lock_t *lk2 = &p->lock[i];
                if (lk2->trk != t || lk2->param != P_LFO + 8 * l + L_DEST) continue;
                int to2 = to, d2 = lk2->val - 1;
                for (int j = 0; j < M2K_LOCKS; j++)
                    if (p->lock[j].trk == t && p->lock[j].step == lk2->step && p->lock[j].param == P_LFO + 8 * l + L_TRCK)
                        to2 = ens_target(p->lock[j].val);
                if (p->machine[to2] == M_ENS && d2 >= P_SYN && d2 < P_SYN + 8) lk2->val = 0;
            }
        }
}

/* the bus's controls took new laws (version 9, tools/gen_tables.py): each old value goes to the new one that sounds as
 * it. GLCH, DRIV, TAPE and ADPC as they were (TAPE's 1..3, under a cent of wow, to its least); HISS by its crackle, the
 * old crackle as it was, its hiss there as loud or louder (its own law brings it in sooner) */
static void bus_v9(m2k_pattern_t *p)
{
    uint8_t *g = p->glob;
    g[G_GLCH] = T_V9_GLCH[g[G_GLCH] & 127];
    g[G_HISS] = T_V9_HISS[g[G_HISS] & 127];
    g[G_DRIV] = T_V9_DRIV[g[G_DRIV] & 127];
    g[G_TAPE] = T_V9_TAPE[g[G_TAPE] & 127];
    g[G_ADPC] = T_V9_ADPC[g[G_ADPC] & 127];
}

/* FM+STAT and FM+DYN took the Monomachine's pages (version 10). The old ones were MONO2K's own: RAT1 DEP1 DEC1 RAT2
 * DEP2 DEC2 FDBK, then ALGO (STAT) or SWEP (DYN), the second operator fed back and modulating the first in series.
 * Both new machines run their operators side by side, so an old sound is carried over as near as the pages go, not
 * as it was: each ratio to the nearest the new page holds (STAT's 24 steps with 1FIN making up the rest; DYN's
 * straight /64 and squared /64); each depth to the new law's nearest; a decay to the new decay of the nearest time,
 * which starts from 64's depth (STAT's 1ENV, DYN's 2ENV) or, on DYN's first operator, as 1VEN's push down to a 1VOL
 * that keeps the old start; the feedback to the first operator (STAT) or stays on the second (DYN); SWEP to 1FEN.
 * A lock goes with its parameter where it has a place; DEC1, DEC2, ALGO's go, and so does an LFO aimed at them */
static int near_u32(const uint32_t *T, int lo, int hi, uint32_t want)
{
    int best = lo;
    for (int v = lo; v <= hi; v++)
        if ((T[v] > want ? T[v] - want : want - T[v]) < (T[best] > want ? T[best] - want : want - T[best])) best = v;
    return best;
}

static int isqrt(int x)
{
    int r = 0;
    while ((r + 1) * (r + 1) <= x) r++;
    return r;
}

/* a ratio (Q8) to FM+STAT's step, as its knob value, and the 1FIN that makes up the rest */
static int fms_ratio(int q8, int *fin)
{
    int best = 0;
    uint64_t r = (uint64_t)q8 << 8;
    for (int s = 1; s < 24; s++) {    /* nearest on a log scale, by products (no division) */
        uint64_t x = T_FMR24[s], y = T_FMR24[best];
        int closer = x >= r ? (y >= r ? x < y : x * y < r * r) : (y >= r ? r * r < x * y : x > y);
        if (closer) best = s;
    }
    if (fin) {
        int f = (int)(((uint32_t)q8 << 16) / T_FMR24[best]) - 192;   /* (192 + fin) / 256 = r / step */
        *fin = CLAMP(f, 0, 127);
    }
    return (best * 128 + 23) / 24;
}

static int fm_dec(int d) { return 65 + near_u32(T_FMEC, 1, 63, T_ENV_COEF[d]) - 1; }        /* an old decay's 1ENV */

/* where an old page's parameter went: STAT, DYN (-1: nowhere) */
static const int8_t FM_TO[2][8] = {{0, 2, -1, 4, 5, -1, 3, -1}, {0, 2, -1, 4, 5, -1, 6, 1}};

static int fm_val(int dyn, int prm, int v, const uint8_t *old)
{
    switch (prm) {
    case 0: return dyn ? (T_RATIO[(v * 32) >> 7] >= 512 ? 127 : (T_RATIO[(v * 32) >> 7] + 2) >> 2)
                       : fms_ratio(T_RATIO[(v * 32) >> 7], 0);
    case 3: return dyn ? (T_RATIO[(v * 32) >> 7] >= 1024 ? 127 : isqrt(T_RATIO[(v * 32) >> 7] * 16))
                       : fms_ratio(T_RATIO[(v * 32) >> 7], 0);
    case 1:
        if (dyn) {                    /* 1VOL: the old start, less 1VEN's push */
            int d = old[2] < 127 ? near_u32(T_FMDEC, 1, 63, T_ENV_COEF[old[2]]) : 0;
            int e = d < 32 ? 4 * d : 128;
            return CLAMP(near_u32(T_FMDV, 0, 256, T_FMK[v]) - e, 0, 127);
        }
        return near_u32(T_FMB, 0, 64, T_FMK[v]);
    case 4: return dyn ? near_u32(T_FMDV, 0, 64, T_FMK[v]) : near_u32(T_FMB, 0, 127, T_FMK[v]);
    case 6: return near_u32(T_FMFB, 0, 127, T_FMK[v] >> 1);
    case 7: {                         /* SWEP: the old push at C4, as 1FEN's note-independent one */
        int sw = v - 64;
        return sw >= 0 ? 64 + CLAMP(isqrt(12 * sw), 0, 63) : 64 - CLAMP(isqrt(48 * -sw), 0, 64);
    }
    default: return 64;
    }
}

static void fm_v9(m2k_pattern_t *p)
{
    for (int t = 0; t < M2K_TRACKS; t++) {
        int m = p->machine[t], dyn = m == M_FMD;
        if (m != M_FMS && m != M_FMD) continue;
        uint8_t *k = &p->kit[t][P_SYN], o[8];
        m_cpy(o, k, 8);
        for (int i = 0; i < M2K_LOCKS; i++) {
            m2k_lock_t *l = &p->lock[i];
            if (l->trk != t || l->param >= P_SYN + 8) continue;   /* P_SYN is 0: none under it */
            int prm = l->param - P_SYN, to = FM_TO[dyn][prm];
            if (prm == 1 && !dyn && o[2] < 127) to = -1;          /* STAT's 1ENV holds the decay */
            if (prm == 4 && dyn && o[5] < 127) to = -1;           /* and DYN's 2ENV */
            if (to < 0) { l->trk = 0xFF; continue; }
            l->val = (uint8_t)fm_val(dyn, prm, l->val, o);
            l->param = (uint8_t)(P_SYN + to);
        }
        int fin = 64;
        if (dyn) {
            k[0] = (uint8_t)fm_val(1, 0, o[0], o);
            k[1] = (uint8_t)fm_val(1, 7, o[7], o);
            k[2] = (uint8_t)fm_val(1, 1, o[1], o);
            k[3] = (uint8_t)(o[2] < 127 ? 64 + near_u32(T_FMDEC, 1, 63, T_ENV_COEF[o[2]]) : 64);
            k[4] = (uint8_t)fm_val(1, 3, o[3], o);
            k[5] = (uint8_t)(o[5] < 127 ? fm_dec(o[5]) : fm_val(1, 4, o[4], o));
            k[6] = (uint8_t)fm_val(1, 6, o[6], o);
        } else {
            k[0] = (uint8_t)fms_ratio(T_RATIO[(o[0] * 32) >> 7], &fin);
            k[1] = (uint8_t)fin;
            k[2] = (uint8_t)(o[2] < 127 ? fm_dec(o[2]) : fm_val(0, 1, o[1], o));
            k[3] = (uint8_t)fm_val(0, 6, o[6], o);
            k[4] = (uint8_t)fm_val(0, 3, o[3], o);
            k[5] = (uint8_t)fm_val(0, 4, o[4], o);
            k[6] = 64;
        }
        k[7] = 64;
    }
    for (int t = 0; t < M2K_TRACKS; t++)              /* LFOs aimed at an FM page: to the parameter's new place */
        for (int l = 0; l < 3; l++) {
            uint8_t *b = &p->kit[t][P_LFO + 8 * l];
            int to = ens_target(b[L_TRCK]), dest = b[L_DEST] - 1, m = p->machine[to];
            if ((m != M_FMS && m != M_FMD) || dest < P_SYN || dest >= P_SYN + 8) continue;
            int n = FM_TO[m == M_FMD][dest - P_SYN];
            b[L_DEST] = (uint8_t)(n < 0 ? 0 : P_SYN + n + 1);
        }
}

/* SID took the Monomachine's SID-6581 page (version 11). The old one was MONO2K's own, two oscillators: WAV1 PW PWM
 * WAV2 DTN2 FINE MODE MIX. Carried over as near as the new page goes: the first oscillator's wave (a combination to
 * MIX), its duty (PW now a distance from 64), PWM to the sweep, MODE to MOD, the second oscillator's interval to MFRQ
 * (where it stood over C4; MFRQ is the same at every note). WAV2, FINE and MIX have no place; a lock goes with its
 * parameter, an LFO too */
static const int8_t SID_TO[8] = {3, 0, 1, -1, 6, -1, 4, -1};

static int sid_val(int prm, int v)
{
    static const uint8_t W[8] = {0, 40, 64, 115, 90, 90, 90, 90}, M[4] = {0, 64, 32, 100};
    switch (prm) {
    case 0: return W[(v * 8) >> 7];
    case 1: { int m = v < 128 - v ? v : 128 - v; return CLAMP(64 - m, 0, 127); }
    case 2: return v >> 1;
    case 4: return CLAMP(32 + ((v - 64) * 24 / 64) * 32 / 12, 0, 127);
    case 6: return M[(v * 4) >> 7];
    default: return 64;
    }
}

/* format 14: VOX-6 on the Monomachine's VO-6 page (VOC1 VOC2 V-SW VOIC CONS CLEN CVOL TUNE), from MONO2K's own (VOWL
 * VSWP VTIM SHFT BRTH RESO BUZZ VIBR): the vowel nearest the old one (T_VOX14: VOWL's F1 F2 to the nearest VOC1 VOC2),
 * BRTH to VOIC (the noise's share alike), V-SW off, no consonant, CLEN CVOL TUNE in the middle. A VOWL lock becomes
 * VOC1's, a BRTH lock VOIC's, the others go (their knobs are gone); an LFO on VOWL stays on VOC1, one on BRTH moves to
 * VOIC, one on the rest stops */
static void vox_v13(m2k_pattern_t *p)
{
    for (int t = 0; t < M2K_TRACKS; t++) {
        if (p->machine[t] != M_VOX) continue;
        uint8_t *k = &p->kit[t][P_SYN], o[8];
        m_cpy(o, k, 8);
        k[0] = T_VOX14[o[0]][0]; k[1] = T_VOX14[o[0]][1]; k[2] = 0; k[3] = T_VOX14_BR[o[4]];
        k[4] = 0; k[5] = 64; k[6] = 64; k[7] = 64;
        for (int i = 0; i < M2K_LOCKS; i++) {
            m2k_lock_t *l = &p->lock[i];
            if (l->trk != t || l->param < P_SYN || l->param >= P_SYN + 8) continue;
            if (l->param == P_SYN) l->val = T_VOX14[l->val][0];
            else if (l->param == P_SYN + 4) { l->param = P_SYN + 3; l->val = T_VOX14_BR[l->val]; }
            else l->trk = 0xFF;
        }
    }
    for (int t = 0; t < M2K_TRACKS; t++)
        for (int l = 0; l < 3; l++) {
            uint8_t *b = &p->kit[t][P_LFO + 8 * l];
            int to = ens_target(b[L_TRCK]), dest = b[L_DEST] - 1;
            if (p->machine[to] != M_VOX || dest < P_SYN || dest >= P_SYN + 8) continue;
            b[L_DEST] = (uint8_t)(dest == P_SYN ? P_SYN + 1 : dest == P_SYN + 4 ? P_SYN + 3 + 1 : 0);
        }
}

/* format 13: HOLD in beats, as the Monomachine's (v / 32 of a beat, measured at 60, 120 and 240 BPM), no longer 14.6 ms
 * a step: the kit's and its locks' HOLD to the step nearest the time they held, at the pattern's tempo (INF stays) */
static uint8_t hold_to(int v, int bpm) { return v >= 127 ? 127 : (uint8_t)CLAMP((v * bpm * 146 + 9375) / 18750, 0, 126); }

static void hold_v12(m2k_pattern_t *p)
{
    int bpm = 30 + p->glob[G_TMPO];
    for (int t = 0; t < M2K_TRACKS; t++) p->kit[t][P_HOLD] = hold_to(p->kit[t][P_HOLD], bpm);
    for (int i = 0; i < M2K_LOCKS; i++)
        if (p->lock[i].trk != 0xFF && p->lock[i].param == P_HOLD) p->lock[i].val = hold_to(p->lock[i].val, bpm);
}

/* format 12: GND-SIN on the Monomachine's page, TUNE on its eighth knob (CC 55) and MONO2K's own before it. A..H were
 * TUNE FOLD SUB NOIS PDEP PDEC CLIK HARM, are FOLD SUB NOIS PDEP PDEC CLIK HARM TUNE: the kit, its locks and every LFO
 * aimed at one of them (from any track) move, values as they were */
static int gsin_to(int k) { return k == 0 ? 7 : k - 1; }

static void gsin_v11(m2k_pattern_t *p)
{
    for (int t = 0; t < M2K_TRACKS; t++) {
        if (p->machine[t] != M_GSIN) continue;
        uint8_t *k = &p->kit[t][P_SYN], o[8];
        m_cpy(o, k, 8);
        for (int i = 0; i < 8; i++) k[gsin_to(i)] = o[i];
        for (int i = 0; i < M2K_LOCKS; i++) {
            m2k_lock_t *l = &p->lock[i];
            if (l->trk == t && l->param < P_SYN + 8) l->param = (uint8_t)(P_SYN + gsin_to(l->param - P_SYN));
        }
    }
    for (int t = 0; t < M2K_TRACKS; t++)
        for (int l = 0; l < 3; l++) {
            uint8_t *b = &p->kit[t][P_LFO + 8 * l];
            int to = ens_target(b[L_TRCK]), dest = b[L_DEST] - 1;
            if (p->machine[to] != M_GSIN || dest < P_SYN || dest >= P_SYN + 8) continue;
            b[L_DEST] = (uint8_t)(P_SYN + gsin_to(dest - P_SYN) + 1);
        }
}

static void sid_v10(m2k_pattern_t *p)
{
    for (int t = 0; t < M2K_TRACKS; t++) {
        if (p->machine[t] != M_SID) continue;
        uint8_t *k = &p->kit[t][P_SYN], o[8];
        m_cpy(o, k, 8);
        for (int i = 0; i < M2K_LOCKS; i++) {
            m2k_lock_t *l = &p->lock[i];
            if (l->trk != t || l->param >= P_SYN + 8) continue;   /* P_SYN is 0: none under it */
            int prm = l->param - P_SYN;
            if (SID_TO[prm] < 0) { l->trk = 0xFF; continue; }
            l->val = (uint8_t)sid_val(prm, l->val);
            l->param = (uint8_t)(P_SYN + SID_TO[prm]);
        }
        k[0] = (uint8_t)sid_val(1, o[1]); k[1] = (uint8_t)sid_val(2, o[2]); k[2] = 127; k[3] = (uint8_t)sid_val(0, o[0]);
        k[4] = (uint8_t)sid_val(6, o[6]); k[5] = 0; k[6] = (uint8_t)sid_val(4, o[4]); k[7] = 64;
    }
    for (int t = 0; t < M2K_TRACKS; t++)
        for (int l = 0; l < 3; l++) {
            uint8_t *b = &p->kit[t][P_LFO + 8 * l];
            int to = ens_target(b[L_TRCK]), dest = b[L_DEST] - 1;
            if (p->machine[to] != M_SID || dest < P_SYN || dest >= P_SYN + 8) continue;
            int n = SID_TO[dest - P_SYN];
            b[L_DEST] = (uint8_t)(n < 0 ? 0 : P_SYN + n + 1);
        }
}

static void lk(m2k_pattern_t *p, int t, int s, int prm, int v)
{
    for (int i = 0; i < M2K_LOCKS; i++)
        if (p->lock[i].trk == 0xFF) { p->lock[i] = (m2k_lock_t){(uint8_t)t, (uint8_t)s, (uint8_t)prm, (uint8_t)v}; return; }
}

static void set(m2k_pattern_t *p, int t, const uint8_t *pv, int n)   /* (param, value) pairs */
{
    for (int i = 0; i + 1 < n; i += 2) p->kit[t][pv[i]] = pv[i + 1];
}
#define SET(p, t, ...) do { const uint8_t _v[] = {__VA_ARGS__}; set(p, t, _v, sizeof _v); } while (0)

/* an LFO: its destination (a parameter of the same track), wave, multiplier index (x1 .. x128), speed, depth,
 * trig mode (FREE TRIG HOLD ONE HALF) */
static void lfo(m2k_pattern_t *p, int t, int l, int dest, int wave, int mult, int spd, int dpth, int trig)
{
    uint8_t *b = &p->kit[t][P_LFO + 8 * l];
    b[L_DEST] = (uint8_t)(dest + 1);
    b[L_WAVE] = enc(wave, 7);
    b[L_MULT] = enc(mult, 8);
    b[L_SPD] = (uint8_t)spd;
    b[L_DPTH] = (uint8_t)dpth;
    b[L_TRIG] = enc(trig, 5);
}

/* GND-SIN's old page's init (TUNE first, a pitch drop and a click): what the parts and the older formats were written on */
static const uint8_t GSIN_V11[8] = {64, 0, 0, 0, 40, 40, 30, 0};

/* the blank kit of MONO2K's chain before version 5: what an old pattern was written against (the self-test's) */
static void kit_v4(m2k_pattern_t *p, int t, int m)
{
    static const uint8_t D[] = {0, 0, 64, 64, 0, 100, 64, 0, 0, 127, 0, 0, 0, 64, 64, 64, 64, 64, 0, 12, 0, 50, 0, 110};
    kit_default(p, t, m);
    if (m == M_GSIN) m_cpy(&p->kit[t][P_SYN], GSIN_V11, 8);
    m_cpy(&p->kit[t][P_ATK], D, sizeof D);
    p->kit[t][P_LEVL] = 110;
}

#define MACHINES(...) (const uint8_t[]){__VA_ARGS__}

/* every track's LEV scaled: the mix's level under the bus's knee */
static void trim(m2k_pattern_t *p, int lev)
{
    for (int t = 0; t < M2K_TRACKS; t++) p->kit[t][P_LEVL] = (uint8_t)CLAMP(p->kit[t][P_LEVL] * lev / 100, 0, 127);
}

/* the modulation effect: 0 chorus, 1 flanger, 2 phaser; rate (0.1..10 Hz), depth, mix */
#define MOD(p, t, type, rate, dpth, mix) SET(p, t, P_MODT, enc(type, 3), P_MODR, rate, P_MODD, dpth, P_MODM, mix)

/* the machines' A..H, by name */
#define SA (P_SYN + 0)
#define SB (P_SYN + 1)
#define SC (P_SYN + 2)
#define SD (P_SYN + 3)
#define SE (P_SYN + 4)
#define SF (P_SYN + 5)
#define SG (P_SYN + 6)
#define SH (P_SYN + 7)
/* choices */
#define WAVE(i) ((uint8_t)((i) * 127 / 31))   /* DIGI-WAV: 5 BELL 6 GLAS 7 PLST 8 LTEX 19 MTL1 20 MTL2 25 BITS 26 8BIT 30 CRSH */
#define GSIZE(i) enc(i, 5)             /* 1/64 1/32 1/16 1/8 1/4 */
#define FR24(i) enc(i, 24)             /* FM+'s 24 ratio steps, as the sound decodes them (fr24) */
/* the new bank's (docs/bank-a.md): SWAV-ENS on its own page, the owner's waves */
/* an ENS interval, -12 .. +12 semitones but 0, equal tempered (the middle of its group on the Monomachine's table) */
#define PCH(n) ((uint8_t)((n) <= -12 ? 0 : (n) < 0 ? T_ENS_ET[(n) + 11] : T_ENS_ET[(n) + 10]))
#define PCH_OFF 62
#define OS(n) ((uint8_t)(2 * ((n) - 1)))     /* DIGI-WAV on the owner's 64 (P_WBNK 127): OSn exactly */

/* the bank's bus drive, a master limiter's: each part is balanced under the knee without it (m2k_host --demo-dry),
 * then driven about 6 dB into the bus's soft clip, every slot at HAZE's loudness (docs/bank-a.md) */
#define BANK_DRIVE 88

/* an ENS chord on one step, or on every trig from step a to b */
static void chord(m2k_pattern_t *p, int t, int s, int i2, int i3, int i4)
{
    lk(p, t, s, SA, PCH(i2));
    lk(p, t, s, SB, PCH(i3));
    lk(p, t, s, SC, PCH(i4));
}

static void chords(m2k_pattern_t *p, int t, int a, int b, int i2, int i3, int i4)
{
    for (int s = a; s < b; s++)
        if (p->step[t][s].flags & ST_TRIG) chord(p, t, s, i2, i3, i4);
}

/* a DIGI-WAV part on another wave from step a to b (each trig there), at the VOL that gives that wave the part's
 * level: the owner's waves stand 16 dB apart, so a part is balanced wave by wave (tools/demo_levels.py) */
static void wave_span(m2k_pattern_t *p, int t, int a, int b, int wave, int vol)
{
    for (int s = a; s < b; s++)
        if (p->step[t][s].flags & ST_TRIG) { lk(p, t, s, SA, wave); lk(p, t, s, P_VOL, vol); }
}

/* ---------------------------------------------------------------------------------------- bank A: LO-BIT CLUB */

static void pattern_init_blank(m2k_pattern_t *p);   /* (m2k_proj.c) */

/* a part of the bank (docs/bank-a.md), written on today's pages: its machines at their init, the bank's bus drive */
static m2k_pattern_t *part(int i, const char *name, int bpm, const uint8_t *machines)
{
    m2k_pattern_t *p = PAT(i);
    pat_init(p, name);
    for (int t = 0; t < M2K_TRACKS; t++) kit_default(p, t, machines[t]);
    p->glob[G_TMPO] = (uint8_t)(bpm - 30);
    p->glob[G_LEN] = 64;
    p->glob[G_VERB] = 50;
    p->glob[G_DRIV] = BANK_DRIVE;
    return p;
}
#define SPEED(i) enc(i, 7)             /* a track's: 0 1/8x 1 1/4x 2 1/2x 3 3/4x 4 1x 5 3/2x (triplets) 6 2x */
#define DW4(i) ((uint8_t)((i) * 32 + 16))   /* DRM-KICK's and DRM-SNAR's waves: 0 SIN 1 TRI 2 SAW 3 SQR */
#define DW5(i) ((uint8_t)((i) * 26 + 13))   /* DRM-HATS' and DRM-CYMB's: 0 SIN 1 TRI 2 SAW 3 SQR 4 NOIS */
#define SID5(i) enc(i, 5)              /* SID-6581's WAVE: 0 TRI 1 SAW 2 PULS 3 MIX 4 NOIS */
#define SIDM(i) enc(i, 4)              /* its MOD: 0 OFF 1 RING 2 SYNC 3 R+S */

/* each trig of track t a lock of prm, the next value of v in turn (cycling: over a count that is not the trigs', it
 * lands differently each bar) */
static void cyc(m2k_pattern_t *p, int t, int prm, const uint8_t *v, int n)
{
    for (int st = 0, k = 0; st < M2K_STEPS; st++)
        if (p->step[t][st].flags & ST_TRIG) lk(p, t, st, prm, v[k++ % n]);
}
#define CYC(p, t, prm, ...) cyc(p, t, prm, NOTES(__VA_ARGS__))

static void factory(void)
{
    m2k_pattern_t *p;

    /* A01 LUMEN, after Dataline: A minor at 124, the set's first groove. A round kick, a clap of three on two and four,
     * hats in sixteenths opening on each off-beat; an off-beat sine bass on A1, to F1 and G1 in the second half; a dub
     * stab, Am7 then Fmaj7, thrown into a dotted echo; an FM call, five notes over four trigs a bar, so it lands
     * somewhere new each time round */
    p = part(0, "LUMEN", 124, MACHINES(M_DKIK, M_DSNR, M_DHAT, M_GSIN, M_ENS, M_FMS));
    p->glob[G_SWNG] = 16; p->glob[G_VERB] = 64; p->glob[G_DAMP] = 60; p->glob[G_ADPC] = 10;
    SET(p, 0, SA, DW4(0), SB, 52, SC, 24, SD, 18, SE, 0, SF, 64, SG, 0, SH, 26, P_HOLD, 6, P_DEC, 34, P_REL, 8,
        P_DIST, 74, P_VOL, 71);
    BEAT(p, 0, "x...x...x...x...|x...x...x...x...|x...x...x...x...|x...x...x...x.q.", 33);
    SET(p, 1, SA, DW4(1), SB, 40, SC, 10, SD, 118, SE, 104, SF, 86, SG, 24, SH, 6, P_DEC, 22, P_REL, 8, P_SPU, 40,
        P_VOL, 73);
    BEAT(p, 1, "....X.......X...|....X.......X...|....X.......X...|....X.......X..x", 52);
    SET(p, 2, SG, 5, SH, 0, P_DEC, 90, P_BASE, 44, P_VOL, 69, P_PAN, 70);
    BEAT(p, 2, "xxXxxxXxxxXxxxXx|xxXxxxXxxxXxxxXx|xxXxxxXxxxXxxxXx|xxXxxxXxxxXx2xXx", 60);
    for (int st = 2; st < 64; st += 4) lk(p, 2, st, SH, 16);              /* open on each off-beat */
    SET(p, 3, SA, 8, SB, 0, SC, 0, SD, 6, SE, 20, SF, 10, SG, 24, P_ATK, 2, P_HOLD, 3, P_DEC, 26, P_REL, 10,
        P_GATE, 12, P_DIST, 70, P_VOL, 73);
    BEAT(p, 3, "..x...x...x...x.|..x...x...x...x.|..x...x...x...x.|..x...x...x.x.x.", 33, 33, 33, 45, 33, 33, 33, 45,
         29, 29, 29, 41, 31, 31, 43, 31, 31);
    SET(p, 4, SA, PCH(3), SB, PCH(7), SC, PCH(10), SD, 0, SE, 64, SF, 40, SG, 70, P_ATK, 4, P_DEC, 26, P_REL, 20,
        P_GATE, 6, P_WDTH, 44, P_LPQ, 10, P_FDEC, 30, P_WOF, 90, P_DTIM, 48, P_DSND, 100, P_DFB, 60, P_DWID, 80,
        P_SPU, 50, P_VOL, 127, P_LEVL, 127, P_PAN, 58);
    BEAT(p, 4, "......x.........|..x.......x.....|......x.........|..x.......x.....", 57, 57, 57, 53, 53, 53);
    chords(p, 4, 32, 64, 4, 7, 11);                                         /* Fmaj7 */
    lfo(p, 4, 0, P_WDTH, 1, 0, 20, 76, 0);
    SET(p, 5, SA, FR24(15), SB, 64, SC, 86, SD, 20, SE, FR24(21), SF, 30, SG, 90, SH, 64, P_ATK, 6, P_HOLD, 6,
        P_DEC, 36, P_REL, 20, P_GATE, 10, P_DTIM, 48, P_DSND, 90, P_DFB, 40, P_SPU, 40, P_VOL, 69, P_PAN, 74);
    BEAT(p, 5, "x..x..x...x.....|x..x..x...x.....|x..x..x...x.....|x..x..x...x..x..", 64, 62, 60, 57, 67);
    lfo(p, 5, 0, SH, 1, 4, 46, 67, 1);                                      /* a vibrato on TUNE: 5.7 Hz, +-19 c */
    trim(p, 91);

    /* A02 PULSAR, after Auvrel: G minor at 122, a drum synth alone and a sub. A tight kick; a rim on the off-beats, on
     * 1:2; hats in triplets (3/2x, their own sixteen steps); a ride at half speed; the hook on a second DRM-KICK, an FM
     * pluck swept at 3/4x, five notes against its seven trigs; a sub under it on G1, E♭1, F1 and D1 */
    p = part(1, "PULSAR", 122, MACHINES(M_DKIK, M_DSNR, M_DHAT, M_DCYM, M_DKIK, M_GSIN));
    p->glob[G_VERB] = 56; p->glob[G_DAMP] = 50; p->glob[G_SPD1 + 2] = SPEED(5); p->glob[G_SPD1 + 3] = SPEED(2);
    p->glob[G_SPD1 + 4] = SPEED(3);
    SET(p, 0, SA, DW4(0), SB, 40, SC, 18, SD, 22, SE, 0, SF, 64, SG, 10, SH, 40, P_HOLD, 4, P_DEC, 28, P_REL, 6,
        P_DIST, 76, P_VOL, 112);
    BEAT(p, 0, "x...x...x...x...|x...x...x...x...|x...x...x...x...|x...x...x..xx...", 31);
    SET(p, 1, SA, DW4(3), SB, 30, SC, 6, SD, 127, SE, 50, SF, 100, SG, 0, SH, 6, P_DEC, 12, P_REL, 4, P_SPU, 30,
        P_VOL, 72, P_PAN, 54);
    BEAT(p, 1, "..x...x...h...x.|..x...x...x...H.|..x...x...h...x.|..x...x.x.x...x.", 72);
    SET(p, 2, SA, 60, SB, 66, SC, 100, SD, 120, SE, 90, SF, DW5(0), SG, 5, SH, 0, P_DEC, 90, P_BASE, 46, P_VOL, 126, P_LEVL, 100,
        P_PAN, 72);
    BEAT(p, 2, "x.xx.xx.xx.x.xxx|x.xx.xx.xx.x.xxx|x.xx.xx.xx.x.xxx|x.xx.xx.xx.x.xxx", 60);
    SET(p, 3, SA, 76, SB, 83, SC, 80, SD, 120, SE, 40, SF, DW5(3), SG, 0, SH, 40, P_DEC, 90, P_BASE, 40, P_VOL, 83,
        P_PAN, 46);
    BEAT(p, 3, "x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.", 60);
    SET(p, 4, SA, DW4(0), SB, 16, SC, 6, SD, 30, SE, 70, SF, 83, SG, 0, SH, 0, P_HOLD, 2, P_DEC, 18, P_REL, 8,
        P_DTIM, 48, P_DSND, 96, P_DFB, 50, P_DWID, 70, P_SPU, 40, P_VOL, 92, P_PAN, 64);
    BEAT(p, 4, "x..x.x..x..x.x..|x..x.x..x..x.x..|x..x.x..x..x.x..|x..x.x..x..x.x..", 55, 58, 62, 65, 60);
    SET(p, 5, SA, 0, SB, 0, SC, 0, SD, 0, SE, 40, SF, 6, SG, 16, P_HOLD, 24, P_DEC, 40, P_REL, 16, P_GATE, 40,
        P_DIST, 70, P_VOL, 95);
    BEAT(p, 5, "x.......x.......|x.......x.....x.|x.......x.......|x.......x.......", 31, 31, 27, 27, 31, 29, 29, 26);
    trim(p, 60);

    /* A03 LATTICE, after Sqein: D phrygian at 120, dark and polymetric under a long dark reverb. A deep kick in
     * 3 3 2; FM+PAR metal at 3/4x, against the bar; ticks at 3/2x, half of them by chance; a growling FM bass on D1,
     * sliding to E♭1; a drone of D, A and E♭ swelling; a snare on three, half time, deep in the reverb */
    p = part(2, "LATTICE", 120, MACHINES(M_DKIK, M_FMP, M_DHAT, M_FMD, M_ENS, M_DSNR));
    p->glob[G_VERB] = 110; p->glob[G_DAMP] = 96; p->glob[G_HISS] = 16; p->glob[G_ADPC] = 20;
    p->glob[G_SPD1 + 1] = SPEED(3); p->glob[G_SPD1 + 2] = SPEED(5);
    SET(p, 0, SA, DW4(0), SB, 60, SC, 30, SD, 26, SE, 10, SF, 76, SG, 0, SH, 14, P_HOLD, 6, P_DEC, 36, P_REL, 8,
        P_DIST, 78, P_VOL, 100);
    BEAT(p, 0, "x..x..x.x..x..x.|x..x..x.x..x....|x..x..x.x..x..x.|x..x..x.x...x.x.", 26);
    SET(p, 1, SA, FR24(22), SB, 100, SC, FR24(16), SD, 90, SE, FR24(23), SF, 110, SG, 100, SH, 64, P_DEC, 20,
        P_REL, 10, P_SPU, 90, P_DTIM, 64, P_DSND, 80, P_DFB, 50, P_VOL, 50, P_PAN, 40);
    BEAT(p, 1, "x....x...x..x...|x....x...x..x...|x....x...x..x...|x....x...x..x...", 62, 62, 63, 57);
    SET(p, 2, SA, 72, SB, 80, SC, 60, SD, 127, SE, 100, SF, DW5(0), SG, 5, SH, 0, P_DEC, 90, P_BASE, 52, P_VOL, 127, P_LEVL, 114,
        P_PAN, 80);
    BEAT(p, 2, "x.x?x.?x.x?..x?x|x.x?x.?x.x?..x?x|x.x?x.?x.x?..x?x|x.x?x.?x.x?..x?x", 60);
    SET(p, 3, SA, 64, SB, 64, SC, 36, SD, 74, SE, 64, SF, 40, SG, 20, SH, 64, P_HOLD, 20, P_DEC, 44, P_REL, 16,
        P_GATE, 30, P_DIST, 78, P_VOL, 127, P_LEVL, 127);
    BEAT(p, 3, "x.......x..s....|x.......x.......|x.......x..s....|x...x...x.......", 26, 26, 27, 26, 26, 26, 26, 27,
         26, 26, 38, 26);
    lfo(p, 3, 0, SC, 1, 1, 24, 80, 0);                                       /* the growl, slowly */
    SET(p, 4, SA, PCH(7), SB, PCH(1), SC, PCH(12), SD, 40, SE, 64, SF, 80, SG, 100, P_ATK, 70, P_HOLD, 127,
        P_DEC, 127, P_REL, 80, P_GATE, 127, P_WDTH, 30, P_SPU, 100, P_VOL, 12);
    BEAT(p, 4, "x...............................|x...............................", 50);
    SET(p, 5, SA, DW4(1), SB, 60, SC, 16, SD, 90, SE, 90, SF, 40, SG, 0, SH, 6, P_DEC, 30, P_REL, 12, P_SPU, 110,
        P_VOL, 65);
    BEAT(p, 5, "........X.......|........X......q|........X.......|........X...q...", 48);
    trim(p, 45);

    /* A04 SPUDUB, after Dataline and the PS1's SPU: C minor at 118, dub techno. A soft deep kick; a shaker in swung
     * sixteenths; a rim off the beat; a Cm7 stab thrown into a ping-pong echo under the SPU reverb, its filter breathing;
     * a sparse sub, C1 to E♭1; open hats on the off-beats */
    p = part(3, "SPUDUB", 118, MACHINES(M_DKIK, M_GPRC, M_DSNR, M_ENS, M_GSIN, M_DHAT));
    p->glob[G_SWNG] = 20; p->glob[G_VERB] = 96; p->glob[G_DAMP] = 70; p->glob[G_ADPC] = 34; p->glob[G_HISS] = 30;
    SET(p, 0, SA, DW4(0), SB, 36, SC, 28, SD, 30, SE, 0, SF, 64, SG, 0, SH, 8, P_HOLD, 6, P_DEC, 36, P_REL, 8,
        P_DIST, 72, P_VOL, 110);
    BEAT(p, 0, "x...x...x...x...|x...x...x...x...|x...x...x...x...|x...x...x...x...", 31);
    SET(p, 1, SA, 100, SB, 0, SC, 0, SD, 0, SE, 0, SF, 0, SG, 0, P_DEC, 8, P_REL, 4, P_BASE, 50, P_VOL, 66, P_PAN, 76);
    BEAT(p, 1, "xxXxxxXxxxXxxxXx|xxXxxxXxxxXxxxXx|xxXxxxXxxxXxxxXx|xxXxxxXxxxXxxxXx", 60);
    SET(p, 2, SA, DW4(3), SB, 20, SC, 4, SD, 127, SE, 30, SF, 110, SG, 0, SH, 6, P_DEC, 10, P_REL, 4, P_DTIM, 48,
        P_DSND, 90, P_DFB, 40, P_SPU, 50, P_VOL, 95, P_PAN, 40);
    BEAT(p, 2, "...x..x....x.x..|...x..x....x....|...x..x....x.x..|...x..x...x..x..", 76);
    SET(p, 3, SA, PCH(3), SB, PCH(7), SC, PCH(10), SD, 20, SE, 64, SF, 50, SG, 80, P_ATK, 2, P_DEC, 22, P_REL, 30,
        P_GATE, 4, P_WDTH, 36, P_LPQ, 24, P_DTIM, 48, P_DSND, 104, P_DFB, 70, P_DBAS, 20, P_DWID, 100, P_SPU, 70,
        P_BITS, 20, P_SRR, 20, P_VOL, 121);
    BEAT(p, 3, "......x.........|..x.......x.....|......x.........|..x.....x.......", 60);
    lfo(p, 3, 0, P_WDTH, 0, 0, 12, 80, 0);
    SET(p, 4, SA, 0, SB, 60, SC, 0, SD, 0, SE, 40, SF, 6, SG, 10, P_HOLD, 16, P_DEC, 36, P_REL, 14, P_GATE, 30,
        P_DIST, 70, P_VOL, 94);
    BEAT(p, 4, "x.........x.....|....x.......x...|x.........x.....|....x.....x.....", 36, 36, 39, 34, 36, 36, 39, 39);
    SET(p, 5, SG, 5, SH, 14, P_DEC, 90, P_BASE, 48, P_VOL, 118, P_PAN, 60);
    BEAT(p, 5, "..x...x...x...x.|..x...x...x...x.|..x...x...x...x.|..x...x...x...x.", 60);
    trim(p, 59);

    /* A05 PROCESS, after 0F.digital's process music: A dorian at 126, every track on its own clock. A dry kick on the
     * beat; sine bloops in triplets (3/2x), five notes over six trigs, echoing in eighths; an FM tick at 3/4x, by
     * chance; noise in thirty-seconds (2x), most of them by chance; open hats off the beat; a bass on A1 jumping the
     * octave, to G1 at the turn */
    p = part(4, "PROCESS", 126, MACHINES(M_DKIK, M_GSIN, M_FMS, M_GNOI, M_DHAT, M_GSIN));
    p->glob[G_VERB] = 40; p->glob[G_DAMP] = 50; p->glob[G_ADPC] = 40;
    p->glob[G_SPD1 + 1] = SPEED(5); p->glob[G_SPD1 + 2] = SPEED(3); p->glob[G_SPD1 + 3] = SPEED(6);
    SET(p, 0, SA, DW4(0), SB, 44, SC, 20, SD, 20, SE, 0, SF, 64, SG, 0, SH, 30, P_HOLD, 4, P_DEC, 30, P_REL, 6,
        P_DIST, 76, P_VOL, 112);
    BEAT(p, 0, "x...x...x...x...|x...x...x...x...|x...x...x...x...|x...x...x...k...", 33);
    SET(p, 1, SA, 0, SB, 0, SC, 0, SD, 24, SE, 16, SF, 10, SG, 10, P_DEC, 10, P_REL, 4, P_BASE, 20, P_BITS, 30,
        P_DTIM, 32, P_DSND, 96, P_DFB, 50, P_DWID, 90, P_SPU, 30, P_VOL, 127, P_LEVL, 107, P_PAN, 70);
    BEAT(p, 1, "x.x..xx..x.x....|x.x..xx..x.x....|x.x..xx..x.x....|x.x..xx..x.x....", 69, 72, 76, 67, 74);
    SET(p, 2, SA, FR24(22), SB, 64, SC, 120, SD, 30, SE, FR24(19), SF, 20, SG, 110, SH, 64, P_DEC, 10, P_REL, 4,
        P_BASE, 30, P_SPU, 40, P_VOL, 120, P_PAN, 46);
    BEAT(p, 2, "x..?..x?.x..?...|x..?..x?.x..?...|x..?..x?.x..?...|x..?..x?.x..?...", 57, 64);
    SET(p, 3, SA, 0, SB, 0, SC, 0, P_DEC, 4, P_REL, 2, P_BASE, 60, P_BITS, 40, P_VOL, 125, P_LEVL, 105, P_PAN, 84);
    BEAT(p, 3, "x?q?x?q?x?q?x?q?|x?q?x?q?x?q?x?q?|x?q?x?q?x?q?x?q?|x?q?x?q?x?q?x?q?", 60);
    SET(p, 4, SG, 5, SH, 12, P_DEC, 90, P_BASE, 46, P_VOL, 112, P_PAN, 56);
    BEAT(p, 4, "..x...x...x...x.|..x...x...x...x.|..x...x...x...x.|..x...x...x.x.x.", 60);
    SET(p, 5, SA, 6, SB, 0, SC, 0, SD, 10, SE, 20, SF, 8, SG, 30, P_HOLD, 4, P_DEC, 24, P_REL, 8, P_GATE, 10,
        P_DIST, 72, P_VOL, 103);
    BEAT(p, 5, "..x...x...x.x.x.|..x...x...x.x.x.|..x...x...x.x.x.|..x...x...x.x.x.", 33, 45, 33, 33, 45, 33, 45, 33, 33,
         45, 33, 33, 31, 43, 31);
    trim(p, 66);

    /* A06 SQUELCH, after Dataline's TB-03: E minor at 128, acid. A 909-ish kick on E1; a SID saw through the resonant
     * lowpass, its envelope wide, sliding and accented (each accent's filter thrown open), the cutoff drifting over the
     * bars; hats opening on the off-beat; a clap on two and four; a quiet ride in eighths; a rim on the off sixteenths */
    p = part(5, "SQUELCH", 128, MACHINES(M_DKIK, M_SID, M_DHAT, M_DSNR, M_DCYM, M_GPRC));
    p->glob[G_VERB] = 40; p->glob[G_DAMP] = 60; p->glob[G_ACNT] = 70; p->glob[G_SWNG] = 8;
    SET(p, 0, SA, DW4(0), SB, 56, SC, 22, SD, 16, SE, 0, SF, 64, SG, 6, SH, 44, P_HOLD, 6, P_DEC, 36, P_REL, 6,
        P_DIST, 80, P_VOL, 112);
    BEAT(p, 0, "x...x...x...x...|x...x...x...x...|x...x...x...x...|x...x...x...x.x.", 28);
    SET(p, 1, SA, 0, SB, 0, SC, 0, SD, SID5(1), SE, SIDM(0), SF, 0, SG, 0, SH, 64, P_HOLD, 10, P_DEC, 30, P_REL, 6,
        P_GATE, 12, P_PORT, 0, P_WDTH, 32, P_LPQ, 46, P_FDEC, 24, P_WOF, 110, P_DIST, 82, P_VOL, 109);
    BEAT(p, 1, "x.xSx.x.xxS.x.xS|x.xSx.x.xx.Sx.x.|x.xSx.x.xxS.x.xS|x.Sxx.x.xxS.xSx.",
         40, 40, 52, 43, 40, 47, 40, 50, 52, 40, 43, 47, 40, 40, 52, 43, 40, 47, 38, 40, 52, 40, 43, 45);
    for (int st = 0; st < 64; st++) if (p->step[1][st].flags & ST_ACC) lk(p, 1, st, P_WOF, 124);
    lfo(p, 1, 0, P_WDTH, 0, 0, 8, 90, 0);                                    /* the cutoff, over the bars */
    SET(p, 2, SG, 5, SH, 0, P_DEC, 90, P_BASE, 46, P_VOL, 86, P_PAN, 70);
    BEAT(p, 2, "x.X.x.X.x.X.x.X.|x.X.x.X.x.X.x.X.|x.X.x.X.x.X.x.X.|x.X.x.X.x.X.x2X.", 60);
    for (int st = 2; st < 64; st += 4) lk(p, 2, st, SH, 18);
    SET(p, 3, SA, DW4(1), SB, 40, SC, 10, SD, 120, SE, 106, SF, 84, SG, 24, SH, 6, P_DEC, 22, P_REL, 8, P_SPU, 30,
        P_VOL, 92);
    BEAT(p, 3, "....X.......X...|....X.......X...|....X.......X...|....X.......X...", 52);
    SET(p, 4, SA, 76, SB, 83, SC, 80, SD, 120, SE, 40, SF, DW5(3), SG, 0, SH, 36, P_DEC, 90, P_BASE, 44, P_VOL, 98,
        P_PAN, 44);
    BEAT(p, 4, "x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.", 64);
    SET(p, 5, SA, 110, SB, 0, SC, 60, SD, 40, SE, 0, SF, 0, SG, 20, P_DEC, 6, P_REL, 4, P_BASE, 40, P_VOL, 86, P_PAN, 80);
    BEAT(p, 5, ".x.....x...x....|.x.....x.x......|.x.....x...x....|.x.....x...x..x.", 64);
    trim(p, 55);

    /* A07 GROWL, after LØSTLVLS's LO sounds: F minor at 130, techno with a growl. A rumbling kick on F1; an FM+DYN
     * growl on the off-beats, F2 to A♭2 and C3, its depth thrown hit to hit by a random LFO, a sine sub under it an
     * octave down; sixteenths of hats, open off the beat; a snare on two and four, rolling into the turn; a noise
     * riser every four bars */
    p = part(6, "GROWL", 130, MACHINES(M_DKIK, M_FMD, M_DHAT, M_DSNR, M_GNOI, M_GSIN));
    p->glob[G_VERB] = 50; p->glob[G_DAMP] = 70; p->glob[G_ADPC] = 16;
    SET(p, 0, SA, DW4(0), SB, 64, SC, 30, SD, 24, SE, 20, SF, 76, SG, 0, SH, 30, P_HOLD, 6, P_DEC, 40, P_REL, 8,
        P_DIST, 82, P_VOL, 114);
    BEAT(p, 0, "x...x...x...x..x|x...x...x...x...|x...x...x...x..x|x...x...x...x...", 29);
    SET(p, 1, SA, 64, SB, 64, SC, 80, SD, 90, SE, 90, SF, 70, SG, 70, SH, 64, P_HOLD, 8, P_DEC, 28, P_REL, 8,
        P_GATE, 10, P_WDTH, 50, P_LPQ, 20, P_BASE, 10, P_DIST, 84, P_VOL, 66, P_LEVL, 100);
    BEAT(p, 1, "..x...x...x...x.|..x...x...x.s.x.|..x...x...x...x.|..x...x...x.s.x.", 41, 41, 44, 41, 41, 41, 48, 44,
         41, 41, 44, 41, 41, 44, 48, 53, 48);
    lfo(p, 1, 0, SC, 6, 3, 40, 90, 0);                                       /* the growl's depth, hit to hit */
    SET(p, 2, SG, 5, SH, 0, P_DEC, 90, P_BASE, 46, P_VOL, 90, P_PAN, 70);
    BEAT(p, 2, "xxXxxxXxxxXxxxXx|xxXxxxXxxxXxxxXx|xxXxxxXxxxXxxxXx|xxXxxxXxxxXx3xXx", 60);
    for (int st = 2; st < 64; st += 4) lk(p, 2, st, SH, 14);
    SET(p, 3, SA, DW4(1), SB, 60, SC, 14, SD, 110, SE, 80, SF, 76, SG, 0, SH, 6, P_DEC, 24, P_REL, 8, P_SPU, 40,
        P_VOL, 74);
    BEAT(p, 3, "....X.......X...|....X.......X...|....X.......X...|....X.......X.33", 50);
    SET(p, 4, SA, 0, SB, 30, SC, 0, P_ATK, 96, P_HOLD, 127, P_DEC, 127, P_REL, 20, P_GATE, 127, P_BASE, 20,
        P_BITS, 20, P_SPU, 60, P_VOL, 89, P_PAN, 64);
    BEAT(p, 4, "................|................|................|x...............", 60);
    lfo(p, 4, 0, P_BASE, 3, 0, 20, 110, 1);                                  /* the riser, opening */
    SET(p, 5, SA, 0, SB, 0, SC, 0, SD, 0, SE, 40, SF, 6, SG, 10, P_HOLD, 8, P_DEC, 26, P_REL, 8, P_GATE, 10,
        P_DIST, 70, P_VOL, 98);
    BEAT(p, 5, "..x...x...x...x.|..x...x...x.s.x.|..x...x...x...x.|..x...x...x.s.x.", 29, 29, 32, 29, 29, 29, 36, 32,
         29, 29, 32, 29, 29, 32, 36, 41, 36);
    trim(p, 57);

    /* A08 BREAKS, after Dataline: E minor at 132, breaks. A kick off the grid and a snare on two and four with ghost
     * notes round them; hats by chance; a reese, SWAV-SAW's detuned pair beating, its sine sub an octave down on E1,
     * G1, D1; a hoover, a SID saw synced to a swept second oscillator, diving from B3 to E3 at the end of every second
     * bar; an Em7 pad behind */
    p = part(7, "BREAKS", 132, MACHINES(M_DKIK, M_DSNR, M_DHAT, M_SAW, M_SID, M_ENS));
    p->glob[G_VERB] = 56; p->glob[G_DAMP] = 60; p->glob[G_SWNG] = 12; p->glob[G_GLCH] = 16; p->glob[G_GSIZ] = GSIZE(2);
    SET(p, 0, SA, DW4(0), SB, 48, SC, 22, SD, 20, SE, 0, SF, 64, SG, 0, SH, 34, P_HOLD, 4, P_DEC, 32, P_REL, 6,
        P_DIST, 78, P_VOL, 80);
    BEAT(p, 0, "x.....x...x.....|x.....x.x..x....|x.....x...x.....|x.....x.x....x..", 31);
    SET(p, 1, SA, DW4(1), SB, 50, SC, 12, SD, 116, SE, 80, SF, 80, SG, 0, SH, 6, P_DEC, 26, P_REL, 8, P_SPU, 30,
        P_VOL, 66);
    BEAT(p, 1, "....X..x.x..X...|....X..x..x.X..x|....X..x.x..X...|....X..x..x.X.xx", 52);
    static const uint8_t ghosts8[] = {7, 9, 23, 26, 31, 39, 41, 55, 58, 62, 63};
    for (unsigned i = 0; i < sizeof ghosts8; i++) lk(p, 1, ghosts8[i], P_VOL, 54);
    SET(p, 2, SG, 4, SH, 0, P_DEC, 90, P_BASE, 46, P_VOL, 80, P_PAN, 68);
    BEAT(p, 2, "x?x?x?X?x?x?x?X?|x?x?x?X?x?x?x?X?|x?x?x?X?x?x?x?X?|x?x?x?X?x?x?x?X2", 60);
    lk(p, 2, 14, SH, 16); lk(p, 2, 46, SH, 16);
    SET(p, 3, SA, 127, SB, 36, SC, 0, SD, 10, SE, 0, SF, 110, SG, 0, SH, 64, P_HOLD, 40, P_DEC, 44, P_REL, 14,
        P_GATE, 60, P_PORT, 20, P_WDTH, 40, P_LPQ, 10, P_DIST, 76, P_VOL, 127, P_LEVL, 110);
    BEAT(p, 3, "x.......x.....x.|x.......s.......|x.......x.....x.|x.......s.......", 40, 40, 43, 40, 38, 40, 40, 43, 40,
         38);
    SET(p, 4, SA, 0, SB, 0, SC, 0, SD, SID5(1), SE, SIDM(2), SF, 0, SG, 40, SH, 64, P_HOLD, 8, P_DEC, 30, P_REL, 20,
        P_GATE, 20, P_PORT, 40, P_WDTH, 60, P_DIST, 76, P_SPU, 40, P_VOL, 89, P_LEVL, 100);
    BEAT(p, 4, "................|..........x.s...|................|..........x.s...", 71, 64);
    lfo(p, 4, 0, SG, 0, 0, 30, 100, 1);                                      /* the sync's sweep, from each stab */
    SET(p, 5, SA, PCH(3), SB, PCH(7), SC, PCH(10), SD, 20, SE, 64, SF, 80, SG, 100, P_ATK, 80, P_HOLD, 127,
        P_DEC, 127, P_REL, 70, P_GATE, 127, P_WDTH, 36, P_SPU, 70, P_VOL, 96, P_LEVL, 100);
    BEAT(p, 5, "x...............................|x...............................", 52);
    trim(p, 82);

    /* A09 SPRITES, after sm0hm's Game Boy chiptek: F# minor at 136. A hard kick on F#1; a SID pulse bass pumping the
     * octave in sixteenths, F#m, D, E, C#m a bar each; a SID square arpeggio of each chord in sixteenths, three notes
     * over sixteen steps so each bar starts on another; noise hats crushed to the console's, open off the beat; a clap,
     * rolling into the turn; a pulse lead with its unison, a hook of five notes in echoes */
    p = part(8, "SPRITES", 136, MACHINES(M_DKIK, M_SID, M_SID, M_GNOI, M_DSNR, M_PUL));
    p->glob[G_VERB] = 50; p->glob[G_DAMP] = 50; p->glob[G_ADPC] = 30;
    SET(p, 0, SA, DW4(0), SB, 60, SC, 18, SD, 14, SE, 0, SF, 64, SG, 8, SH, 50, P_HOLD, 4, P_DEC, 30, P_REL, 6,
        P_DIST, 84, P_VOL, 112);
    BEAT(p, 0, "x...x...x...x...|x...x...x...x...|x...x...x...x...|x...x...x...x.x.", 30);
    SET(p, 1, SA, 32, SB, 0, SC, 0, SD, SID5(2), SE, SIDM(0), SF, 0, SG, 0, SH, 64, P_HOLD, 2, P_DEC, 16, P_REL, 4,
        P_GATE, 8, P_WDTH, 50, P_DIST, 76, P_VOL, 111);
    BEAT(p, 1, ".xxx.xxx.xxx.xxx|.xxx.xxx.xxx.xxx|.xxx.xxx.xxx.xxx|.xxx.xxx.xxx.xxx",
         30, 42, 30, 30, 42, 30, 30, 42, 30, 30, 42, 30, 26, 38, 26, 26, 38, 26, 26, 38, 26, 26, 38, 26,
         28, 40, 28, 28, 40, 28, 28, 40, 28, 28, 40, 28, 25, 37, 25, 25, 37, 25, 25, 37, 25, 25, 37, 25);
    SET(p, 2, SA, 0, SB, 0, SC, 0, SD, SID5(2), SE, SIDM(0), SF, 0, SG, 0, SH, 64, P_DEC, 12, P_REL, 4, P_GATE, 8,
        P_WDTH, 60, P_DTIM, 48, P_DSND, 86, P_DFB, 40, P_SPU, 30, P_VOL, 98, P_PAN, 72);
    BEAT(p, 2, "xxxxxxxxxxxxxxxx|xxxxxxxxxxxxxxxx|xxxxxxxxxxxxxxxx|xxxxxxxxxxxxxxxx",
         66, 69, 73, 66, 69, 73, 66, 69, 73, 66, 69, 73, 66, 69, 73, 66, 62, 66, 69, 62, 66, 69, 62, 66, 69, 62, 66, 69,
         62, 66, 69, 62, 64, 68, 71, 64, 68, 71, 64, 68, 71, 64, 68, 71, 64, 68, 71, 64, 61, 64, 68, 61, 64, 68, 61, 64,
         68, 61, 64, 68, 61, 64, 68, 61);
    SET(p, 3, SA, 0, SB, 0, SC, 0, P_DEC, 6, P_REL, 2, P_BASE, 56, P_BITS, 50, P_VOL, 83, P_PAN, 60);
    BEAT(p, 3, "x.X.x.X.x.X.x.X.|x.X.x.X.x.X.x.X.|x.X.x.X.x.X.x.X.|x.X.x.X.x.X.xxXx", 60);
    for (int st = 2; st < 64; st += 4) lk(p, 3, st, P_DEC, 20);
    SET(p, 4, SA, DW4(1), SB, 40, SC, 10, SD, 120, SE, 104, SF, 86, SG, 24, SH, 6, P_DEC, 22, P_REL, 8, P_SPU, 30,
        P_VOL, 104);
    BEAT(p, 4, "....X.......X...|....X.......X...|....X.......X...|....X.......X.44", 52);
    SET(p, 5, SA, 110, SB, 70, SC, 0, SD, 0, SE, 64, SF, 0, SG, 0, SH, 64, P_ATK, 2, P_HOLD, 8, P_DEC, 30,
        P_REL, 16, P_GATE, 10, P_DTIM, 48, P_DSND, 92, P_DFB, 50, P_DWID, 90, P_SPU, 40, P_VOL, 127, P_LEVL, 102, P_PAN, 56);
    BEAT(p, 5, "x..x..x.x.......|................|x..x..x.x.......|..........x.x...", 73, 71, 69, 66, 71, 68, 64, 71,
         68, 73);                                                         /* the chords' tones, bar by bar */
    trim(p, 60);

    /* A10 OPLL, after sm0hm's NES FM (a VRC7 under the console's channels): B minor at 128. A short kick with a snap; the
     * noise channel as hats and a snare; the triangle's bass bouncing octaves, Bm, G, D, A; an FM lead of two operators
     * sliding through its phrase, a vibrato coming in on each note; the 12.5 % pulse in arpeggios; a crash every four
     * bars */
    p = part(9, "OPLL", 128, MACHINES(M_DKIK, M_GNOI, M_SID, M_FMS, M_SID, M_DCYM));
    p->glob[G_VERB] = 40; p->glob[G_DAMP] = 50; p->glob[G_ADPC] = 24;
    SET(p, 0, SA, DW4(0), SB, 40, SC, 12, SD, 20, SE, 0, SF, 64, SG, 60, SH, 20, P_HOLD, 4, P_DEC, 22, P_REL, 4,
        P_DIST, 76, P_VOL, 110);
    BEAT(p, 0, "x.......x.x.....|x.......x.......|x.......x.x.....|x.......x...x.x.", 35);
    SET(p, 1, SA, 0, SB, 0, SC, 0, P_DEC, 6, P_REL, 2, P_BASE, 50, P_BITS, 60, P_VOL, 54, P_PAN, 60);
    BEAT(p, 1, "x.x.X.x.x.x.X.x.|x.x.X.x.x.x.X.x.|x.x.X.x.x.x.X.x.|x.x.X.x.x.x.X.XX", 60);
    for (int st = 4; st < 64; st += 8) { lk(p, 1, st, P_DEC, 22); lk(p, 1, st, P_BASE, 30); }   /* the snare */
    SET(p, 2, SA, 0, SB, 0, SC, 0, SD, SID5(0), SE, SIDM(0), SF, 0, SG, 0, SH, 64, P_HOLD, 4, P_DEC, 22, P_REL, 6,
        P_GATE, 10, P_VOL, 127);
    BEAT(p, 2, "x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.", 35, 47, 35, 47, 35, 47, 35, 47,
         31, 43, 31, 43, 31, 43, 31, 43, 38, 50, 38, 50, 38, 50, 38, 50, 33, 45, 33, 45, 33, 45, 33, 45);
    SET(p, 3, SA, FR24(15), SB, 64, SC, 70, SD, 40, SE, FR24(19), SF, 20, SG, 100, SH, 64, P_ATK, 2, P_HOLD, 16,
        P_DEC, 40, P_REL, 20, P_GATE, 24, P_PORT, 24, P_DTIM, 48, P_DSND, 80, P_DFB, 30, P_SPU, 30, P_VOL, 96, P_PAN, 70);
    BEAT(p, 3, "x.....x...x.s...|x.......x.......|x.....x...x.s...|x...x...x.......", 71, 74, 76, 74, 71, 67, 69, 74, 76,
         74, 73, 76, 69);                                                 /* Bm, G, D, A: on their tones */
    lfo(p, 3, 0, SH, 1, 4, 46, 67, 1);                                       /* a vibrato on TUNE, from each note */
    SET(p, 4, SA, 48, SB, 0, SC, 0, SD, SID5(2), SE, SIDM(0), SF, 0, SG, 0, SH, 64, P_DEC, 10, P_REL, 4, P_GATE, 6,
        P_WDTH, 64, P_DTIM, 32, P_DSND, 80, P_DFB, 30, P_VOL, 125, P_PAN, 44);
    BEAT(p, 4, "x.xxx.xxx.xxx.xx|x.xxx.xxx.xxx.xx|x.xxx.xxx.xxx.xx|x.xxx.xxx.xxx.xx", 59, 62, 66, 62, 59, 62, 66, 62, 59,
         62, 66, 62, 55, 59, 62, 59, 55, 59, 62, 59, 55, 59, 62, 59, 62, 66, 69, 66, 62, 66, 69, 66, 62, 66, 69, 66,
         57, 61, 64, 61, 57, 61, 64, 61, 57, 61, 64, 61);
    SET(p, 5, SA, 127, SB, 64, SC, 0, SD, 64, SE, 0, SF, DW5(4), SG, 0, SH, 104, P_DEC, 90, P_BASE, 40, P_SPU, 50,
        P_VOL, 68, P_PAN, 64);
    BEAT(p, 5, "x...............|................|................|................", 60);
    trim(p, 63);

    /* A11 JUKE, after foodman and suesett's footwork: F minor at 160. A long 808 kick in footwork's threes against the
     * bar; a clap on two and four; rapid hats ratcheting, by chance; VO-6 chopping a vowel in threes, each hit another
     * vowel; an 808 sub dropping into each note, F1, D♭1, E♭1; a SID stab answering */
    p = part(10, "JUKE", 160, MACHINES(M_DKIK, M_DSNR, M_DHAT, M_VOX, M_GSIN, M_SID));
    p->glob[G_VERB] = 40; p->glob[G_DAMP] = 60; p->glob[G_GLCH] = 12; p->glob[G_GSIZ] = GSIZE(1);
    SET(p, 0, SA, DW4(0), SB, 30, SC, 40, SD, 40, SE, 0, SF, 64, SG, 0, SH, 10, P_HOLD, 10, P_DEC, 50, P_REL, 8,
        P_DIST, 78, P_VOL, 70);
    BEAT(p, 0, "x..x..x..x..x...|x..x..x..x.x..x.|x..x..x..x..x...|x..x.x..x..x.x..", 29);
    SET(p, 1, SA, DW4(1), SB, 40, SC, 10, SD, 120, SE, 100, SF, 84, SG, 16, SH, 6, P_DEC, 22, P_REL, 8, P_SPU, 30,
        P_VOL, 73);
    BEAT(p, 1, "....X.......X...|....X.......X..x|....X.......X...|....X.....X.X...", 52);
    SET(p, 2, SG, 4, SH, 0, P_DEC, 90, P_BASE, 48, P_VOL, 101, P_PAN, 70);
    BEAT(p, 2, "x?3?x?x2x?3?x?x?|x?3?x?x2x?3?x4x?|x?3?x?x2x?3?x?x?|x?3?x?x2x?4?x3x?", 60);
    SET(p, 3, SA, 40, SB, 90, SC, 0, SD, 0, SE, 0, SF, 64, SG, 64, SH, 64, P_ATK, 0, P_HOLD, 10, P_DEC, 26, P_REL, 8,
        P_GATE, 10, P_SPU, 30, P_VOL, 126, P_LEVL, 122, P_PAN, 58);
    BEAT(p, 3, "x..x..x.....x..x|..x.....x..x..x.|x..x..x.....x..x|..x..x..x..x....", 65, 68, 72, 65, 68, 65);
    CYC(p, 3, SA, 20, 40, 70, 90, 50);                                       /* each hit another vowel */
    SET(p, 4, SA, 0, SB, 0, SC, 0, SD, 30, SE, 30, SF, 6, SG, 20, P_HOLD, 24, P_DEC, 50, P_REL, 16, P_GATE, 40,
        P_DIST, 72, P_VOL, 62);
    BEAT(p, 4, "x.....x.....x...|x.....x.....x.x.|x.....x.....x...|x.....x...x.....", 29, 29, 25, 29, 29, 27, 25, 29, 29,
         25, 27, 24);
    SET(p, 5, SA, 32, SB, 0, SC, 0, SD, SID5(2), SE, SIDM(0), SF, 0, SG, 0, SH, 64, P_DEC, 14, P_REL, 4, P_GATE, 6,
        P_WDTH, 50, P_DTIM, 48, P_DSND, 90, P_DFB, 40, P_VOL, 66, P_PAN, 74);
    BEAT(p, 5, "..........x.....|.......x........|..........x.....|.......x..x.....", 77, 75, 77, 72, 75);
    trim(p, 72);

    /* A12 CHIPJNG, after KUF's chip jungle: A minor at 172. A kick and a snare cut as a break, ghosts and rolls round
     * them; a SID saw bass, dark and held, A1, F1, G1, E1, sliding; a square lead in eighths over it, echoing; a ride
     * in eighths; hats in sixteenths, by chance */
    p = part(11, "CHIPJNG", 172, MACHINES(M_DKIK, M_DSNR, M_DCYM, M_SID, M_SID, M_DHAT));
    p->glob[G_VERB] = 46; p->glob[G_DAMP] = 60; p->glob[G_ADPC] = 20; p->glob[G_GLCH] = 20; p->glob[G_GSIZ] = GSIZE(2);
    SET(p, 0, SA, DW4(0), SB, 50, SC, 18, SD, 20, SE, 0, SF, 64, SG, 20, SH, 40, P_HOLD, 4, P_DEC, 30, P_REL, 6,
        P_DIST, 80, P_VOL, 110);
    BEAT(p, 0, "x.x.......x.....|x.x.......x..x..|x.x.......x.....|..x..x....x.....", 33);
    SET(p, 1, SA, DW4(1), SB, 60, SC, 12, SD, 120, SE, 70, SF, 70, SG, 0, SH, 6, P_DEC, 26, P_REL, 8, P_SPU, 30,
        P_VOL, 75);
    BEAT(p, 1, "....X..x.x..X..x|.x..X.....x.X...|....X..x.x..X..x|.x..X..x..X.X.33", 52);
    static const uint8_t ghosts12[] = {7, 9, 15, 17, 39, 41, 47, 49, 55};
    for (unsigned i = 0; i < sizeof ghosts12; i++) lk(p, 1, ghosts12[i], P_VOL, 52);
    SET(p, 2, SA, 76, SB, 83, SC, 80, SD, 120, SE, 40, SF, DW5(3), SG, 0, SH, 34, P_DEC, 90, P_BASE, 44, P_VOL, 97,
        P_PAN, 46);
    BEAT(p, 2, "x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.", 64);
    SET(p, 3, SA, 0, SB, 0, SC, 0, SD, SID5(1), SE, SIDM(0), SF, 0, SG, 0, SH, 64, P_HOLD, 48, P_DEC, 44, P_REL, 16,
        P_GATE, 64, P_PORT, 16, P_WDTH, 32, P_DIST, 78, P_VOL, 104);
    BEAT(p, 3, "x...............|x.......s.......|x...............|x.......s.......", 33, 29, 31, 31, 28, 33);
    SET(p, 4, SA, 0, SB, 0, SC, 0, SD, SID5(2), SE, SIDM(0), SF, 0, SG, 0, SH, 64, P_HOLD, 6, P_DEC, 24, P_REL, 10,
        P_GATE, 10, P_WDTH, 64, P_DTIM, 48, P_DSND, 90, P_DFB, 50, P_SPU, 40, P_VOL, 65, P_PAN, 70);
    BEAT(p, 4, "x.x...x.x.x...x.|x.x.x...x.......|x.x...x.x.x...x.|x.x.x...x...x...", 69, 72, 71, 67, 64, 67, 69, 72,
         76, 74, 72, 71, 69, 67, 64);
    SET(p, 5, SG, 4, SH, 0, P_DEC, 90, P_BASE, 48, P_VOL, 127, P_LEVL, 108, P_PAN, 74);
    BEAT(p, 5, "x?x?x?x?x?x?x?x?|x?x?x?x?x?x?x?x?|x?x?x?x?x?x?x?x?|x?x?x?x?x?x?x?x?", 60);
    trim(p, 62);

    /* A13 VIOLET, after L-tron's hologram dance: G minor at 112, half time, in slow motion. A soft kick, a snare on
     * three deep in the reverb, hats in swung eighths; two squares harmonising a bar a chord, the pulse's voice over
     * the SID's (D5 over B♭4, B♭4 over G4, C5 over A4, A4 over F4: Gm, E♭, F, Dm), the tape slowing them a little; a sub
     * on G1, E♭1, F1, D1 */
    p = part(12, "VIOLET", 112, MACHINES(M_DKIK, M_DSNR, M_DHAT, M_PUL, M_SID, M_GSIN));
    p->glob[G_VERB] = 100; p->glob[G_DAMP] = 60; p->glob[G_TAPE] = 40; p->glob[G_ADPC] = 10; p->glob[G_SWNG] = 30;
    SET(p, 0, SA, DW4(0), SB, 40, SC, 30, SD, 30, SE, 0, SF, 64, SG, 0, SH, 10, P_HOLD, 6, P_DEC, 36, P_REL, 8,
        P_DIST, 72, P_VOL, 110);
    BEAT(p, 0, "x.........x.....|x.......x.....x.|x.........x.....|x.......x...x...", 31);
    SET(p, 1, SA, DW4(1), SB, 50, SC, 14, SD, 100, SE, 80, SF, 70, SG, 0, SH, 6, P_DEC, 30, P_REL, 12, P_SPU, 80,
        P_VOL, 80);
    BEAT(p, 1, "........X.......|........X.......|........X.......|........X.....x.", 50);
    SET(p, 2, SG, 4, SH, 0, P_DEC, 90, P_BASE, 46, P_VOL, 127, P_LEVL, 101, P_PAN, 70);
    BEAT(p, 2, "x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.xx", 60);
    SET(p, 3, SA, 0, SB, 0, SC, 0, SD, 0, SE, 56, SF, 6, SG, 0, SH, 64, P_ATK, 40, P_HOLD, 127, P_DEC, 127,
        P_REL, 50, P_GATE, 100, P_DTIM, 96, P_DSND, 80, P_DFB, 40, P_SPU, 80, P_VOL, 127, P_LEVL, 111, P_PAN, 70);
    BEAT(p, 3, "x...............|x...............|x...............|x...............", 74, 70, 72, 69);
    lfo(p, 3, 0, SH, 1, 4, 44, 67, 1);                                       /* a vibrato on TUNE */
    SET(p, 4, SA, 0, SB, 0, SC, 0, SD, SID5(2), SE, SIDM(0), SF, 0, SG, 0, SH, 64, P_ATK, 40, P_HOLD, 127, P_DEC, 127,
        P_REL, 50, P_GATE, 100, P_WDTH, 60, P_DTIM, 64, P_DSND, 50, P_DFB, 40, P_SPU, 80, P_VOL, 54, P_PAN, 56);
    BEAT(p, 4, "x...............|x...............|x...............|x...............", 70, 67, 69, 65);
    SET(p, 5, SA, 0, SB, 0, SC, 0, SD, 0, SE, 40, SF, 6, SG, 10, P_HOLD, 40, P_DEC, 50, P_REL, 16, P_GATE, 60,
        P_DIST, 70, P_VOL, 92);
    BEAT(p, 5, "x.......x.......|x.......x.......|x.......x.......|x.......x.......", 31, 31, 27, 27, 29, 29, 26, 26);
    trim(p, 57);

    /* A14 DUSKLIT, after random_meteor's lo-bit night: C minor at 100, swung. A kick and a crushed snare; 8-bit noise
     * hats; a dark lead on one of the owner's waves (odd-harmonic, a reed's), C4 E♭4 G4 F4 E♭4 D4, in a dotted echo; a
     * chorused chord, Cm7 to A♭maj7; a soft SID bass under it, C2 A♭1 B♭1 G1, sliding */
    p = part(13, "DUSKLIT", 100, MACHINES(M_DKIK, M_DSNR, M_GPRC, M_WAV, M_ENS, M_SID));
    p->glob[G_VERB] = 80; p->glob[G_DAMP] = 70; p->glob[G_ADPC] = 40; p->glob[G_HISS] = 24; p->glob[G_TAPE] = 20;
    p->glob[G_SWNG] = 40;
    SET(p, 0, SA, DW4(0), SB, 44, SC, 26, SD, 24, SE, 10, SF, 76, SG, 0, SH, 16, P_HOLD, 6, P_DEC, 36, P_REL, 8,
        P_DIST, 74, P_VOL, 110);
    BEAT(p, 0, "x......x..x.....|x......x...x..x.|x......x..x.....|x......x....x...", 31);
    SET(p, 1, SA, DW4(1), SB, 46, SC, 12, SD, 110, SE, 76, SF, 74, SG, 0, SH, 6, P_DEC, 24, P_REL, 8, P_BITS, 40,
        P_SRR, 30, P_SPU, 40, P_VOL, 86);
    BEAT(p, 1, "....X.......X...|....X.......X..x|....X.......X...|....X.......X...", 52);
    SET(p, 2, SA, 110, SB, 0, SC, 0, SD, 0, SE, 0, SF, 0, SG, 90, P_DEC, 6, P_REL, 4, P_BASE, 46, P_VOL, 82,
        P_PAN, 74);
    BEAT(p, 2, "x.x.x.xxx.x.x.x.|x.x.x.xxx.x.x.xx|x.x.x.xxx.x.x.x.|x.x.x.xxx.x.xxx.", 60);
    SET(p, 3, SA, OS(9), SB, 64, SC, 127, SD, 0, SE, 40, SF, 64, SG, 0, SH, 0, P_WBNK, 127, P_ATK, 10, P_HOLD, 24,
        P_DEC, 44, P_REL, 30, P_GATE, 30, P_WDTH, 40, P_DTIM, 48, P_DSND, 92, P_DFB, 56, P_DWID, 70, P_SPU, 60,
        P_VOL, 63, P_PAN, 60);
    BEAT(p, 3, "x.....x...x.....|....x.......x...|x.....x...x.....|....x...........", 60, 63, 67, 65, 63, 62, 60, 63, 67,
         65, 62);
    SET(p, 4, SA, PCH(3), SB, PCH(7), SC, PCH(10), SD, 30, SE, 64, SF, 90, SG, 100, P_ATK, 50, P_HOLD, 127,
        P_DEC, 127, P_REL, 60, P_GATE, 120, P_WDTH, 34, P_SPU, 60, P_VOL, 127, P_LEVL, 114);
    BEAT(p, 4, "x...............................|x...............................", 48, 44);
    chords(p, 4, 32, 64, 4, 7, 11);                                         /* A♭maj7 */
    SET(p, 5, SA, 0, SB, 0, SC, 0, SD, SID5(0), SE, SIDM(0), SF, 0, SG, 0, SH, 64, P_HOLD, 30, P_DEC, 40, P_REL, 14,
        P_GATE, 40, P_PORT, 20, P_WDTH, 40, P_VOL, 122);
    BEAT(p, 5, "x.......x...s...|x.......x.......|x.......x...s...|x.......x.......", 36, 36, 39, 32, 32, 34, 34, 37, 31,
         31);
    trim(p, 56);

    /* A15 FLUTE, after LØSTLVLS's HI sounds: D dorian at 96, a swung hip-hop beat. A boom-bap kick and snare, hats in
     * swung eighths with ghosts; a flute on GND-SIN (a breath of noise, its second harmonic, a vibrato) singing D5 C5 A4
     * F4 G4 in a long echo; strings, Dm7 to G7, slow to swell; a sub on D2, G1, A1 and C2 */
    p = part(14, "FLUTE", 96, MACHINES(M_DKIK, M_DSNR, M_DHAT, M_GSIN, M_ENS, M_GSIN));
    p->glob[G_VERB] = 70; p->glob[G_DAMP] = 70; p->glob[G_TAPE] = 50; p->glob[G_HISS] = 40; p->glob[G_SWNG] = 56;
    SET(p, 0, SA, DW4(0), SB, 36, SC, 30, SD, 30, SE, 0, SF, 64, SG, 0, SH, 20, P_HOLD, 6, P_DEC, 36, P_REL, 8,
        P_DIST, 74, P_VOL, 110);
    BEAT(p, 0, "x.....x...x.....|x.......x.x.....|x.....x...x.....|x.......x....x..", 33);
    SET(p, 1, SA, DW4(1), SB, 50, SC, 14, SD, 110, SE, 70, SF, 76, SG, 0, SH, 6, P_DEC, 26, P_REL, 8, P_SPU, 40,
        P_VOL, 80);
    BEAT(p, 1, "....X.......X...|....X.......X...|....X.......X...|....X.......X..x", 52);
    SET(p, 2, SG, 4, SH, 0, P_DEC, 90, P_BASE, 46, P_VOL, 127, P_LEVL, 104, P_PAN, 68);
    BEAT(p, 2, "x.x.x.xxx.x.x.x.|x.x.x.xxx.x.x.x.|x.x.x.xxx.x.x.x.|x.x.x.xxx.x.x.xx", 60);
    SET(p, 3, SA, 0, SB, 0, SC, 30, SD, 0, SE, 40, SF, 0, SG, 30, SH, 64, P_ATK, 30, P_HOLD, 127, P_DEC, 127,
        P_REL, 30, P_GATE, 24, P_DTIM, 96, P_DSND, 92, P_DFB, 50, P_DWID, 80, P_SPU, 60, P_VOL, 67, P_PAN, 60);
    BEAT(p, 3, "x.....x...x.....|x...............|x.....x...x...x.|x...............", 74, 72, 69, 65, 67, 72, 69, 67,
         65, 62);
    lfo(p, 3, 0, SH, 1, 4, 56, 68, 1);                                       /* the flute's vibrato, on TUNE */
    SET(p, 4, SA, PCH(3), SB, PCH(7), SC, PCH(10), SD, 0, SE, 64, SF, 100, SG, 110, P_ATK, 70, P_HOLD, 127,
        P_DEC, 127, P_REL, 80, P_GATE, 120, P_WDTH, 40, P_SPU, 70, P_VOL, 120, P_LEVL, 100);
    BEAT(p, 4, "x...............................|x...............................", 50, 43);
    chords(p, 4, 32, 64, 4, 7, 10);                                         /* G7 */
    SET(p, 5, SA, 0, SB, 50, SC, 0, SD, 0, SE, 40, SF, 6, SG, 10, P_HOLD, 30, P_DEC, 44, P_REL, 14, P_GATE, 40,
        P_DIST, 70, P_VOL, 97);
    BEAT(p, 5, "x.....x...x.....|x.......x.......|x.....x...x.....|x.......x.......", 38, 38, 36, 31, 31, 38, 38, 36,
         33, 33);
    trim(p, 54);

    /* A16 EMBERS, the set's last loop: E minor at 116. A soft kick thinning out by its conditions; ticks at 3/4x by
     * chance; FM+PAR bells, B4 G4 E4 F#4, deep in a long dark reverb; an Em7 drone of strings; a sub on E1 now and
     * then; a soft motif on one of the owner's near-sine waves, B3 E4 G4, in an echo that won't stop */
    p = part(15, "EMBERS", 116, MACHINES(M_DKIK, M_DHAT, M_FMP, M_ENS, M_GSIN, M_WAV));
    p->glob[G_VERB] = 120; p->glob[G_DAMP] = 90; p->glob[G_HISS] = 20; p->glob[G_TAPE] = 30;
    p->glob[G_SPD1 + 1] = SPEED(3);
    SET(p, 0, SA, DW4(0), SB, 30, SC, 30, SD, 30, SE, 0, SF, 64, SG, 0, SH, 6, P_HOLD, 6, P_DEC, 36, P_REL, 8,
        P_DIST, 70, P_VOL, 108);
    BEAT(p, 0, "x...x...x...x...|x...h...x...H...|x...x...h...x...|x...h...x...q...", 28);
    SET(p, 1, SA, 72, SB, 80, SC, 60, SD, 127, SE, 100, SF, DW5(0), SG, 4, SH, 0, P_DEC, 90, P_BASE, 52, P_SPU, 50,
        P_VOL, 113, P_PAN, 76);
    BEAT(p, 1, "x.?.x?..x.?.?x..|x.?.x?..x.?.?x..|x.?.x?..x.?.?x..|x.?.x?..x.?.?x..", 60);
    SET(p, 2, SA, FR24(19), SB, 104, SC, FR24(23), SD, 100, SE, FR24(21), SF, 96, SG, 100, SH, 64, P_DEC, 50,
        P_REL, 30, P_SPU, 110, P_DTIM, 96, P_DSND, 70, P_DFB, 50, P_VOL, 12, P_PAN, 44);
    BEAT(p, 2, "x.......x.......|....x...........|x.......x.......|....x...........", 71, 67, 64, 66);
    SET(p, 3, SA, PCH(3), SB, PCH(7), SC, PCH(10), SD, 40, SE, 64, SF, 90, SG, 110, P_ATK, 90, P_HOLD, 127,
        P_DEC, 127, P_REL, 100, P_GATE, 127, P_WDTH, 30, P_SPU, 100, P_VOL, 12);
    BEAT(p, 3, "x...............................|x...............................", 52);
    SET(p, 4, SA, 0, SB, 50, SC, 0, SD, 0, SE, 40, SF, 6, SG, 10, P_HOLD, 40, P_DEC, 50, P_REL, 20, P_GATE, 60,
        P_DIST, 70, P_VOL, 105, P_LEVL, 100);
    BEAT(p, 4, "x...............|........x.......|x...............|................", 40, 40, 43);
    SET(p, 5, SA, OS(1), SB, 64, SC, 127, SD, 0, SE, 0, SF, 64, SG, 0, SH, 0, P_WBNK, 127, P_ATK, 6, P_HOLD, 10,
        P_DEC, 40, P_REL, 30, P_GATE, 20, P_DTIM, 48, P_DSND, 100, P_DFB, 90, P_DWID, 90, P_SPU, 80, P_VOL, 2,
        P_PAN, 64);
    BEAT(p, 5, "x..x..x.........|................|................|................", 59, 64, 67);
    trim(p, 51);
}
