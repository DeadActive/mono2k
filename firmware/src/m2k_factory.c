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

/* the demos were voiced on MONO2K's first envelope curve: their ATK HOLD DEC REL are given on it and moved onto the
 * Monomachine's as they are set (amp_v1_val), so they sound as they were written */
static void lk(m2k_pattern_t *p, int t, int s, int prm, int v)
{
    for (int i = 0; i < M2K_LOCKS; i++)
        if (p->lock[i].trk == 0xFF) { p->lock[i] = (m2k_lock_t){(uint8_t)t, (uint8_t)s, (uint8_t)prm, amp_v1_val(prm, v)}; return; }
}

static void set(m2k_pattern_t *p, int t, const uint8_t *pv, int n)   /* (param, value) pairs */
{
    for (int i = 0; i + 1 < n; i += 2) p->kit[t][pv[i]] = amp_v1_val(pv[i], pv[i + 1]);
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

static m2k_pattern_t *pat_new(int i, const char *name, int bpm, const uint8_t *machines)
{
    m2k_pattern_t *p = &pat[i];
    pat_init(p, name);
    for (int t = 0; t < M2K_TRACKS; t++) {
        kit_default(p, t, machines[t]);
        static const uint8_t AMP0[4] = {0, 0, 60, 30};          /* the blank kit the demos were voiced from */
        for (int k = 0; k < 4; k++) p->kit[t][P_ATK + k] = amp_v1_val(P_ATK + k, AMP0[k]);
    }
    p->glob[G_TMPO] = (uint8_t)(bpm - 30);
    p->glob[G_VERB] = 40;
    return p;
}
#define MACHINES(...) (const uint8_t[]){__VA_ARGS__}

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
    SET(p, 4, SA, 30, SB, VOICES(4), P_DEC, 70, P_WDTH, 64, P_LPQ, 90, P_FDEC, 46, P_WOF, 86, P_BITS, 50, P_VOL, 74, P_SPU, 30);
    BEAT(p, 4, ".x...x..s....x..", 72, 75, 79, 77);
    lk(p, 4, 5, P_SRR, 80); lk(p, 4, 13, P_XOR, 70);
    SET(p, 5, SH, 127, SC, 70, SB, 110, P_DEC, 70, P_VOL, 70, P_PAN, 90, P_SPU, 60);
    BEAT(p, 5, "..............x.", 84);
    lk(p, 5, 14, SH, 0);

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

    /* A09 RUNNER: drum and bass, a reese that breathes */
    p = pat_new(8, "RUNNER", 172, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_SAW, M_FMS, M_GNOI));
    p->glob[G_DRIV] = 40; p->glob[G_VERB] = 40; p->glob[G_GLCH] = 20; p->glob[G_GSIZ] = GSIZE(2);
    SET(p, 0, SE, 70, SF, 40, SG, 50, P_DEC, 78, P_VOL, 112);
    BEAT(p, 0, "x.........x.....", 36);
    SET(p, 1, SA, 68, P_DEC, 72, P_BASE, 44, P_DIST, 30, P_VOL, 96);
    BEAT(p, 1, "....X.....?.X..2", 60);
    SET(p, 2, SA, 116, P_DEC, 50, P_BASE, 100, P_VOL, 70, P_PAN, 78);
    BEAT(p, 2, "x.x.x.x.x.x.x.x.", 84);
    SET(p, 3, SA, 70, SB, VOICES(3), SC, 100, SD, 60, SF, 30, P_HOLD, 127, P_DEC, 127, P_REL, 50,
        P_WDTH, 64, P_LPQ, 30, P_VOL, 92, P_GATE, 64);
    BEAT(p, 3, "x.......s.......", 33, 36);
    lfo(p, 3, 0, P_WDTH, 1, 0, 24, 82, 0);
    SET(p, 4, SA, RATIO(3), SB, 50, SC, 50, SD, RATIO(6), SE, 30, P_DEC, 74, P_DSND, 60, P_DTIM, 12, P_DFB, 60, P_VOL, 72, P_PAN, 40);
    BEAT(p, 4, "......x.......x.", 69, 72);
    SET(p, 5, SA, 90, P_DEC, 62, P_BASE, 70, P_VOL, 70, P_PAN, 30);
    BEAT(p, 5, "..x...x..?....x.", 72);

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

    /* A11 DUBTEC: chords into a long delay, soft kick, tape and dust */
    p = pat_new(10, "DUBTEC", 120, MACHINES(M_GSIN, M_GNOI, M_GSIN, M_ENS, M_GSIN, M_GNOI));
    p->glob[G_TAPE] = 30; p->glob[G_VERB] = 90; p->glob[G_DAMP] = 70; p->glob[G_HISS] = 18; p->glob[G_DRIV] = 10;
    SET(p, 0, SE, 56, SF, 44, SG, 10, P_DEC, 82, P_VOL, 110);
    BEAT(p, 0, "x...x...x...x...", 36);
    SET(p, 1, SA, 104, P_DEC, 58, P_BASE, 86, P_VOL, 86);
    BEAT(p, 1, "..x...x...x...x.", 80);
    SET(p, 2, SE, 20, SF, 30, SH, 40, P_DEC, 50, P_VOL, 66, P_PAN, 84);
    BEAT(p, 2, "...x..?....x..q.", 84);
    SET(p, 3, SA, SEMI(3), SB, SEMI(7), SC, SEMI(10), SD, 30, P_HOLD, 20, P_DEC, 74, P_WDTH, 64, P_LPQ, 40,
        P_DSND, 100, P_DTIM, 12, P_DFB, 100, P_DBAS, 30, P_DWID, 60, P_SPU, 70, P_VOL, 80);
    BEAT(p, 3, "..x.....x..x....", 57);
    lfo(p, 3, 0, P_WDTH, 1, 0, 10, 90, 0);
    SET(p, 4, SE, 0, SC, 60, P_HOLD, 40, P_DEC, 94, P_VOL, 100);
    BEAT(p, 4, "x.......x.......", 33);
    SET(p, 5, SA, 40, P_ATK, 80, P_DEC, 84, P_VOL, 84, P_SPU, 80);
    BEAT(p, 5, "x...............", 60);

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

    /* A13 INDSTRL: a distorted kick, metal on metal, a grinding bass */
    p = pat_new(12, "INDSTRL", 130, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_FMD, M_WAV, M_VOX));
    p->glob[G_DRIV] = 90; p->glob[G_VERB] = 40; p->glob[G_DAMP] = 20; p->glob[G_GLCH] = 20; p->glob[G_GSIZ] = GSIZE(2); p->glob[G_ADPC] = 20;
    SET(p, 0, SE, 80, SF, 40, SB, 60, P_DEC, 80, P_DIST, 100, P_VOL, 98);
    BEAT(p, 0, "x..x..x...x..x..", 33);
    SET(p, 1, SC, 127, SD, 120, P_DEC, 72, P_FOLD, 40, P_XOR, 40, P_VOL, 88);
    BEAT(p, 1, "....x.......x...", 60);
    SET(p, 2, SA, 100, P_DEC, 52, P_BITS, 90, P_SRR, 60, P_VOL, 72);
    BEAT(p, 2, "x.xx.x.x.xx.x.x.", 84);
    SET(p, 3, SA, RATIO(3), SB, 120, SC, 127, SG, 90, SH, 64, P_HOLD, 30, P_DEC, 76, P_DIST, 80, P_VOL, 90);
    BEAT(p, 3, "x.x...x.x.x...x.", 31, 31, 34, 31, 29);
    SET(p, 4, SA, WAVE(30), SD, 100, SE, 70, P_DEC, 70, P_VOL, 80);
    BEAT(p, 4, "..x.......x.....", 60, 63);
    SET(p, 5, SA, VOWEL(0), SG, 127, SF, 100, P_DEC, 70, P_DIST, 90, P_VOL, 70, P_PAN, 40);
    BEAT(p, 5, "......x.......x.", 55, 58);

    /* A14 MINIMAL: clicks, blips and chances, a small chord in the delay */
    p = pat_new(13, "MINIMAL", 124, MACHINES(M_GSIN, M_GSIN, M_GNOI, M_GSIN, M_FMS, M_GSIN));
    p->glob[G_SWNG] = 40; p->glob[G_VERB] = 50; p->glob[G_GLCH] = 8; p->glob[G_GSIZ] = GSIZE(1);
    SET(p, 0, SE, 60, SF, 40, P_DEC, 80, P_VOL, 110);
    BEAT(p, 0, "x...x...x...x...", 36);
    SET(p, 1, SE, 40, SF, 20, SG, 100, P_DEC, 50, P_VOL, 74, P_PAN, 80);
    BEAT(p, 1, "..x..x.?..x..?.x", 96);
    SET(p, 2, SA, 110, P_DEC, 58, P_BASE, 88, P_VOL, 86);
    BEAT(p, 2, "..x...x...x...x.", 80);
    SET(p, 3, SE, 0, SH, 30, P_DEC, 64, P_DSND, 50, P_DTIM, 6, P_DFB, 50, P_VOL, 70, P_PAN, 44);
    BEAT(p, 3, "...x...?..x....q", 72, 79, 84);
    SET(p, 4, SA, RATIO(6), SB, 40, SC, 40, P_DEC, 70, P_DSND, 70, P_DTIM, 12, P_DFB, 60, P_VOL, 72);
    BEAT(p, 4, ".......x......x.", 63);
    SET(p, 5, SE, 0, SC, 50, P_HOLD, 20, P_DEC, 84, P_VOL, 100);
    BEAT(p, 5, "x.....x.x.......", 36, 36, 43);

    /* A15 TRAP: a gliding 808, a lonely snare, hats that roll */
    p = pat_new(14, "TRAP", 140, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_FMS, M_VOX, M_GNOI));
    p->glob[G_VERB] = 60; p->glob[G_DAMP] = 60; p->glob[G_DRIV] = 20;
    SET(p, 0, SE, 30, SF, 60, SC, 40, P_HOLD, 20, P_DEC, 96, P_DIST, 30, P_PORT, 30, P_VOL, 114);
    BEAT(p, 0, "x......x..s.....", 31, 31, 34);
    SET(p, 1, SA, 70, P_DEC, 74, P_BASE, 46, P_VOL, 96);
    BEAT(p, 1, "........X.......", 60);
    SET(p, 2, SA, 118, P_DEC, 48, P_BASE, 100, P_VOL, 72, P_PAN, 76);
    BEAT(p, 2, "x.x.x3x.x.x.4.x.", 84);
    SET(p, 3, SA, RATIO(10), SB, 50, SC, 60, P_DEC, 84, P_VOL, 74, P_SPU, 50);
    BEAT(p, 3, "x...x...x.....x.", 75, 70, 72, 67);
    SET(p, 4, SA, VOWEL(5), P_ATK, 60, P_HOLD, 127, P_DEC, 127, P_REL, 74, P_GATE, 120, P_SPU, 90, P_VOL, 70);
    BEAT(p, 4, "x...............", 63);
    SET(p, 5, SA, 100, P_DEC, 76, P_BASE, 80, P_VOL, 62, P_PAN, 40);
    BEAT(p, 5, "......x.......x.", 80);

    /* A16: empty, to start from */
    pat_new(15, "INIT", 120, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_SID, M_FMS, M_ENS));
}
