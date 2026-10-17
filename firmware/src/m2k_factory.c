/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's factory patterns: fifteen sound designs to show what six mono machines can do, and one empty one.
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

/* the first demos were voiced on MONO2K's first envelope curve and its quiet reverb: their ATK HOLD DEC REL move
 * onto the Monomachine's times as they are set (amp_v1_val), and their reverb sends come down a little, so the
 * space they were written with is now heard. The later demos (pat_now) are written on today's scales. */
static int fac_v1;
static uint8_t fac_val(int prm, int v)
{
    if (!fac_v1) return (uint8_t)v;
    if (prm == P_SPU) return (uint8_t)(v * 3 / 4);
    return amp_v1_val(prm, v);
}

static void lk(m2k_pattern_t *p, int t, int s, int prm, int v)
{
    for (int i = 0; i < M2K_LOCKS; i++)
        if (p->lock[i].trk == 0xFF) { p->lock[i] = (m2k_lock_t){(uint8_t)t, (uint8_t)s, (uint8_t)prm, fac_val(prm, v)}; return; }
}

static void set(m2k_pattern_t *p, int t, const uint8_t *pv, int n)   /* (param, value) pairs */
{
    for (int i = 0; i + 1 < n; i += 2) p->kit[t][pv[i]] = fac_val(pv[i], pv[i + 1]);
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

/* the demos are written on the chain they were voiced on (MONO2K's before version 5) and converted as old patterns are:
 * this is that chain's blank kit */
static void kit_v4(m2k_pattern_t *p, int t, int m)
{
    static const uint8_t D[] = {0, 0, 64, 64, 0, 100, 64, 0, 0, 127, 0, 0, 0, 64, 64, 64, 64, 64, 0, 12, 0, 50, 0, 110};
    kit_default(p, t, m);
    m_cpy(&p->kit[t][P_ATK], D, sizeof D);
    p->kit[t][P_LEVL] = 110;
}

static m2k_pattern_t *pat_new(int i, const char *name, int bpm, const uint8_t *machines)
{
    m2k_pattern_t *p = &pat[i];
    fac_v1 = 1;
    pat_init(p, name);
    for (int t = 0; t < M2K_TRACKS; t++) {
        kit_v4(p, t, machines[t]);
        static const uint8_t AMP0[4] = {0, 0, 60, 30};          /* the blank kit the demos were voiced from */
        for (int k = 0; k < 4; k++) p->kit[t][P_ATK + k] = amp_v1_val(P_ATK + k, AMP0[k]);
    }
    p->glob[G_TMPO] = (uint8_t)(bpm - 30);
    p->glob[G_VERB] = 40;
    return p;
}
#define MACHINES(...) (const uint8_t[]){__VA_ARGS__}

/* a demo written on today's scales: the blank kit as it is, every track's envelope given */
static m2k_pattern_t *pat_now(int i, const char *name, int bpm, const uint8_t *machines)
{
    m2k_pattern_t *p = &pat[i];
    fac_v1 = 0;
    pat_init(p, name);
    for (int t = 0; t < M2K_TRACKS; t++) kit_v4(p, t, machines[t]);
    p->glob[G_TMPO] = (uint8_t)(bpm - 30);
    p->glob[G_VERB] = 60;
    return p;
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
#define SIDW(i) enc(i, 8)              /* 0 TRI 1 SAW 2 PUL 3 NOI 4 T+S 5 T+P 6 S+P 7 ALL */
#define MODE4(i) enc(i, 4)             /* SID: OFF SYNC RING S+R; FM+STAT: STCK PAR 2CAR RING */
#define RATIO(i) enc(i, 32)            /* 3: 1.0, 6: 2.0, 8: 2.76, 9: 3.0, 10: 3.5, 11: 4.0, 13: 5.0, 15: 6.0, 17: 7.07, 26: 15.7 */
#define SEMI(n) enc(n, 25)             /* ENS intervals, 0..24 semitones */
#define WAVE(i) ((uint8_t)((i) * 127 / 31))   /* DIGI-WAV: 5 BELL 6 GLAS 7 PLST 8 LTEX 19 MTL1 20 MTL2 25 BITS 26 8BIT 30 CRSH */
#define VOWEL(i) ((uint8_t)((i) * 127 / 7))   /* A AE E I Y O U ER */
#define VOICES(n) enc((n) - 1, 7)
#define GSIZE(i) enc(i, 5)             /* 1/64 1/32 1/16 1/8 1/4 */

static void factory(void)
{
    m2k_pattern_t *p;

    /* A01 LATEX: rubber kick, a plastic squeak, a bass that hiccups (after SOPHIE's bounce) */
    p = pat_new(0, "LATEX", 122, MACHINES(M_GSIN, M_FMD, M_GNOI, M_SID, M_VOX, M_ENS));
    p->glob[G_TAPE] = 8; p->glob[G_GLCH] = 14; p->glob[G_GSIZ] = GSIZE(2); p->glob[G_VERB] = 72; p->glob[G_HISS] = 5; p->glob[G_DRIV] = 18;
    SET(p, 0, SE, 78, SF, 38, SG, 50, SB, 12, P_DEC, 78, P_DIST, 34, P_VOL, 112);
    BEAT(p, 0, "X.....x...x..?.2", 36);
    lk(p, 0, 13, SE, 110);
    SET(p, 1, SA, RATIO(10), SB, 80, SC, 45, SD, RATIO(9), SE, 40, SF, 35, SG, 20, SH, 96,
        P_DEC, 62, P_LPQ, 76, P_WDTH, 70, P_FDEC, 52, P_WOF, 84, P_VOL, 96, P_DSND, 30, P_DTIM, 6, P_DFB, 60, P_SPU, 30, P_PAN, 52);
    BEAT(p, 1, "..x.xs.X.Q..X..h", 67, 72, 75, 79, 72, 70, 67);
    lk(p, 1, 7, SB, 127); lk(p, 1, 12, SH, 20); lk(p, 1, 15, SA, RATIO(17));
    SET(p, 2, SA, 104, SC, 70, SD, 40, P_DEC, 54, P_BASE, 86, P_VOL, 84, P_PAN, 80);
    BEAT(p, 2, "x.X.x.x?x.X.x.4?", 76);
    lk(p, 2, 6, P_DEC, 70); lk(p, 2, 14, P_DEC, 74);
    SET(p, 3, SA, SIDW(2), SD, SIDW(1), SE, 96, SG, MODE4(1), SH, 40, P_DEC, 74, P_WDTH, 58, P_LPQ, 54,
        P_FDEC, 54, P_WOF, 84, P_VOL, 100, P_PORT, 20);
    BEAT(p, 3, "x..x....X..s..x.", 36, 36, 39, 31, 36);
    SET(p, 4, P_ATK, 4, P_HOLD, 20, P_DEC, 74, P_DSND, 64, P_DTIM, 12, P_DFB, 72, P_SPU, 50, P_VOL, 92, P_PAN, 40);
    BEAT(p, 4, "....x.......X...", 67, 70);
    lk(p, 4, 12, SA, VOWEL(5));
    SET(p, 5, SA, SEMI(3), SB, SEMI(7), SC, SEMI(10), P_DEC, 78, P_WDTH, 82, P_LPQ, 30, P_FDEC, 56, P_WOF, 80,
        P_SPU, 76, P_VOL, 78, P_PAN, 76);
    BEAT(p, 5, "x.........x.....", 60, 63);
    lfo(p, 1, 0, SB, 1, 0, 40, 90, 0);
    MOD(p, 5, 0, 36, 80, 56);                  /* the chords, chorused */

    /* A02 PVC: hard plastic and metal */
    p = pat_new(1, "PVC", 132, MACHINES(M_FMS, M_GSIN, M_WAV, M_GNOI, M_SAW, M_FMD));
    p->glob[G_GLCH] = 40; p->glob[G_GSIZ] = GSIZE(2); p->glob[G_ADPC] = 30; p->glob[G_VERB] = 50; p->glob[G_DRIV] = 40;
    SET(p, 0, SA, RATIO(4), SB, 100, SC, 50, SD, RATIO(26), SE, 70, SF, 40, SH, MODE4(0), P_DEC, 70, P_VOL, 100, P_PAN, 54);
    BEAT(p, 0, "X..x..x..x..x..x", 60, 65, 70);
    lk(p, 0, 3, SD, RATIO(29)); lk(p, 0, 9, SB, 127); lk(p, 0, 12, P_PTCH, 76);
    SET(p, 1, SB, 40, SC, 70, SE, 60, SF, 52, P_DEC, 86, P_DIST, 60, P_VOL, 108);
    BEAT(p, 1, "X......x..s.....", 33, 33, 45);
    SET(p, 2, SA, WAVE(5), SB, 96, SC, 60, SD, 40, P_DEC, 76, P_DSND, 60, P_DTIM, 3, P_DFB, 96, P_VOL, 80);
    BEAT(p, 2, "..x...x....X....", 84, 91, 86);
    SET(p, 3, SA, 70, SG, 60, P_DEC, 70, P_BASE, 50, P_DIST, 30, P_VOL, 90);
    BEAT(p, 3, "....X.......x..2", 60);
    lk(p, 3, 15, SB, 90);
    SET(p, 4, SA, 100, SB, 24, SC, 80, SF, 20, P_DEC, 70, P_WDTH, 64, P_LPQ, 90, P_FDEC, 46, P_WOF, 86, P_BITS, 50, P_VOL, 127, P_SPU, 30);
    BEAT(p, 4, ".x...x..s....x..", 72, 75, 79, 77);
    lk(p, 4, 5, P_SRR, 80); lk(p, 4, 13, P_XOR, 70);
    SET(p, 5, SH, 127, SC, 70, SB, 110, P_DEC, 70, P_VOL, 70, P_PAN, 90, P_SPU, 60);
    BEAT(p, 5, "..............x.", 84);
    lk(p, 5, 14, SH, 0);
    MOD(p, 4, 1, 20, 90, 50);                  /* the saw, flanged */

    /* A03 SPU RM: slow, wide, wobbly */
    p = pat_new(2, "SPU RM", 88, MACHINES(M_ENS, M_WAV, M_VOX, M_GSIN, M_GNOI, M_GSIN));
    p->glob[G_TAPE] = 60; p->glob[G_ADPC] = 70; p->glob[G_VERB] = 112; p->glob[G_DAMP] = 70; p->glob[G_HISS] = 26; p->glob[G_LEN] = 32;
    SET(p, 0, SA, SEMI(4), SB, SEMI(7), SC, SEMI(11), SD, 50, P_ATK, 70, P_HOLD, 127, P_DEC, 127, P_REL, 86,
        P_WDTH, 60, P_SPU, 90, P_VOL, 80, P_GATE, 100);
    BEAT(p, 0, "x...............|x...............", 57, 53);
    lk(p, 0, 16, SB, SEMI(9));
    SET(p, 1, SA, WAVE(8), SB, 90, SC, 90, SH, 80, P_DEC, 78, P_DSND, 80, P_DTIM, 24, P_DFB, 80, P_VOL, 70);
    BEAT(p, 1, "..x....x....x...|.x....x....Q....", 76, 79, 81, 76, 84, 79);
    SET(p, 2, SA, VOWEL(5), SB, 30, SC, 100, P_ATK, 50, P_HOLD, 127, P_DEC, 127, P_REL, 70, P_SPU, 100, P_VOL, 76, P_GATE, 60);
    BEAT(p, 2, "........x.......|........x.......", 64, 62);
    SET(p, 3, SC, 60, SE, 30, SF, 50, P_DEC, 94, P_VOL, 100);
    BEAT(p, 3, "x...............|....x...........", 33, 29);
    SET(p, 4, SA, 30, P_DEC, 66, P_VOL, 84, P_BITS, 70, P_SPU, 40);
    BEAT(p, 4, "....x.......x...|....x.......x..q", 60);
    SET(p, 5, SE, 0, SB, 20, P_ATK, 10, P_DEC, 72, P_VOL, 84, P_DSND, 90, P_DTIM, 16, P_DFB, 90, P_PAN, 90);
    BEAT(p, 5, "......e.....q...|..........e.....", 96, 91);
    MOD(p, 0, 0, 24, 100, 70);                 /* the pad, wide */
    MOD(p, 1, 2, 18, 100, 64);                 /* the latex, phased */

    /* A04 ELECTRO: an 808 that hums, a clap that flams, a squelching bass, a robot, a laser */
    p = pat_new(3, "ELECTRO", 128, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_SID, M_VOX, M_FMD));
    p->glob[G_SWNG] = 10; p->glob[G_VERB] = 40; p->glob[G_DAMP] = 60; p->glob[G_DRIV] = 24; p->glob[G_GLCH] = 6;
    SET(p, 0, SE, 70, SF, 46, SG, 30, P_DEC, 84, P_DIST, 20, P_VOL, 114);
    BEAT(p, 0, "X.....x...x...?.", 33);
    SET(p, 1, SA, 74, SC, 20, P_DEC, 70, P_BASE, 46, P_VOL, 98);
    BEAT(p, 1, "....X.......3...", 60);
    SET(p, 2, SA, 108, SC, 80, SD, 30, P_DEC, 56, P_BASE, 90, P_VOL, 80, P_PAN, 76);
    BEAT(p, 2, "x.x?x.x.x.xqx.xX", 80);
    lk(p, 2, 15, P_DEC, 76);
    SET(p, 3, SA, SIDW(2), SB, 70, SC, 30, SD, SIDW(1), SE, 64, SF, 70, SG, MODE4(1), SH, 50,
        P_HOLD, 20, P_DEC, 76, P_WDTH, 64, P_LPQ, 70, P_FDEC, 56, P_WOF, 84, P_VOL, 96);
    BEAT(p, 3, "x..x..x.x..x.s..", 29, 29, 32, 29, 41, 36, 34);
    SET(p, 4, SA, VOWEL(2), SB, 40, SC, 60, SE, 5, SF, 80, SG, 90, SH, 0, P_ATK, 2, P_HOLD, 40, P_DEC, 76,
        P_DSND, 40, P_DTIM, 12, P_DFB, 50, P_VOL, 88, P_PAN, 52);
    BEAT(p, 4, "..x.....x...x...", 60, 63, 67);
    lk(p, 4, 8, SA, VOWEL(5));
    SET(p, 5, SA, RATIO(15), SB, 100, SC, 50, SD, RATIO(3), SE, 20, SF, 40, SG, 30, SH, 120,
        P_DEC, 70, P_DSND, 50, P_DTIM, 6, P_DFB, 70, P_PAN, 40, P_VOL, 76);
    BEAT(p, 5, "..........h.....", 72);

    /* A05 GLITCH: ratchets, chances, digital noise; the bus repeats a third of the time */
    p = pat_new(4, "GLITCH", 140, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_FMS, M_WAV, M_VOX));
    p->glob[G_GLCH] = 90; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_ADPC] = 40; p->glob[G_VERB] = 30; p->glob[G_DRIV] = 40;
    SET(p, 0, SE, 90, SF, 34, SG, 70, SB, 30, P_DEC, 74, P_DIST, 40, P_VOL, 112);
    BEAT(p, 0, "x..x..?.x.3..x.?", 36);
    SET(p, 1, SA, 64, SB, 90, SG, 70, P_DEC, 70, P_BITS, 60, P_XOR, 30, P_VOL, 90);
    BEAT(p, 1, "....x..2....x.4q", 60);
    SET(p, 2, SA, 120, P_DEC, 50, P_BASE, 100, P_VOL, 70, P_PAN, 84);
    BEAT(p, 2, "x?x?x?x?x?x?x?Qq", 84);
    SET(p, 3, SA, RATIO(21), SB, 80, SC, 40, SD, RATIO(9), SE, 60, SF, 30, SG, 40, SH, MODE4(3), P_DEC, 62, P_VOL, 76, P_PAN, 90);
    BEAT(p, 3, "x.?x..x?.x?.x..x", 84, 91, 79, 96, 88, 81);
    lk(p, 3, 3, SA, RATIO(26)); lk(p, 3, 9, P_SRR, 90); lk(p, 3, 12, SA, RATIO(13)); lk(p, 3, 15, P_XOR, 90);
    SET(p, 4, SA, WAVE(25), SB, 90, SC, 50, SD, 60, SE, 60, SH, 80, P_DEC, 74, P_WDTH, 80, P_LPQ, 40, P_VOL, 96);
    BEAT(p, 4, "x...x.s.x..x.s..", 36, 36, 39, 34, 36, 43);
    SET(p, 5, SA, VOWEL(0), SB, 127, SC, 50, SE, 30, SF, 90, P_DEC, 66, P_VOL, 80, P_PAN, 40);
    BEAT(p, 5, "......3.......2.", 72, 67);

    /* A06 CHROME: inharmonic bells, folded kicks, scraped metal */
    p = pat_new(5, "CHROME", 110, MACHINES(M_GSIN, M_FMS, M_GNOI, M_WAV, M_FMD, M_GNOI));
    p->glob[G_VERB] = 80; p->glob[G_DAMP] = 30; p->glob[G_DRIV] = 30; p->glob[G_GLCH] = 10; p->glob[G_GSIZ] = GSIZE(1);
    SET(p, 0, SE, 64, SF, 44, SB, 70, SH, 40, P_DEC, 80, P_DIST, 30, P_VOL, 110);
    BEAT(p, 0, "x.......x.x.....", 34);
    SET(p, 1, SA, RATIO(8), SB, 70, SC, 76, SD, RATIO(26), SE, 50, SF, 50, SG, 10, P_DEC, 86,
        P_SPU, 60, P_DSND, 40, P_DTIM, 12, P_DFB, 60, P_VOL, 84);
    BEAT(p, 1, "x...x..x...x.x..", 72, 79, 75, 84, 77);
    SET(p, 2, SA, 100, SC, 127, SD, 90, P_DEC, 60, P_BASE, 80, P_VOL, 78, P_PAN, 82);
    BEAT(p, 2, "x.xx.x.xx.x.x.xx", 79);
    lk(p, 2, 7, P_DEC, 74); lk(p, 2, 15, P_DEC, 76);
    SET(p, 3, SA, WAVE(20), SB, 100, SC, 70, SD, 40, P_DEC, 76, P_VOL, 80);
    BEAT(p, 3, "..x...x...x...x.", 48, 51, 55, 51);
    SET(p, 4, SA, RATIO(17), SB, 110, SC, 70, SG, 60, SH, 30, P_DEC, 74, P_BASE, 50, P_PAN, 30, P_VOL, 90);
    BEAT(p, 4, "......x.......x.", 67, 62);
    SET(p, 5, SA, 80, SC, 60, SG, 30, P_DEC, 72, P_BASE, 52, P_VOL, 92);
    BEAT(p, 5, "....x.......x...", 60);
    MOD(p, 1, 1, 14, 70, 44);                  /* the bells, a slow flange */

    /* A07 ACID: a SID line through a screaming filter, slides and accents */
    p = pat_new(6, "ACID", 132, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_SID, M_ENS, M_GNOI));
    p->glob[G_ACNT] = 90; p->glob[G_DRIV] = 50; p->glob[G_VERB] = 30;
    SET(p, 0, SE, 72, SF, 42, SG, 40, P_DEC, 80, P_VOL, 114);
    BEAT(p, 0, "x...x...x...x...", 36);
    SET(p, 1, SA, 76, P_DEC, 66, P_BASE, 50, P_VOL, 90);
    BEAT(p, 1, "....x.......3...", 60);
    SET(p, 2, SA, 112, P_DEC, 54, P_BASE, 96, P_VOL, 78);
    BEAT(p, 2, "..x...x...x...x.", 80);
    SET(p, 3, SA, SIDW(1), SD, SIDW(1), SE, 64, SF, 66, SG, MODE4(0), SH, 0, P_HOLD, 127, P_DEC, 127, P_REL, 30,
        P_WDTH, 56, P_LPQ, 96, P_FDEC, 58, P_WOF, 88, P_DIST, 30, P_VOL, 52, P_GATE, 13);
    BEAT(p, 3, "xXsx.xSx.xsXx.sx", 36, 48, 46, 36, 39, 43, 41, 36, 48, 44, 36, 39);
    lfo(p, 3, 0, P_WDTH, 0, 0, 12, 84, 0);
    SET(p, 4, SA, SEMI(3), SB, SEMI(7), SC, SEMI(10), P_DEC, 78, P_WDTH, 80, P_SPU, 50, P_DSND, 60, P_DTIM, 12, P_DFB, 60, P_VOL, 76);
    BEAT(p, 4, "......h.........", 60);
    SET(p, 5, SA, 118, P_DEC, 62, P_BASE, 100, P_VOL, 66, P_PAN, 84);
    BEAT(p, 5, "x?x?x?x?x?x?x?x?", 84);

    /* A08 2STEP: swung garage, a skipping kick, a chopped voice, organ chords */
    p = pat_new(7, "2STEP", 134, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_GSIN, M_VOX, M_ENS));
    p->glob[G_SWNG] = 60; p->glob[G_VERB] = 50; p->glob[G_DAMP] = 60; p->glob[G_TAPE] = 10;
    SET(p, 0, SE, 60, SF, 44, SG, 30, P_DEC, 80, P_VOL, 112);
    BEAT(p, 0, "x.........x..q..", 36);
    SET(p, 1, SA, 70, P_DEC, 70, P_BASE, 48, P_VOL, 92);
    BEAT(p, 1, "....x.......x...", 60);
    SET(p, 2, SA, 120, P_DEC, 54, P_BASE, 90, P_VOL, 88, P_PAN, 80);
    BEAT(p, 2, "x.xQx.xxx.x?x.xx", 84);
    SET(p, 3, SE, 0, SC, 60, P_HOLD, 40, P_DEC, 92, P_VOL, 104, P_PORT, 30);
    BEAT(p, 3, "x.....x...x.s...", 38, 38, 41, 33);
    SET(p, 4, SA, VOWEL(1), SB, 90, SC, 50, SD, 80, SE, 20, SF, 80, SG, 30, SH, 30, P_DEC, 70, P_VOL, 86, P_PAN, 48);
    BEAT(p, 4, "..x....x..x....3", 74, 72, 69, 74);
    SET(p, 5, SA, SEMI(3), SB, SEMI(7), SC, SEMI(14), P_DEC, 74, P_WDTH, 90, P_DSND, 50, P_DTIM, 12, P_DFB, 50, P_VOL, 76);
    BEAT(p, 5, "...x.....x..x...", 62, 60, 62);
    MOD(p, 5, 0, 44, 64, 60);                  /* the organ, chorused */
    SET(p, 5, P_VOL, 84);

    /* A09 GROOVE: rolling techno: a round kick, a flammed clap, shuffled hats through a phaser, a bass on the
     * off-sixteenths, FM congas, a chord stab into the delay */
    p = pat_now(8, "GROOVE", 126, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_SID, M_FMS, M_ENS));
    p->glob[G_SWNG] = 24; p->glob[G_VERB] = 64; p->glob[G_DAMP] = 50; p->glob[G_DRIV] = 22; p->glob[G_TAPE] = 6;
    p->glob[G_GLCH] = 6; p->glob[G_GSIZ] = GSIZE(2);
    SET(p, 0, SE, 66, SF, 38, SG, 36, SC, 30, P_DEC, 32, P_REL, 10, P_DIST, 18, P_VOL, 114);
    BEAT(p, 0, "X...x...x...x...", 36);
    SET(p, 1, SA, 72, SC, 24, P_DEC, 22, P_REL, 14, P_BASE, 40, P_SPU, 44, P_VOL, 92, P_PAN, 58);
    BEAT(p, 1, "....x.......2...", 60);
    SET(p, 2, SA, 112, SG, 10, P_DEC, 4, P_REL, 4, P_BASE, 84, P_VOL, 90, P_PAN, 74);
    MOD(p, 2, 2, 30, 90, 70);
    BEAT(p, 2, "x.X.x.Xxx.X.x.X?", 84);
    lk(p, 2, 2, P_DEC, 18); lk(p, 2, 6, P_DEC, 18); lk(p, 2, 10, P_DEC, 22); lk(p, 2, 14, P_DEC, 18);
    SET(p, 3, SA, SIDW(1), SB, 64, SC, 30, SD, SIDW(2), SE, 32, SH, 50, P_DEC, 16, P_REL, 8, P_WDTH, 36, P_LPQ, 64,
        P_FDEC, 46, P_WOF, 92, P_DIST, 20, P_VOL, 96);
    BEAT(p, 3, ".xx..xx..xS..x.x", 33, 45, 33, 33, 36, 33, 45, 33);
    lfo(p, 3, 0, P_WDTH, 1, 0, 32, 84, 0);
    SET(p, 4, SA, RATIO(3), SB, 70, SC, 30, SD, RATIO(6), SE, 30, SF, 20, SH, MODE4(0), P_DEC, 14, P_REL, 8,
        P_DSND, 30, P_DTIM, 12, P_DFB, 40, P_VOL, 78, P_PAN, 40);
    BEAT(p, 4, "..x..x....x..xx.", 64, 67, 60, 64, 69);
    SET(p, 5, SA, SEMI(3), SB, SEMI(7), SC, SEMI(10), SD, 40, SE, 40, P_DEC, 24, P_REL, 20, P_WDTH, 50, P_LPQ, 30,
        P_DSND, 60, P_DTIM, 12, P_DFB, 70, P_SPU, 40, P_VOL, 70, P_PAN, 82);
    MOD(p, 5, 0, 40, 70, 60);
    BEAT(p, 5, "......x.......x.", 57, 55);

    /* A10 HYPRPOP: a hard bouncy kick, plastic bass, major chords, chipmunk squeaks */
    p = pat_new(9, "HYPRPOP", 150, MACHINES(M_GSIN, M_FMD, M_GNOI, M_ENS, M_FMS, M_VOX));
    p->glob[G_VERB] = 60; p->glob[G_DAMP] = 40; p->glob[G_DRIV] = 30; p->glob[G_GLCH] = 10; p->glob[G_GSIZ] = GSIZE(1);
    SET(p, 0, SE, 100, SF, 30, SG, 60, P_DEC, 74, P_DIST, 50, P_VOL, 114);
    BEAT(p, 0, "x..x..x...x..x..", 36);
    SET(p, 1, SA, RATIO(6), SB, 70, SC, 50, SH, 96, P_DEC, 70, P_VOL, 96, P_PORT, 20);
    BEAT(p, 1, "x.sx..x.sx..x.s.", 36, 43, 48, 46, 36, 43, 41, 39);
    SET(p, 2, SA, 72, P_DEC, 64, P_BASE, 50, P_VOL, 90);
    BEAT(p, 2, "....2.......2...", 60);
    SET(p, 3, SA, SEMI(4), SB, SEMI(7), SC, SEMI(12), SD, 60, P_HOLD, 30, P_DEC, 78, P_WDTH, 100, P_SPU, 60, P_VOL, 74);
    BEAT(p, 3, "x.....x.....x...", 64, 67, 62);
    SET(p, 4, SA, RATIO(11), SB, 60, SC, 40, P_DEC, 74, P_DSND, 70, P_DTIM, 6, P_DFB, 60, P_VOL, 70, P_PAN, 84);
    BEAT(p, 4, "x.xx.x.xx.x.x.xx", 76, 79, 83, 88, 83, 79);
    SET(p, 5, SA, VOWEL(3), SB, 100, SC, 40, SD, 100, P_PTCH, 76, P_DEC, 70, P_VOL, 100, P_PAN, 44);
    BEAT(p, 5, "......x.......x.", 79, 84);
    MOD(p, 3, 0, 40, 70, 50);                  /* the chords, chorused */

    /* A11 DUB: dub techno: a deep soft kick, a rim in the delay, shakers, the chord stab: chorused, filtered by a
     * slow LFO, into a long dark echo and the reverb; a sub under it, blips of FM in the echo, tape and dust */
    p = pat_now(10, "DUB", 118, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_ENS, M_GSIN, M_FMS));
    p->glob[G_LEN] = 32; p->glob[G_TAPE] = 40; p->glob[G_VERB] = 92; p->glob[G_DAMP] = 84; p->glob[G_HISS] = 22;
    p->glob[G_DRIV] = 12; p->glob[G_SWNG] = 12;
    SET(p, 0, SE, 48, SF, 46, SG, 8, SC, 70, P_DEC, 36, P_REL, 14, P_VOL, 110);
    BEAT(p, 0, "x...x...x...x...|x...x...x...x...", 36);
    SET(p, 1, SA, 96, SC, 60, SD, 40, P_DEC, 6, P_REL, 4, P_BASE, 70, P_DSND, 90, P_DTIM, 12, P_DFB, 96,
        P_DBAS, 40, P_DWID, 40, P_SPU, 50, P_VOL, 72, P_PAN, 40);
    BEAT(p, 1, "...x..x....x..x.|...x..x.....x..q", 72);
    SET(p, 2, SA, 116, SG, 0, P_DEC, 4, P_REL, 6, P_BASE, 82, P_VOL, 80, P_PAN, 84);
    MOD(p, 2, 2, 20, 100, 60);
    BEAT(p, 2, "xqx?xqx?xqx?xqx?|xqx?xqx?xqx?xqxx", 84);
    SET(p, 3, SA, SEMI(3), SB, SEMI(7), SC, SEMI(10), SD, 34, SE, 30, P_DEC, 26, P_REL, 22, P_WDTH, 40, P_LPQ, 46,
        P_DSND, 110, P_DTIM, 12, P_DFB, 104, P_DBAS, 30, P_DWID, 50, P_SPU, 70, P_VOL, 80, P_PAN, 60);
    MOD(p, 3, 0, 30, 80, 64);
    BEAT(p, 3, "..x.....x..x....|..x.....x.....x.", 57, 57, 57, 55);
    lfo(p, 3, 0, P_WDTH, 1, 0, 16, 96, 0);
    SET(p, 4, SE, 0, SC, 60, P_HOLD, 6, P_DEC, 40, P_REL, 20, P_VOL, 78);
    BEAT(p, 4, "x.......x.x.....|x.......x.....x.", 33, 33, 36, 33, 31);
    SET(p, 5, SA, RATIO(9), SB, 60, SC, 30, SD, RATIO(3), SE, 40, P_DEC, 12, P_REL, 10, P_DSND, 100, P_DTIM, 24,
        P_DFB, 90, P_SPU, 60, P_VOL, 64, P_PAN, 90);
    BEAT(p, 5, "......e.....q...|.........e....q.", 81, 76, 84);

    /* A12 CHIP: 8-bit drums, SID arps, a square lead with vibrato */
    p = pat_new(11, "CHIP", 150, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_SID, M_SID, M_SID));
    p->glob[G_ADPC] = 30; p->glob[G_VERB] = 20;
    SET(p, 0, SE, 80, SF, 36, P_DEC, 74, P_BITS, 90, P_VOL, 110);
    BEAT(p, 0, "x...x...x...x.x.", 36);
    SET(p, 1, SB, 80, SG, 110, SA, 80, P_DEC, 70, P_VOL, 88);
    BEAT(p, 1, "....x.......x..x", 60);
    SET(p, 2, SG, 127, SA, 120, P_DEC, 54, P_VOL, 70);
    BEAT(p, 2, "x.x.x.x.x.x.x.x.", 84);
    SET(p, 3, SA, SIDW(2), SB, 32, SC, 60, SH, 0, P_HOLD, 10, P_DEC, 62, P_VOL, 70, P_PAN, 84);
    BEAT(p, 3, "xxxxxxxxxxxxxxxx", 72, 76, 79, 84, 72, 76, 79, 84, 70, 74, 77, 82, 70, 74, 77, 82);
    SET(p, 4, SA, SIDW(0), SH, 0, P_HOLD, 30, P_DEC, 74, P_VOL, 100);
    BEAT(p, 4, "x.x.x.x.x.x.x.x.", 36, 36, 43, 43, 41, 41, 39, 39);
    SET(p, 5, SA, SIDW(2), SB, 64, SH, 0, P_HOLD, 40, P_DEC, 80, P_VOL, 84, P_PAN, 44);
    BEAT(p, 5, "x.x.x..xx.x.x...", 72, 74, 76, 79, 77, 76, 74, 72);
    lfo(p, 5, 0, P_PTCH, 1, 2, 64, 66, 0);

    /* A13 HARDGRV: hardgroove techno: a driven kick, a rumble under it from the reverb, a metal ride, hats on a
     * three-against-four, tribal toms, a clap, the bus pushed */
    p = pat_now(12, "HARDGRV", 142, MACHINES(M_GSIN, M_GSIN, M_GNOI, M_GNOI, M_GSIN, M_GNOI));
    p->glob[G_DRIV] = 64; p->glob[G_VERB] = 70; p->glob[G_DAMP] = 96; p->glob[G_GLCH] = 12; p->glob[G_GSIZ] = GSIZE(2);
    p->glob[G_TAPE] = 4; p->glob[G_SWNG] = 8;
    SET(p, 0, SE, 84, SF, 34, SG, 64, SB, 24, SC, 20, P_DEC, 30, P_REL, 8, P_DIST, 70, P_VOL, 112);
    BEAT(p, 0, "X...X...X...X..q", 34);
    SET(p, 1, SE, 30, SF, 40, SB, 60, P_DEC, 36, P_REL, 20, P_WDTH, 28, P_DIST, 60, P_SPU, 110, P_VOL, 58);
    BEAT(p, 1, "..x...x...x...x.", 34);
    SET(p, 2, SA, 96, SC, 120, SD, 70, P_DEC, 30, P_REL, 20, P_BASE, 70, P_VOL, 66, P_PAN, 82);
    BEAT(p, 2, "..x...x...x...X.", 84);
    SET(p, 3, SA, 122, SG, 40, P_DEC, 2, P_REL, 4, P_BASE, 100, P_VOL, 70, P_PAN, 46);
    BEAT(p, 3, "XxxXxxXxxXxxXxXx", 84);
    SET(p, 4, SE, 60, SF, 44, SG, 30, P_DEC, 24, P_REL, 10, P_DSND, 30, P_DTIM, 6, P_DFB, 40, P_VOL, 84, P_PAN, 30);
    BEAT(p, 4, "..x..x.x..x..x.2", 45, 48, 52, 45, 50, 48);
    SET(p, 5, SA, 70, SC, 30, P_DEC, 24, P_REL, 16, P_BASE, 46, P_DIST, 40, P_SPU, 70, P_VOL, 72);
    BEAT(p, 5, "....x.......x.3.", 60);

    /* A14 IDM: broken and kept moving: a kick that never lands the same, a crushed snare, hats in ratchets and
     * chances, an FM bell arp, a scanned digital pad in a chorus, SID blips; the bus repeats and crunches */
    p = pat_now(13, "IDM", 112, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_FMS, M_WAV, M_SID));
    p->glob[G_GLCH] = 56; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_ADPC] = 36; p->glob[G_VERB] = 70; p->glob[G_DAMP] = 40;
    p->glob[G_DRIV] = 30; p->glob[G_LEN] = 32;
    SET(p, 0, SE, 90, SF, 30, SG, 70, SB, 40, P_DEC, 26, P_REL, 6, P_DIST, 30, P_VOL, 110);
    BEAT(p, 0, "x..x.?..x.2..x.?|x....x?.x..3.h..", 36);
    lk(p, 0, 5, SE, 120); lk(p, 0, 13, SE, 50); lk(p, 0, 21, P_PTCH, 52);
    SET(p, 1, SA, 70, SB, 60, SG, 90, P_DEC, 18, P_REL, 8, P_BITS, 50, P_XOR, 24, P_SPU, 40, P_VOL, 86, P_PAN, 56);
    BEAT(p, 1, "....x..3.hx....4|....x.....x..2.k", 60);
    SET(p, 2, SA, 118, SG, 60, P_DEC, 4, P_REL, 4, P_BASE, 78, P_VOL, 88);
    BEAT(p, 2, "x?x.xq2.x?x.3qx.|x?x.4qx.x?2.xqx3", 84);
    lfo(p, 2, 0, P_PAN, 6, 3, 64, 120, 0);
    SET(p, 3, SA, RATIO(13), SB, 70, SC, 40, SD, RATIO(26), SE, 30, SF, 30, SH, MODE4(1), P_DEC, 16, P_REL, 14,
        P_DSND, 50, P_DTIM, 6, P_DFB, 60, P_SPU, 40, P_VOL, 70, P_PAN, 76);
    BEAT(p, 3, "x.xx.x.xx.x.x.xx|x.x.xx.x.xx.x?xx", 72, 79, 76, 84, 81, 72, 88, 79, 76, 91);
    lfo(p, 3, 0, SB, 5, 2, 50, 100, 0);
    SET(p, 4, SA, WAVE(7), SB, 90, SC, 70, SE, 40, SH, 60, P_ATK, 56, P_HOLD, 127, P_DEC, 127, P_REL, 50,
        P_WDTH, 70, P_SPU, 80, P_VOL, 64, P_GATE, 110);
    MOD(p, 4, 0, 20, 110, 80);
    BEAT(p, 4, "x...............|........x.......", 60, 63);
    SET(p, 5, SA, SIDW(2), SB, 30, SC, 60, SH, 0, P_DEC, 10, P_REL, 8, P_DSND, 70, P_DTIM, 9, P_DFB, 70, P_VOL, 60, P_PAN, 30);
    BEAT(p, 5, "..e...q..2..e...|.q....e...2...q.", 96, 103, 91, 108, 98);

    /* A15 DUBSTEP: half time at 140: the kick on one, a big snare on three, a supersaw growl wobbling on the
     * eighths with a sub under it, a vowel that talks, hats that roll */
    p = pat_now(14, "DUBSTEP", 140, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_SAW, M_GSIN, M_VOX));
    p->glob[G_DRIV] = 50; p->glob[G_VERB] = 66; p->glob[G_DAMP] = 60; p->glob[G_GLCH] = 16; p->glob[G_GSIZ] = GSIZE(2);
    p->glob[G_LEN] = 32;
    SET(p, 0, SE, 92, SF, 32, SG, 60, SB, 20, P_DEC, 32, P_REL, 8, P_DIST, 40, P_VOL, 114);
    BEAT(p, 0, "x.........x.....|x.......q.......", 34);
    SET(p, 1, SA, 66, SC, 30, SE, 40, SF, 30, P_DEC, 30, P_REL, 20, P_BASE, 36, P_DIST, 50, P_SPU, 64, P_VOL, 104);
    BEAT(p, 1, "........X.......|........X.......", 60);
    SET(p, 2, SA, 116, P_DEC, 3, P_REL, 4, P_BASE, 96, P_VOL, 64, P_PAN, 78);
    BEAT(p, 2, "..x...x.3.x...4.|..x...x...x.4.3.", 84);
    SET(p, 3, SA, 120, SB, 40, SC, 100, SD, 10, SE, 80, P_HOLD, 127, P_DEC, 127, P_REL, 16,
        P_WDTH, 40, P_LPQ, 84, P_DIST, 70, P_VOL, 53, P_GATE, 100);
    BEAT(p, 3, "x.......x...s...|x.....x.....x...", 41, 41, 44, 41, 39, 44);
    lfo(p, 3, 0, P_WDTH, 1, 3, 64, 120, 1);
    lk(p, 3, 8, P_LFO + L_MULT, enc(4, 8)); lk(p, 3, 22, P_LFO + L_MULT, enc(4, 8)); lk(p, 3, 28, P_LFO + L_SPD, 96);
    SET(p, 4, SE, 0, SC, 40, P_HOLD, 127, P_DEC, 127, P_REL, 14, P_VOL, 52, P_GATE, 100);
    BEAT(p, 4, "x.......x...s...|x.....x.....x...", 29, 29, 32, 29, 27, 32);
    SET(p, 5, SA, VOWEL(5), SB, 100, SC, 30, SF, 60, SG, 90, P_DEC, 20, P_REL, 12, P_DIST, 40, P_SPU, 50, P_VOL, 80, P_PAN, 56);
    BEAT(p, 5, "....x.......x.x.|....x.......x...", 41, 44, 41, 39);
    lfo(p, 5, 0, SA, 3, 3, 64, 110, 1);

    /* A16: empty, to start from */
    for (int i = 0; i < 15; i++) chain_v4(&pat[i]);    /* onto the Monomachine's chain, as an old pattern would go */

    /* A16 INIT: the blank kit itself, on today's chain */
    p = &pat[15];
    pat_init(p, "INIT");
    static const uint8_t M16[M2K_TRACKS] = {M_GSIN, M_GNOI, M_GNOI, M_SID, M_FMS, M_ENS};
    for (int t = 0; t < M2K_TRACKS; t++) kit_default(p, t, M16[t]);
    p->glob[G_TMPO] = 120 - 30;
    p->glob[G_VERB] = 40;
}
