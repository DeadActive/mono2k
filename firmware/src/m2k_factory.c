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

/* the blank kit of MONO2K's chain before version 5: what an old pattern was written against (the self-test's) */
static void kit_v4(m2k_pattern_t *p, int t, int m)
{
    static const uint8_t D[] = {0, 0, 64, 64, 0, 100, 64, 0, 0, 127, 0, 0, 0, 64, 64, 64, 64, 64, 0, 12, 0, 50, 0, 110};
    kit_default(p, t, m);
    m_cpy(&p->kit[t][P_ATK], D, sizeof D);
    p->kit[t][P_LEVL] = 110;
}

#define MACHINES(...) (const uint8_t[]){__VA_ARGS__}

/* a demo written on today's chain, the Monomachine's (version 5): DIST 64 clean, the filter's steps over the note
 * (16 on it, 8 an octave), DTIM in 256ths of a note, DSND from 64 (below keeps the side, above ping-pongs) */
static m2k_pattern_t *pat_v5(int i, const char *name, int bpm, const uint8_t *machines)
{
    m2k_pattern_t *p = &pat[i];
    pat_init(p, name);
    for (int t = 0; t < M2K_TRACKS; t++) kit_default(p, t, machines[t]);
    p->glob[G_TMPO] = (uint8_t)(bpm - 30);
    p->glob[G_VERB] = 50;
    return p;
}

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
#define SIDW(i) enc(i, 8)              /* 0 TRI 1 SAW 2 PUL 3 NOI 4 T+S 5 T+P 6 S+P 7 ALL */
#define MODE4(i) enc(i, 4)             /* SID: OFF SYNC RING S+R; FM+STAT: STCK PAR 2CAR RING */
#define RATIO(i) enc(i, 32)            /* 3: 1.0, 6: 2.0, 8: 2.76, 9: 3.0, 10: 3.5, 11: 4.0, 13: 5.0, 15: 6.0, 17: 7.07, 26: 15.7 */
#define SEMI(n) enc(n, 25)             /* ENS intervals, 0..24 semitones */
#define WAVE(i) ((uint8_t)((i) * 127 / 31))   /* DIGI-WAV: 5 BELL 6 GLAS 7 PLST 8 LTEX 19 MTL1 20 MTL2 25 BITS 26 8BIT 30 CRSH */
#define VOWEL(i) ((uint8_t)((i) * 127 / 7))   /* A AE E I Y O U ER */
#define VOICES(n) enc((n) - 1, 7)
#define GSIZE(i) enc(i, 5)             /* 1/64 1/32 1/16 1/8 1/4 */
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

static void factory(void)
{
    m2k_pattern_t *p;

    /* A01 HAZE, after Boards of Canada: D minor at 88, a one-chord vamp, Dm7 opening to Dsus2 (no third) in the
     * second half: a dark pad, chorused and wobbling on the tape (the detune is the point: TAPE 80, about +-10 c,
     * slow); a dusty beat with room in it and a sub that walks under it carry it. Over it a fragment of four long notes,
     * A G, then E D, low on one of the owner's dark waves, low-passed, bit-worn and lightly driven into a dark echo */
    p = pat_v5(0, "HAZE", 88, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_ENS, M_GSIN, M_WAV));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 18; p->glob[G_TAPE] = 80; p->glob[G_HISS] = 56; p->glob[G_ADPC] = 16;
    p->glob[G_VERB] = 74; p->glob[G_DAMP] = 80; p->glob[G_DRIV] = BANK_DRIVE;
    SET(p, 0, SE, 56, SF, 28, SG, 16, SC, 30, P_DEC, 34, P_REL, 10, P_DIST, 66, P_VOL, 120);
    BEAT(p, 0, "x.........x.....|x.......x.......|x.........x.....|x.......x..h....", 38);
    SET(p, 1, SA, 62, SC, 20, SD, 40, SE, 20, SF, 20, SG, 16, P_DEC, 30, P_REL, 14, P_BASE, 18, P_SRR, 30, P_BITS, 20,
        P_SPU, 50, P_VOL, 95);
    BEAT(p, 1, "....x.......x...|....x.......x...|....x.......x...|....x.......x.q.", 60);
    SET(p, 2, SA, 100, P_ATK, 10, P_DEC, 14, P_REL, 8, P_BASE, 36, P_SRR, 40, P_VOL, 60, P_PAN, 58);
    BEAT(p, 2, "..x...x...x...x.|..x...x?..x...x.|..x...x...x...x.|..x...x...x?..x.", 60);
    SET(p, 3, SA, PCH(3), SB, PCH(7), SC, PCH(10), SD, 30, SF, 70, SG, 30, P_ATK, 80, P_HOLD, 127, P_DEC, 127,
        P_REL, 90, P_GATE, 127, P_WDTH, 28, P_LPQ, 0, P_SPU, 80, P_VOL, 95);
    BEAT(p, 3, "x...............|x...............|x...............|x...............", 50);
    chords(p, 3, 32, 64, 2, 7, 12);                                         /* Dsus2 */
    lfo(p, 3, 0, P_WDTH, 1, 0, 16, 70, 0);
    SET(p, 4, SE, 0, SC, 40, SG, 0, SH, 6, P_HOLD, 127, P_DEC, 127, P_REL, 40, P_GATE, 60, P_WDTH, 24, P_VOL, 65);
    BEAT(p, 4, "x...............|x.........x.....|x...............|x.........x.....", 38, 38, 36, 38, 34, 33);
    SET(p, 5, SA, OS(30), SB, 64, SC, 127, SE, 30, SF, 64, SG, 0, SH, 0, P_WBNK, 127, P_ATK, 20, P_HOLD, 127,
        P_DEC, 127, P_REL, 60, P_GATE, 56, P_WDTH, 24, P_DIST, 66, P_SRR, 30, P_DTIM, 64, P_DSND, 30, P_DFB, 50,
        P_DBAS, 10, P_DWID, 30, P_SPU, 70, P_VOL, 51, P_PAN, 70);
    BEAT(p, 5, "................|..x.......x.....|................|..x.......h.H...", 57, 55, 52, 50, 45);
    trim(p, 93);

    /* A02 SPLICE, after Autechre (the mid-90s): E phrygian at 104 in 14 beats (56 steps), so the rhythm turns against
     * the bar. The idea is the metal: a low FM clank whose decay is new every few steps and whose ratio is locked to a
     * cleaner tick here and there, ratcheting, on trig conditions, with a kick off the grid and hats crushed to a
     * console's noise. Under it an open chord with no third (E B D E) that turns to Fmaj7 every second loop, a sub,
     * and four long notes, B A G, then E, low on one of the owner's buzzing waves, closed down and crushed */
    p = pat_v5(1, "SPLICE", 104, MACHINES(M_GSIN, M_FMD, M_GNOI, M_WAV, M_ENS, M_GSIN));
    p->glob[G_LEN] = 56; p->glob[G_SWNG] = 4; p->glob[G_GLCH] = 24; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_ADPC] = 24;
    p->glob[G_DRIV] = 32; p->glob[G_VERB] = 40; p->glob[G_DAMP] = 60;
    SET(p, 0, SE, 50, SF, 18, SG, 50, SC, 10, P_DEC, 22, P_REL, 6, P_DIST, 74, P_VOL, 127);
    BEAT(p, 0, "x.....x...h.....|x..?....x..H..x.|x.....x...k.....|x..q..x.", 40);
    SET(p, 1, SA, RATIO(17), SB, 70, SC, 22, SD, RATIO(26), SE, 36, SF, 12, SG, 30, SH, 100, P_DEC, 20, P_REL, 6,
        P_GATE, 4, P_WDTH, 56, P_DIST, 72, P_DTIM, 24, P_DSND, 100, P_DFB, 36, P_DWID, 40, P_SPU, 30, P_VOL, 56,
        P_PAN, 44);
    BEAT(p, 1, "..x.x..2...x.x..|.x...x.3..x..x.x|..x.x..2...x.x..|.x..4.x.", 52);
    for (int st = 11; st < 56; st += 16) lk(p, 1, st, SA, RATIO(13));      /* a cleaner tick: 5.0 */
    lk(p, 1, 20, SB, 96);
    lfo(p, 1, 0, SC, 6, 2, 48, 80, 0);                                      /* its decay new every few steps */
    SET(p, 2, SA, 112, SG, 70, P_DEC, 6, P_REL, 4, P_BASE, 40, P_VOL, 62);
    BEAT(p, 2, "x?x.x?xex?x.x?x2|x?x.x?xex?xqx?x.|x?x.x?xex?x.x?x2|x?x.3?x.", 60);
    lfo(p, 2, 0, P_PAN, 6, 2, 64, 100, 0);
    SET(p, 3, SA, OS(42), SB, 64, SC, 127, SE, 40, SF, 64, SG, 0, SH, 0, P_WBNK, 127, P_ATK, 30, P_HOLD, 127,
        P_DEC, 127, P_REL, 50, P_GATE, 48, P_WDTH, 22, P_DIST, 69, P_SRR, 40, P_DTIM, 72, P_DSND, 34, P_DFB, 52,
        P_DBAS, 20, P_DWID, 30, P_SPU, 50, P_VOL, 85, P_PAN, 80);
    BEAT(p, 3, "........x.......|....x...x.......|............x...|........", 59, 57, 55, 52);
    SET(p, 4, SA, PCH(7), SB, PCH(10), SC, PCH(12), SD, 20, P_ATK, 90, P_HOLD, 127, P_DEC, 127, P_REL, 96,
        P_GATE, 127, P_WDTH, 24, P_LPQ, 0, P_SPU, 70, P_VOL, 110);
    BEAT(p, 4, "x...............|............H...|x...............|........", 52, 53, 52);
    chord(p, 4, 28, 4, 7, 11);                                              /* Fmaj7, every second loop */
    SET(p, 5, SE, 0, SC, 30, SG, 0, SH, 0, P_HOLD, 127, P_DEC, 127, P_REL, 40, P_GATE, 60, P_WDTH, 24, P_VOL, 67);
    BEAT(p, 5, "x...............|............H...|x...............|........", 28, 29, 28);
    trim(p, 87);

    /* A03 RIBBON, after Aphex Twin (drill'n'bass): F minor at 168. Fast programmed breaks, kick and snare ratcheting
     * and rolling, over a pedal F in the sub; a dark pad, Fm7 then Db over F; and a slow melancholic line in its low
     * octave on one of the owner's dark waves, its second oscillator 12.5 c sharp against the first (the detune is
     * the point: the line beats), low-passed, driven and worn. Every second loop the drums drop out of the last bar
     * and the line and the pad are left alone */
    p = pat_v5(2, "RIBBON", 168, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_WAV, M_ENS, M_GSIN));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 0; p->glob[G_VERB] = 72; p->glob[G_DAMP] = 70; p->glob[G_ADPC] = 20;
    p->glob[G_GLCH] = 16; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_DRIV] = 28; p->glob[G_HISS] = 32;
    SET(p, 0, SE, 66, SF, 18, SG, 40, SC, 16, P_DEC, 22, P_REL, 6, P_DIST, 72, P_VOL, 127);
    BEAT(p, 0, "x..x..2...x..x..|x.x...x...3...x.|x..x..2...x..x..|h..h..h.....h...", 41);
    SET(p, 1, SA, 70, SC, 40, SD, 56, SE, 36, SF, 18, SG, 50, P_DEC, 26, P_REL, 10, P_BASE, 20, P_EQG, 70, P_SRR, 16,
        P_SPU, 30, P_VOL, 104);
    BEAT(p, 1, "....x..q.2..x..3|..q.x...x.3.x.4.|....x..q.2..x..3|....h.....h.h.h.", 60);
    SET(p, 2, SA, 110, SG, 30, P_DEC, 8, P_REL, 4, P_BASE, 42, P_VOL, 73, P_PAN, 74);
    BEAT(p, 2, "xxqxx2qxxxqxx3qx|xxqxxxqx2xqxxxqx|xxqxx2qxxxqxx3qx|h.h.h.h.h.h.h.h.", 60);
    SET(p, 3, SA, OS(47), SB, 64, SC, 127, SE, 20, SF, 65, SG, 70, SH, 0, P_WBNK, 127, P_ATK, 30, P_HOLD, 127,
        P_DEC, 127, P_REL, 50, P_GATE, 64, P_WDTH, 28, P_DIST, 66, P_SRR, 20, P_DTIM, 64, P_DSND, 34, P_DFB, 46,
        P_DBAS, 10, P_DWID, 40, P_SPU, 70, P_VOL, 58, P_PAN, 56);   /* osc 2 one step (12.5 c) sharp */
    BEAT(p, 3, "x.......x...x...|....x...........|x...........x...|............h...|", 60, 58, 56, 53, 56, 53, 51);
    SET(p, 4, SA, PCH(3), SB, PCH(7), SC, PCH(10), SD, 40, P_ATK, 92, P_HOLD, 127, P_DEC, 127, P_REL, 92,
        P_GATE, 127, P_WDTH, 28, P_LPQ, 0, P_SPU, 90, P_VOL, 72);
    BEAT(p, 4, "x.......x.......|x.......x.......|x.......x.......|x.......x.......", 53);
    chords(p, 4, 32, 64, 3, 8, 12);                                         /* Db over F */
    SET(p, 5, SE, 0, SC, 40, SG, 0, SH, 10, P_HOLD, 127, P_DEC, 127, P_REL, 20, P_GATE, 60, P_WDTH, 24, P_VOL, 67);
    BEAT(p, 5, "x.........x.....|x.........x.....|x.........x.....|x.........x.....", 41);
    trim(p, 77);

    /* A04 TENFOUR, flavour 2, after Radiohead: the opening progression of "Everything in Its Right Place" (2000),
     * from the owner's MIDI; the rest is MONO2K's own. Around C at 108, four bars. The harmony: C, Dbmaj7 and Eb6, each
     * with C on top, in the opening's proportions over the four bars (6, 4 and 4 beats, then 2 with the Eb ringing),
     * swelling on a chorused pad, driven, crushed and worn by the tape, its filter breathing. Under it a driven sine
     * bass on the roots, its sub on C1, Db1 and Eb1, walking G Bb back to C. The drums never quite repeat: a hard kick,
     * a crushed snare whose length and crack change, ghosts round it, its rate drifting; short hats in swung sixteenths
     * whose decay wanders, with metal ticks and pitched blips locked among them. Over it a four-bar line from the first
     * step, one figure (fifth, third, fifth, the top, a step down) through each chord, held on the changes and sliding
     * into some, on one of the owner's waves, crushed, its filter moving */
    p = pat_v5(3, "TENFOUR", 108, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_ENS, M_GSIN, M_WAV));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 22; p->glob[G_TAPE] = 70; p->glob[G_HISS] = 60; p->glob[G_ADPC] = 40;
    p->glob[G_GLCH] = 16; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_VERB] = 56; p->glob[G_DAMP] = 76; p->glob[G_ACNT] = 50;
    p->glob[G_DRIV] = BANK_DRIVE;
    SET(p, 0, SE, 60, SF, 22, SG, 40, SC, 40, P_DEC, 34, P_REL, 6, P_DIST, 82, P_SRR, 12, P_VOL, 127);
    BEAT(p, 0, "X.........x.....|x.......X.......|x.......X.x.....|x.........x...x.", 36);
    SET(p, 1, SA, 96, SC, 30, SD, 50, SG, 50, P_DEC, 20, P_REL, 6, P_BASE, 26, P_DIST, 78, P_BITS, 36, P_SRR, 30,
        P_SPU, 40, P_VOL, 98);
    BEAT(p, 1, "....X..x....X...|....X.......X..x|....X..x....X...|....X..x....X.x.", 60);
    static const uint8_t ghost4[] = {7, 31, 39, 55, 62};
    for (unsigned i = 0; i < sizeof ghost4; i++) lk(p, 1, ghost4[i], P_VOL, 58);     /* the ghosts, 9 dB down */
    lk(p, 1, 28, P_DEC, 32); lk(p, 1, 60, P_DEC, 32);                       /* a longer one closing bars 2 and 4 */
    lk(p, 1, 44, SC, 100); lk(p, 1, 44, SD, 90);                            /* a metal crack */
    lfo(p, 1, 0, P_SRR, 6, 3, 40, 80, 0);                                   /* its rate drifting, hit to hit */
    SET(p, 2, SA, 116, SG, 80, P_ATK, 0, P_DEC, 4, P_REL, 3, P_BASE, 44, P_SRR, 44, P_BITS, 44, P_VOL, 61, P_PAN, 60);
    BEAT(p, 2, "x?X.x?X.x?X.x?X2|x?X.x?X.x?X3x?X.|x?X.x?X.x?X.x?X2|x?X.x?X.x2X.x?X4", 60);
    lfo(p, 2, 0, P_DEC, 6, 3, 56, 84, 0);                                   /* their length, never the same twice */
    lfo(p, 2, 1, P_PAN, 0, 1, 20, 84, 0);
    for (int st = 9; st < 64; st += 18) { lk(p, 2, st, SC, 110); lk(p, 2, st, SD, 90); }   /* metal ticks */
    lk(p, 2, 19, SB, 70); lk(p, 2, 51, SB, 76);                             /* pitched blips */
    lk(p, 2, 30, P_DEC, 24); lk(p, 2, 62, P_DEC, 24);
    SET(p, 3, SA, PCH(4), SB, PCH(7), SC, PCH(12), SD, 40, SF, 50, SG, 30, P_ATK, 60, P_HOLD, 127, P_DEC, 127,
        P_REL, 60, P_GATE, 127, P_WDTH, 30, P_LPQ, 0, P_DIST, 74, P_SRR, 20, P_BITS, 16, P_SPU, 60, P_VOL, 102);
    BEAT(p, 3, "x...............|........x.......|........x.......|................", 48, 49, 51);
    chord(p, 3, 24, 4, 7, 11);                                              /* Dbmaj7 */
    chord(p, 3, 40, 4, 7, 9);                                               /* Eb6 */
    lfo(p, 3, 0, P_WDTH, 1, 0, 24, 74, 0);                                  /* its filter, breathing */
    SET(p, 4, SE, 0, SC, 64, SG, 0, SH, 30, P_HOLD, 127, P_DEC, 127, P_REL, 30, P_GATE, 50, P_WDTH, 28, P_DIST, 80,
        P_VOL, 85);
    BEAT(p, 4, "x.........x.....|x.......x.......|x.......x.......|x.........x...x.", 36, 36, 36, 37, 37, 39, 39, 43, 46);
    SET(p, 5, SA, OS(16), SB, 64, SC, 127, SE, 30, SF, 64, SG, 0, SH, 0, P_WBNK, 127, P_ATK, 16, P_HOLD, 127,
        P_DEC, 127, P_REL, 60, P_GATE, 50, P_WDTH, 30, P_DIST, 68, P_SRR, 36, P_BITS, 30, P_DTIM, 64, P_DSND, 30,
        P_DFB, 44, P_DBAS, 10, P_DWID, 30, P_SPU, 56, P_VOL, 66, P_PAN, 64);
    BEAT(p, 5, "x..x..x.s...x.x.|x..x..x.s..x..x.|s...x.x.x..x..x.|s...x.x.x...x...",
         55, 52, 55, 60, 58, 55, 52, 55, 60, 56, 53, 56, 60, 56, 58, 55, 51, 55, 60, 58, 55, 58, 55);
    for (int st = 0; st < 64; st += 8) lk(p, 5, st, P_GATE, 100);           /* held on the beats and the changes */
    lfo(p, 5, 0, P_WDTH, 1, 1, 20, 76, 0);
    trim(p, 70);

    /* A05 SMOKE, dub techno after Basic Channel: C minor at 120, one chord. The idea is a Cm7 stab, short and dark,
     * on the offbeat sixteenths, thrown into a long, filtered ping-pong echo whose feedback swells and falls; its own
     * filter breathing over the four bars. A deep four-to-the-floor, offbeat hats with shuffled ghosts, a rim far in the
     * echo, a sub rolling on C; hiss and a touch of tape under it all */
    p = pat_v5(4, "SMOKE", 120, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_ENS, M_GSIN, M_GSIN));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 14; p->glob[G_TAPE] = 32; p->glob[G_HISS] = 40; p->glob[G_ADPC] = 16;
    p->glob[G_VERB] = 66; p->glob[G_DAMP] = 72; p->glob[G_DRIV] = 28;
    SET(p, 0, SE, 50, SF, 30, SG, 10, SC, 30, P_DEC, 36, P_REL, 10, P_DIST, 66, P_VOL, 127);
    BEAT(p, 0, "x...x...x...x...|x...x...x...x...|x...x...x...x...|x...x...x...x..q", 36);
    SET(p, 1, SA, 104, P_DEC, 12, P_REL, 6, P_BASE, 40, P_SRR, 20, P_VOL, 65, P_PAN, 60);
    BEAT(p, 1, "..x...x...x...x.|..x..qx...x..qx.|..x...x...x...x.|..x..qx...x.?.x.", 60);
    SET(p, 2, SA, 90, SC, 60, SD, 70, SF, 10, P_DEC, 10, P_REL, 6, P_BASE, 30, P_DTIM, 48, P_DSND, 40, P_DFB, 50,
        P_DWID, 40, P_SPU, 50, P_VOL, 77, P_PAN, 70);
    BEAT(p, 2, "......x.........|..........x.....|......x.........|..........x...q.", 60);
    SET(p, 3, SA, PCH(3), SB, PCH(7), SC, PCH(10), SD, 30, P_DEC, 44, P_REL, 30, P_GATE, 6, P_WDTH, 30, P_LPQ, 0,
        P_WOF, 76, P_FDEC, 40, P_DTIM, 48, P_DSND, 100, P_DFB, 58, P_DBAS, 30, P_DWID, 28, P_SPU, 70, P_VOL, 127, P_LEVL, 122);
    BEAT(p, 3, "...x.......x....|...x.......x..h.|...x.......x....|...x...q...x..H.", 48);
    lfo(p, 3, 0, P_WDTH, 1, 0, 16, 74, 0);                                  /* its filter, over the four bars */
    lfo(p, 3, 1, P_DFB, 0, 0, 8, 67, 0);                                    /* the echo's feedback, swelling */
    SET(p, 4, SE, 0, SC, 40, SG, 0, SH, 0, P_HOLD, 127, P_DEC, 127, P_REL, 30, P_GATE, 30, P_WDTH, 24, P_VOL, 72);
    BEAT(p, 4, "x.....x...x.....|x.....x...x.....|x.....x...x.....|x.....x...x.....",
         36, 36, 31, 36, 36, 39, 36, 36, 31, 36, 43, 39);
    trim(p, 85);

    /* A06 TORQUE, hardgroove after Ben Sims: E minor at 134. The idea is the loop of drums: a hard kick, sixteenth
     * hats with open ones on the offbeats, a clap, and toms tuned to the key rolling a tribal figure around them; a
     * short low riff on one of the owner's buzzing waves, crushed and driven, a few notes; a rolling offbeat bass */
    p = pat_v5(5, "TORQUE", 134, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_GSIN, M_WAV, M_SID));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 12; p->glob[G_DRIV] = 48; p->glob[G_ADPC] = 20; p->glob[G_GLCH] = 16;
    p->glob[G_GSIZ] = GSIZE(2); p->glob[G_VERB] = 30; p->glob[G_DAMP] = 50;
    SET(p, 0, SE, 56, SF, 22, SG, 20, SC, 24, P_DEC, 34, P_REL, 6, P_DIST, 72, P_VOL, 127);
    BEAT(p, 0, "x...x...x...x...|x...x...x...x...|x...x...x...x...|x...x...x..xx.x.", 40);
    SET(p, 1, SA, 112, SG, 30, P_DEC, 10, P_REL, 4, P_BASE, 44, P_VOL, 68, P_PAN, 56);
    BEAT(p, 1, "x?xxx?xxx?xxx?xx|x?xxx?xxx?xxx?x2|x?xxx?xxx?xxx?xx|x?xxx?xxx?xx2?xx", 60);
    for (int st = 2; st < 64; st += 4) lk(p, 1, st, P_DEC, 28);             /* the open hats, on the offbeats */
    SET(p, 2, SA, 74, SC, 40, SD, 60, SG, 30, P_DEC, 30, P_REL, 12, P_BASE, 22, P_SPU, 30, P_VOL, 96);
    BEAT(p, 2, "....x.......x...|....x.......x..q|....x.......x...|....x..q....x.x.", 60);
    SET(p, 3, SE, 30, SF, 30, SG, 30, SC, 0, P_DEC, 24, P_REL, 8, P_DIST, 70, P_VOL, 66, P_PAN, 54);
    BEAT(p, 3, "..x..x....x..x..|..x..x..x.x..x.x|..x..x....x..x..|..x.xx..x.x.3x.x", 47, 43, 40, 43, 52);
    SET(p, 4, SA, OS(40), SB, 64, SC, 127, SE, 50, SF, 64, SG, 0, SH, 0, P_WBNK, 127, P_DEC, 36, P_REL, 14, P_GATE, 8,
        P_WDTH, 30, P_DIST, 76, P_SRR, 30, P_DTIM, 32, P_DSND, 34, P_DFB, 36, P_DWID, 30, P_SPU, 30, P_VOL, 104,
        P_PAN, 74);
    BEAT(p, 4, "x.....x...x.....|..x.....x.......|x.....x...x.....|..x...h.x.......", 52, 52, 55, 50, 52, 52, 52, 55, 50, 57, 52);
    SET(p, 5, SA, SIDW(2), SB, 40, SC, 20, SD, SIDW(0), SE, 64, SF, 64, SG, MODE4(0), SH, 30, P_DEC, 20, P_REL, 6,
        P_GATE, 4, P_WDTH, 40, P_LPQ, 0, P_FDEC, 20, P_WOF, 66, P_DIST, 74, P_VOL, 96);
    BEAT(p, 5, "..x.x.x...x.x.x.|..x.x.x...x.x.x.|..x.x.x...x.x.x.|..x.x.x...x.x.xx",
         28, 40, 28, 28, 40, 28, 28, 40, 28, 28, 40, 28, 28, 40, 28, 28, 40, 28, 28, 40, 28, 28, 40, 28, 31);
    trim(p, 61);

    /* A07 TUNNEL, after Overmono: UK breakbeat techno in G minor at 130. The idea is the broken beat, a kick and a
     * snare with ghosts and shuffled hats driving it, under warm minor chords that swell each bar, Gm7 then Cm7; a low
     * vocal chop calling on its two notes, through a dotted echo; a sub rolling with the kick */
    p = pat_v5(6, "TUNNEL", 130, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_ENS, M_VOX, M_GSIN));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 12; p->glob[G_DRIV] = 36; p->glob[G_ADPC] = 12; p->glob[G_VERB] = 62;
    p->glob[G_DAMP] = 60; p->glob[G_TAPE] = 16;
    SET(p, 0, SE, 62, SF, 22, SG, 40, SC, 26, P_DEC, 30, P_REL, 8, P_DIST, 72, P_VOL, 127);
    BEAT(p, 0, "x.........x.....|x.......x.x.....|x.........x.....|x.......x...x...", 43);
    SET(p, 1, SA, 70, SC, 44, SD, 56, SE, 36, SF, 18, SG, 40, P_DEC, 30, P_REL, 12, P_BASE, 20, P_EQG, 70, P_SPU, 40,
        P_VOL, 99);
    BEAT(p, 1, "....x..q.x..x...|....x....x..x.q.|....x..q.x..x...|..q.x....x..x.2.", 60);
    SET(p, 2, SA, 110, P_DEC, 10, P_REL, 4, P_BASE, 42, P_VOL, 71, P_PAN, 66);
    BEAT(p, 2, "x.xx.xx.x.xx.xx.|x.xx.xxqx.xx.xx.|x.xx.xx.x.xx.xx.|x.xx.xx.x.x2.xx.", 60);
    SET(p, 3, SA, PCH(3), SB, PCH(7), SC, PCH(10), SD, 20, P_ATK, 70, P_HOLD, 127, P_DEC, 127, P_REL, 80,
        P_GATE, 127, P_WDTH, 32, P_LPQ, 0, P_SPU, 80, P_VOL, 77);
    BEAT(p, 3, "x...............|x...............|x...............|x...............", 43, 43, 48, 48);
    SET(p, 4, SA, VOWEL(0), SB, 70, SC, 50, SD, 64, SE, 30, SF, 60, SG, 30, SH, 0, P_ATK, 10, P_DEC, 50, P_REL, 30,
        P_GATE, 16, P_WDTH, 36, P_DIST, 70, P_SRR, 20, P_DTIM, 48, P_DSND, 92, P_DFB, 40, P_SPU, 60, P_VOL, 126, P_LEVL, 107,
        P_PAN, 60);
    BEAT(p, 4, "......x.......x.|......x.....x...|......x.......x.|......x.....h...", 58, 55, 58, 53, 60, 58, 60, 55);
    SET(p, 5, SE, 0, SC, 40, SG, 0, SH, 0, P_HOLD, 127, P_DEC, 127, P_REL, 24, P_GATE, 24, P_WDTH, 24, P_DIST, 70,
        P_VOL, 70);
    BEAT(p, 5, "x..x......x..x..|x..x......x.....|x..x......x..x..|x..x......x.....",
         31, 31, 43, 31, 31, 31, 43, 36, 36, 48, 36, 36, 36, 43);
    trim(p, 80);

    /* A08 MOTEL, lo-fi house after Anthony Naples: B dorian at 122, loose and swung. A few things: a kick tuned to B, a
     * clap in a small room, open hats sampled down; a muffled, crushed stab, Bm7 then E7sus4, off the beat into a dull
     * echo; and the idea, a bass that walks the chords; tape, hiss and the bus's crunch for the dust */
    p = pat_v5(7, "MOTEL", 122, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_ENS, M_GSIN, M_GSIN));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 22; p->glob[G_TAPE] = 24; p->glob[G_HISS] = 44; p->glob[G_ADPC] = 24;
    p->glob[G_DRIV] = 28; p->glob[G_VERB] = 50; p->glob[G_DAMP] = 78;
    SET(p, 0, SE, 56, SF, 26, SG, 20, SC, 30, P_DEC, 34, P_REL, 10, P_DIST, 70, P_SRR, 10, P_VOL, 127);
    BEAT(p, 0, "x...x...x...x...|x...x...x...x...|x...x...x...x...|x...x...x...x.q.", 35);
    SET(p, 1, SA, 72, SC, 30, SD, 70, SG, 20, P_DEC, 32, P_REL, 14, P_BASE, 24, P_BITS, 24, P_SPU, 70, P_VOL, 106);
    BEAT(p, 1, "....x.......x...|....x.......x...|....x.......x...|....x.......x..q", 60);
    SET(p, 2, SA, 106, SG, 30, P_DEC, 22, P_REL, 10, P_BASE, 38, P_SRR, 30, P_VOL, 73);
    BEAT(p, 2, "..x...x...x...x.|..x...x...x...x.|..x...x...x...x.|..x...x...x.?.x.", 60);
    SET(p, 3, SA, PCH(3), SB, PCH(7), SC, PCH(10), SD, 30, P_DEC, 50, P_REL, 30, P_GATE, 10, P_WDTH, 26, P_LPQ, 0,
        P_BITS, 30, P_SRR, 30, P_DTIM, 64, P_DSND, 34, P_DFB, 50, P_DWID, 30, P_SPU, 50, P_VOL, 124, P_LEVL, 110);
    BEAT(p, 3, ".......x......x.|...x......x.....|.......x......x.|...x......x.....", 47, 47, 47, 47, 52, 52, 52, 52);
    chords(p, 3, 32, 64, 5, 7, 10);                                         /* E7sus4 */
    SET(p, 4, SE, 0, SC, 40, SG, 0, SH, 16, P_HOLD, 127, P_DEC, 127, P_REL, 20, P_GATE, 16, P_WDTH, 32, P_DIST, 70,
        P_VOL, 70);
    BEAT(p, 4, "x..x..x...x..x..|x..x..x...x..x..|x..x..x...x..x..|x..x..x...x..x..",
         35, 35, 42, 35, 38, 35, 35, 42, 40, 38, 40, 40, 47, 40, 45, 40, 40, 47, 45, 42);
    trim(p, 75);

    /* A09 PYLONS, flavour 2, after Boards of Canada: G minor at 84, a slow boom-bap. The idea is the bass: a deep
     * SWAV-SAW that walks under the chords, played an octave over its root so its sine sub sits on G1, Eb1 and C2
     * (the square sub and a little unison under the closed saw for its growl), locked with the kick. Over it Gm9 for two
     * bars, Ebmaj7 over G, Cm7, chorused on a dark pad; a four-note motif, G D C Bb, its last note new every bar, on
     * one of the owner's warm waves, low-passed, worn by the rate and wobbling on the tape; a dusty snare, few hats */
    p = pat_v5(8, "PYLONS", 84, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_ENS, M_SAW, M_WAV));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 20; p->glob[G_TAPE] = 72; p->glob[G_HISS] = 48; p->glob[G_ADPC] = 16;
    p->glob[G_VERB] = 66; p->glob[G_DAMP] = 80; p->glob[G_DRIV] = BANK_DRIVE;
    SET(p, 0, SE, 56, SF, 26, SG, 18, SC, 40, P_DEC, 38, P_REL, 10, P_DIST, 72, P_VOL, 105);
    BEAT(p, 0, "x......x..x.....|x.......x.x.....|x......x..x.....|x.......x.x...q.", 43);
    SET(p, 1, SA, 62, SC, 20, SD, 40, SE, 20, SF, 20, SG, 16, P_DEC, 32, P_REL, 14, P_BASE, 18, P_SRR, 30, P_BITS, 20,
        P_DIST, 72, P_SPU, 50, P_VOL, 84);
    BEAT(p, 1, "....x.......x...|....x.......x..q|....x.......x...|....x.......x.q.", 60);
    SET(p, 2, SA, 100, P_ATK, 10, P_DEC, 14, P_REL, 8, P_BASE, 36, P_SRR, 40, P_BITS, 30, P_VOL, 57, P_PAN, 58);
    BEAT(p, 2, "..x.......x.....|..x.......x...x?|..x.......x.....|..x.......x.x...", 60);
    SET(p, 3, SA, PCH(3), SB, PCH(7), SC, PCH(8), SD, 30, SF, 60, SG, 30, P_ATK, 80, P_HOLD, 127, P_DEC, 127,
        P_REL, 90, P_GATE, 127, P_WDTH, 28, P_LPQ, 0, P_SPU, 80, P_VOL, 91);
    BEAT(p, 3, "x...............|x...............|x...............|x...............", 50, 50, 43, 48);
    chords(p, 3, 32, 48, 3, 7, 8);                                          /* Ebmaj7 over G */
    chords(p, 3, 48, 64, 3, 7, 10);                                         /* Cm7 */
    SET(p, 4, SA, 30, SB, 16, SE, 40, SF, 110, SG, 0, SH, 64, P_ATK, 2, P_HOLD, 127, P_DEC, 127, P_REL, 30,
        P_GATE, 60, P_WDTH, 28, P_LPQ, 0, P_DIST, 72, P_VOL, 127, P_LEVL, 127);
    BEAT(p, 4, "x.........x.....|x.......x.....x.|x.........x.....|x.........x.....", 43, 43, 43, 43, 41, 39, 39, 48, 46);
    SET(p, 5, SA, OS(17), SB, 64, SC, 127, SE, 30, SF, 64, SG, 0, SH, 0, P_WBNK, 127, P_ATK, 12, P_HOLD, 127,
        P_DEC, 127, P_REL, 50, P_GATE, 40, P_WDTH, 26, P_DIST, 64, P_SRR, 24, P_DTIM, 64, P_DSND, 30, P_DFB, 46,
        P_DBAS, 10, P_DWID, 30, P_SPU, 70, P_VOL, 49, P_PAN, 66);
    BEAT(p, 5, "x..x..x...x.....|x..x..x...x.....|x..x..x...x.....|x..x..x...x.....",
         55, 62, 60, 58, 55, 62, 60, 57, 55, 62, 60, 58, 55, 63, 62, 60);
    trim(p, 83);

    /* A10 RIVETS, flavour 1, after Autechre: C# minor at 118, swung. The drums lead, a broken funk with space in it: a
     * hard kick on one and the off-beats, ratcheting into the turn; a short crushed snare on the backbeat with ghost
     * notes round it and a drag before the turn; hats in swung sixteenths, accented on the off-beat eighths, opening
     * every second bar; an FM clank answering the snare, its ratio locked to a brighter strike; a metal rim thrown
     * into a dotted echo. Under it a punchy SWAV-SAW bass in 3 3 4 3 3, on the kick and between, jumping octaves,
     * played an octave over its root so its sub sits on C#1 to C#2 (35-70 Hz); no melody */
    p = pat_v5(9, "RIVETS", 118, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_FMD, M_SAW, M_GNOI));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 24; p->glob[G_GLCH] = 20; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_ADPC] = 24;
    p->glob[G_VERB] = 30; p->glob[G_DAMP] = 50; p->glob[G_ACNT] = 50; p->glob[G_DRIV] = BANK_DRIVE;
    SET(p, 0, SB, 16, SE, 64, SF, 18, SG, 60, SC, 50, P_DEC, 30, P_REL, 6, P_DIST, 80, P_VOL, 111);
    BEAT(p, 0, "X.....x...x.....|X.....x.x.....x.|X.....x...x..x..|X.....x.x...2.x.", 37);
    SET(p, 1, SA, 108, SG, 60, P_DEC, 16, P_REL, 6, P_BASE, 30, P_DIST, 76, P_BITS, 40, P_SRR, 36, P_VOL, 98);
    BEAT(p, 1, "....X..x....X...|....X.....x.X..x|....X..x....X.?.|....X..x..x.X.3.", 60);
    static const uint8_t ghost[] = {7, 26, 31, 39, 46, 55, 58};
    for (unsigned i = 0; i < sizeof ghost; i++) lk(p, 1, ghost[i], P_VOL, 56);   /* the ghosts, 9 dB down */
    SET(p, 2, SA, 116, SG, 90, P_DEC, 6, P_REL, 4, P_BASE, 44, P_BITS, 50, P_SRR, 50, P_VOL, 74, P_PAN, 70);
    BEAT(p, 2, "x?X.x?X.x?X.x?Xx|x?X.x?X.x?X.x?X2|x?X.x?X.x?X.x?Xx|x?X.x?X.x2X.x?X4", 60);
    lk(p, 2, 30, P_DEC, 30); lk(p, 2, 62, P_DEC, 30);                       /* open, every second bar */
    SET(p, 3, SA, RATIO(13), SB, 80, SC, 16, SD, RATIO(26), SE, 40, SF, 10, SG, 40, SH, 100, P_DEC, 14, P_REL, 4,
        P_GATE, 2, P_DIST, 76, P_SPU, 20, P_VOL, 65, P_PAN, 44);
    BEAT(p, 3, ".......x.......x|...x.......x..2.|.......x.......x|...x...x...3..x.", 49);
    lk(p, 3, 7, SA, RATIO(17)); lk(p, 3, 39, SA, RATIO(17));               /* the brighter strike */
    lfo(p, 3, 0, SB, 6, 2, 48, 80, 0);
    SET(p, 4, SA, 20, SB, 24, SE, 60, SF, 110, SG, 0, SH, 64, P_ATK, 0, P_HOLD, 8, P_DEC, 44, P_REL, 14, P_GATE, 4,
        P_WDTH, 22, P_LPQ, 0, P_FDEC, 30, P_WOF, 92, P_DIST, 76, P_VOL, 127, P_LEVL, 127);
    BEAT(p, 4, "x..x..x...x..x..|x..x..x...x...x.|x..x..x...x..x..|x..x..x.x.x..x.x",
         37, 49, 37, 40, 42, 37, 49, 37, 44, 42, 37, 49, 37, 40, 47, 37, 49, 37, 40, 42, 44, 47);
    SET(p, 5, SA, 90, SC, 100, SD, 90, SF, 10, P_DEC, 10, P_REL, 4, P_BASE, 32, P_DIST, 76, P_BITS, 40, P_SRR, 40,
        P_DTIM, 48, P_DSND, 40, P_DFB, 40, P_DWID, 30, P_SPU, 30, P_VOL, 79, P_PAN, 40);
    BEAT(p, 5, "..........x.....|...x..........x.|..........x.....|...x......x...x.", 72);
    trim(p, 84);

    /* A11 SYRUP: warm IDM in A, worn tape: a detuned pad through Amaj7 F#m9 Dmaj7 E6/9, a nostalgic bell melody
     * in echoes, a soft sine bass; lazy drums, a bitten snare; the tape wobbles */
    p = pat_v5(10, "SYRUP", 96, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_ENS, M_FMS, M_GSIN));
    p->glob[G_LEN] = 64; p->glob[G_TAPE] = 50; p->glob[G_HISS] = 14; p->glob[G_VERB] = 70; p->glob[G_ADPC] = 10;
    p->glob[G_SWNG] = 24; p->glob[G_DAMP] = 60;
    SET(p, 0, SE, 60, SF, 34, SG, 20, SC, 30, P_DEC, 40, P_REL, 10, P_VOL, 127);
    BEAT(p, 0, "x.......x.x.....|x......x..x.....|x.......x.x.....|x......x..x..q..", 36);
    SET(p, 1, SA, 66, SC, 30, P_DEC, 30, P_REL, 14, P_BASE, 20, P_BITS, 20, P_SPU, 60, P_VOL, 127, P_LEVL, 103);
    BEAT(p, 1, "....x.......x...|....x.......x..q|....x.......x...|....x..q....x...", 60);
    SET(p, 2, SA, 96, P_DEC, 18, P_REL, 8, P_BASE, 32, P_VOL, 64, P_PAN, 58);
    BEAT(p, 2, "x.x.x.xqx.x.x.xq|x.x.x.xqx.x.x.x.|x.x.x.xqx.x.x.xq|x.x.xqxqx.x.x.x.", 60);
    SET(p, 3, SA, SEMI(4), SB, SEMI(7), SC, SEMI(11), SD, 70, SE, 20, SG, 100, SH, 20, P_ATK, 74, P_HOLD, 127,
        P_DEC, 127, P_REL, 90, P_GATE, 127, P_WDTH, 34, P_LPQ, 16, P_SPU, 80, P_VOL, 66);
    BEAT(p, 3, "x...............|x...............|x...............|x...............", 57, 54, 50, 52);
    lk(p, 3, 16, SA, SEMI(3)); lk(p, 3, 16, SB, SEMI(10)); lk(p, 3, 16, SC, SEMI(14));   /* F#m9 */
    lk(p, 3, 48, SB, SEMI(9)); lk(p, 3, 48, SC, SEMI(14));                                /* E6/9 */
    MOD(p, 3, 0, 14, 100, 70);
    SET(p, 4, SA, RATIO(10), SB, 40, SC, 60, SD, RATIO(8), SE, 24, SF, 40, SH, MODE4(0), P_DEC, 76, P_REL, 60,
        P_GATE, 20, P_DTIM, 64, P_DSND, 40, P_DFB, 40, P_SPU, 70, P_VOL, 51, P_PAN, 70);
    BEAT(p, 4, "x.....x...x.....|x.......x.......|x.....x...x.....|x...x...x.......", 76, 73, 71, 69, 66, 76, 78, 81, 80, 76, 71);
    SET(p, 5, SB, 6, SC, 20, SH, 20, P_HOLD, 127, P_DEC, 127, P_REL, 30, P_GATE, 40, P_WDTH, 26, P_VOL, 64);
    BEAT(p, 5, "x.....x...x.....|x.....x...x.....|x.....x...x.....|x.....x...x...x.", 45, 45, 52, 42, 42, 49, 38, 38, 45, 40, 40, 47, 44);
    trim(p, 74);

    /* A12 SPU: a PlayStation menu at night, C major: a big slow pad through Cmaj9 Am9 Fmaj7 G6, glass bells in
     * eighths, a choir holding the chord tones, an FM pluck answering, a sub; a brush; the SPU's reverb and grain */
    p = pat_v5(11, "SPU", 84, MACHINES(M_GNOI, M_ENS, M_WAV, M_VOX, M_FMS, M_GSIN));
    p->glob[G_LEN] = 64; p->glob[G_ADPC] = 50; p->glob[G_VERB] = 110; p->glob[G_DAMP] = 40; p->glob[G_TAPE] = 10;
    SET(p, 0, SA, 100, P_DEC, 18, P_REL, 8, P_BASE, 32, P_SPU, 60, P_VOL, 22, P_PAN, 40);
    BEAT(p, 0, "x.q.x.q.x.q.x.qx|x.q.x.q.x.q.x.q.|x.q.x.q.x.q.x.qx|x.q.x.q.x.qxx.q.", 60);
    SET(p, 1, SA, SEMI(4), SB, SEMI(11), SC, SEMI(14), SD, 60, SE, 30, SG, 100, SH, 10, P_ATK, 90, P_HOLD, 127,
        P_DEC, 127, P_REL, 100, P_GATE, 127, P_WDTH, 36, P_LPQ, 10, P_WOF, 76, P_FATK, 100, P_FDEC, 100, P_SPU, 110,
        P_VOL, 56);
    BEAT(p, 1, "x...............|x...............|x...............|x...............", 48, 45, 53, 55);
    lk(p, 1, 16, SA, SEMI(3)); lk(p, 1, 16, SB, SEMI(10));                                /* Am9 */
    lk(p, 1, 32, SB, SEMI(7)); lk(p, 1, 32, SC, SEMI(11));                                /* Fmaj7 */
    lk(p, 1, 48, SB, SEMI(7)); lk(p, 1, 48, SC, SEMI(9));                                 /* G6 */
    MOD(p, 1, 0, 12, 100, 70);
    SET(p, 2, SA, WAVE(5), SB, 70, SC, 50, SE, 30, SF, 66, SG, 60, P_DEC, 60, P_REL, 50, P_GATE, 10, P_DTIM, 96,
        P_DSND, 94, P_DFB, 50, P_SPU, 100, P_VOL, 58, P_PAN, 70);
    BEAT(p, 2, "x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.",
         72, 76, 79, 83, 86, 83, 79, 76, 69, 72, 76, 79, 83, 79, 76, 72, 65, 69, 72, 76, 79, 76, 72, 69, 67, 71, 74, 76, 79, 76, 74, 71);
    SET(p, 3, SA, VOWEL(6), SB, 64, SC, 80, SE, 50, SF, 60, SG, 20, SH, 40, P_ATK, 80, P_HOLD, 127, P_DEC, 127,
        P_REL, 90, P_GATE, 127, P_SPU, 120, P_VOL, 46);
    BEAT(p, 3, "x...............|x...............|x...............|x...............", 64, 64, 69, 67);
    SET(p, 4, SA, RATIO(6), SB, 40, SC, 40, SD, RATIO(9), SE, 20, SF, 30, SH, MODE4(0), P_DEC, 56, P_REL, 40,
        P_GATE, 10, P_DTIM, 64, P_DSND, 40, P_DFB, 40, P_SPU, 80, P_VOL, 86, P_PAN, 56);
    BEAT(p, 4, "......x.......x.|..........x.....|......x.......x.|..........x...x.", 79, 76, 76, 81, 79, 74, 72);
    SET(p, 5, SC, 30, SH, 10, P_HOLD, 127, P_DEC, 127, P_REL, 60, P_GATE, 127, P_WDTH, 22, P_VOL, 65);
    BEAT(p, 5, "x...............|x...............|x...............|x.......x.......", 36, 33, 29, 31, 38);
    trim(p, 87);

    /* A13 GROOVE: dub techno that swings, G minor: a minor-ninth stab answering the kick, thrown into the echo,
     * turning to Ebmaj7 at the end; a SID bass that walks, a pitched hand drum, hats opening, a clap */
    p = pat_v5(12, "GROOVE", 124, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_ENS, M_SID, M_GNOI));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 30; p->glob[G_VERB] = 60; p->glob[G_DAMP] = 70; p->glob[G_HISS] = 10;
    SET(p, 0, SE, 70, SF, 30, SG, 30, SC, 30, P_DEC, 36, P_REL, 10, P_VOL, 127);
    BEAT(p, 0, "x...x...x...x...|x...x...x...x...|x...x...x...x...|x...x...x...x.x.", 36);
    SET(p, 1, SA, 72, SC, 30, P_DEC, 30, P_REL, 14, P_BASE, 20, P_SPU, 60, P_VOL, 85, P_PAN, 60);
    BEAT(p, 1, "....x.......x...|....x.......x..q|....x.......x...|....x..q....x.x.", 60);
    SET(p, 2, SA, 104, P_DEC, 14, P_REL, 8, P_BASE, 34, P_VOL, 48, P_PAN, 52);
    BEAT(p, 2, "xqXqxqXqxqXqxqXq|xqXqxqXqxqXqxqXq|xqXqxqXqxqXqxqXq|xqXqxqXqxqXqxqXx", 60);
    for (int st = 2; st < 64; st += 4) lk(p, 2, st, P_DEC, 36);
    SET(p, 3, SA, SEMI(3), SB, SEMI(10), SC, SEMI(14), SD, 50, SE, 10, SG, 100, P_DEC, 36, P_REL, 20, P_GATE, 6,
        P_WDTH, 36, P_LPQ, 30, P_WOF, 82, P_FDEC, 28, P_DTIM, 48, P_DSND, 96, P_DFB, 50, P_DBAS, 40, P_DWID, 40,
        P_SPU, 60, P_VOL, 127, P_LEVL, 105);
    BEAT(p, 3, "..x.....x..x....|..x.....x..x..x.|..x.....x..x....|..x.....x..x.x..", 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 51, 51, 51);
    for (int st = 56; st < 64; st++) if (p->step[3][st].flags) { lk(p, 3, st, SA, SEMI(4)); lk(p, 3, st, SB, SEMI(7)); lk(p, 3, st, SC, SEMI(11)); }
    lfo(p, 3, 0, P_WDTH, 0, 0, 8, 74, 0);
    SET(p, 4, SA, SIDW(2), SB, 30, SC, 30, SD, SIDW(1), SE, 32, SF, 64, SG, MODE4(0), SH, 50, P_DEC, 34, P_REL, 10,
        P_GATE, 10, P_PORT, 14, P_WDTH, 26, P_LPQ, 40, P_WOF, 82, P_FDEC, 30, P_DIST, 72, P_VOL, 126);
    BEAT(p, 4, "x..x..x...x.x..x|x..x..x...x.x...|x..x..x...x.x..x|x..x..x...x.x.xs",
         43, 43, 55, 43, 46, 50, 43, 43, 55, 46, 41, 43, 43, 55, 43, 46, 50, 39, 39, 51, 39, 43, 46, 41);
    SET(p, 5, SA, 50, SB, 40, SC, 20, P_DEC, 32, P_REL, 12, P_BASE, 8, P_SPU, 40, P_VOL, 62, P_PAN, 80);
    BEAT(p, 5, "...x..x....x.x..|...x..x....x..x.|...x..x....x.x..|...x..x.x..x.x.x", 60, 67, 63, 67, 60, 67, 63, 60, 60, 67, 63, 67, 60, 67, 63, 67, 60, 63);
    trim(p, 70);

    /* A14 ORCHARD, flavour 2, after Boards of Canada: E dorian at 92, bouncing. The idea is a four-note call, G A B
     * and down to E, answered in the last bar by the dorian C#, on one of the owner's warm waves, worn and wobbling, its
     * echo answering it; under it Em7 over G for two bars, A7, Em over B on a chorused pad; a deep sustained sine bass, its
     * sub an octave under on E1, A1 and B1, driven for its harmonics and locked with the kick; a slow swung beat with a
     * ghost before the snare */
    p = pat_v5(13, "ORCHARD", 92, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_ENS, M_GSIN, M_WAV));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 24; p->glob[G_TAPE] = 76; p->glob[G_HISS] = 52; p->glob[G_ADPC] = 20;
    p->glob[G_VERB] = 70; p->glob[G_DAMP] = 80; p->glob[G_DRIV] = BANK_DRIVE;
    SET(p, 0, SE, 56, SF, 26, SG, 20, SC, 40, P_DEC, 40, P_REL, 10, P_DIST, 72, P_VOL, 120);
    BEAT(p, 0, "x......x.x......|x.......x.....x.|x......x.x......|x.......x...q...", 40);
    SET(p, 1, SA, 60, SC, 30, SD, 40, SE, 20, SF, 20, SG, 20, P_DEC, 32, P_REL, 14, P_BASE, 18, P_SRR, 30, P_BITS, 24,
        P_DIST, 72, P_SPU, 50, P_VOL, 113);
    BEAT(p, 1, "...qx.......x...|...qx.......x..q|...qx.......x...|...qx.......x...", 60);
    SET(p, 2, SA, 100, P_ATK, 10, P_DEC, 12, P_REL, 8, P_BASE, 36, P_SRR, 40, P_BITS, 30, P_VOL, 59, P_PAN, 60);
    BEAT(p, 2, "x.x?x.x.x.x?x.x.|x.x?x.x.x.x?x.x?|x.x?x.x.x.x?x.x.|x.x?x.x.x.x?xqx.", 60);
    SET(p, 3, SA, PCH(4), SB, PCH(7), SC, PCH(9), SD, 30, SF, 60, SG, 30, P_ATK, 80, P_HOLD, 127, P_DEC, 127,
        P_REL, 90, P_GATE, 127, P_WDTH, 28, P_LPQ, 0, P_SPU, 80, P_VOL, 109);
    BEAT(p, 3, "x...............|x...............|x...............|x...............", 43, 43, 45, 47);
    chords(p, 3, 32, 48, 4, 7, 10);                                         /* A7 */
    chords(p, 3, 48, 64, 3, 5, 8);                                          /* Em over B */
    SET(p, 4, SE, 0, SC, 64, SG, 0, SH, 10, P_HOLD, 127, P_DEC, 127, P_REL, 40, P_GATE, 60, P_WDTH, 24, P_DIST, 72,
        P_VOL, 106);
    BEAT(p, 4, "x......x........|x.......x.......|x......x........|x.......x.....x.", 40, 40, 40, 40, 45, 45, 47, 47, 43);
    SET(p, 5, SA, OS(40), SB, 64, SC, 127, SE, 30, SF, 64, SG, 0, SH, 0, P_WBNK, 127, P_ATK, 14, P_HOLD, 127,
        P_DEC, 127, P_REL, 50, P_GATE, 40, P_WDTH, 28, P_DIST, 64, P_SRR, 24, P_DTIM, 96, P_DSND, 32, P_DFB, 50,
        P_DBAS, 10, P_DWID, 30, P_SPU, 70, P_VOL, 89, P_PAN, 64);
    BEAT(p, 5, "x.x.x.....x.....|x.x.x.....x.....|x.x.x.....x.....|x.x.x.....x.....",
         55, 57, 59, 52, 55, 57, 59, 52, 55, 57, 59, 52, 55, 57, 61, 59);
    trim(p, 62);

    /* A15 SPLINTR, flavour 1, after Aphex Twin (drill'n'bass): F minor at 160. The drums lead: a kick and a snare
     * ratcheting and rolling, the snare's tonal body ringing in its low-pass three octaves up and pitched an octave on
     * some retrigs, crushed on others; hats in thirty-seconds ground to 8 bits; a metal rim flicking between them; and
     * every second loop the drums drop out of the last two beats. Under it a huge sustained SWAV-SAW bass with a little
     * unison (its detune the point), played an octave over its root so its subs sit on F1, Ab1, Db1 and Eb1; once a
     * loop, over the F, a three-note dark hook, Ab G F */
    p = pat_v5(14, "SPLINTR", 160, MACHINES(M_GSIN, M_GSIN, M_GNOI, M_GNOI, M_SAW, M_WAV));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 0; p->glob[G_GLCH] = 32; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_ADPC] = 24;
    p->glob[G_VERB] = 40; p->glob[G_DAMP] = 50; p->glob[G_ACNT] = 60; p->glob[G_DRIV] = BANK_DRIVE;
    SET(p, 0, SB, 20, SE, 64, SF, 16, SG, 70, SC, 60, P_DEC, 32, P_REL, 6, P_DIST, 88, P_SRR, 24, P_VOL, 127);
    BEAT(p, 0, "X..x..2...x..x..|X.x...x...3...x.|X..x..2...x..x..|X...x.x...h.h.h.", 41);
    SET(p, 1, SB, 20, SE, 30, SF, 14, SD, 110, SG, 40, SC, 0, P_DEC, 24, P_REL, 8, P_WDTH, 40, P_LPQ, 50, P_DIST, 86,
        P_BITS, 30, P_SRR, 30, P_SPU, 30, P_VOL, 100);
    BEAT(p, 1, "....X..x.2..X..3|..x.X...x.3.X.4.|....X..x.2..X..3|..2.X.x.h.h.hhhh", 60);
    lk(p, 1, 15, P_PTCH, 76); lk(p, 1, 30, P_PTCH, 76); lk(p, 1, 47, P_SRR, 80); lk(p, 1, 41, P_PTCH, 52);
    lk(p, 1, 9, P_SRR, 70);
    SET(p, 2, SA, 116, SG, 80, P_DEC, 6, P_REL, 4, P_BASE, 46, P_BITS, 50, P_SRR, 40, P_VOL, 85, P_PAN, 70);
    BEAT(p, 2, "x2x2x3x2x2x3x2x4|x2x3x2x2x3x2x2x2|x2x2x3x2x2x3x2x4|x2x3x2x2hhhhhhhh", 60);
    SET(p, 3, SA, 90, SC, 100, SD, 90, SF, 10, P_DEC, 10, P_REL, 4, P_BASE, 30, P_DIST, 76, P_SPU, 30, P_VOL, 65,
        P_PAN, 40);
    BEAT(p, 3, ".x....x..x...x..|...x..x.....x..x|.x....x..x...x..|...x..x.....hh..", 72);
    SET(p, 4, SA, 60, SB, 20, SC, 40, SE, 60, SF, 100, SG, 0, SH, 64, P_ATK, 2, P_HOLD, 127, P_DEC, 127, P_REL, 30,
        P_GATE, 127, P_WDTH, 28, P_LPQ, 0, P_DIST, 75, P_VOL, 127);
    BEAT(p, 4, "x...............|x.......x.......|x...............|x.......x.......", 41, 41, 44, 41, 37, 39);
    SET(p, 5, SA, OS(62), SB, 64, SC, 127, SE, 40, SF, 64, SG, 0, SH, 0, P_WBNK, 127, P_ATK, 10, P_HOLD, 127,
        P_DEC, 127, P_REL, 50, P_GATE, 48, P_WDTH, 28, P_DIST, 69, P_SRR, 30, P_DTIM, 48, P_DSND, 34, P_DFB, 50,
        P_DWID, 30, P_SPU, 50, P_VOL, 65);
    BEAT(p, 5, "................|................|..x...x...x.....|................", 56, 55, 53);
    trim(p, 52);
    p->kit[4][P_LEVL] = 97;               /* the bass over the trim: SWAV-SAW clips 10.5 dB under GND-SIN */

    /* A16 FOUNDRY, flavour 2, after Boards of Canada: the progression of "Kaini Industries", from the owner's MIDI,
     * its bass notes too (B, C#, G#, C#); the rest is MONO2K's own. G# dorian at 102. The idea is a lead bass: a deep,
     * gritty SWAV-SAW running a four-bar line from the first step over the original's four bars (G#m7 over B,
     * C#sus4, G#m9, C#9), one rhythm a bar, sliding into some notes, each bar on the progression's bass note, played an
     * octave over its root so its sub sits on C#1 to D#2, its filter moving; the chords rootless, swelling on a chorused
     * pad, driven, crushed and worn by the tape. The drums never quite repeat: a hard kick; a crushed snare whose length
     * and crack change, ghosts round it, its rate drifting; short hats in swung sixteenths whose decay wanders, metal
     * ticks and pitched blips among them; and a gritty shaker and metal percussion thrown into a dotted echo */
    p = pat_v5(15, "FOUNDRY", 102, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_ENS, M_SAW, M_GNOI));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 22; p->glob[G_TAPE] = 76; p->glob[G_HISS] = 60; p->glob[G_ADPC] = 40;
    p->glob[G_GLCH] = 16; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_VERB] = 56; p->glob[G_DAMP] = 76; p->glob[G_ACNT] = 50;
    p->glob[G_DRIV] = BANK_DRIVE;
    SET(p, 0, SE, 64, SF, 20, SG, 40, SC, 40, P_DEC, 32, P_REL, 6, P_DIST, 82, P_SRR, 12, P_VOL, 113);
    BEAT(p, 0, "X.....x...x.....|x.........X.x...|x.....x...x.....|x.........X.....", 44);
    SET(p, 1, SA, 92, SC, 40, SD, 60, SG, 50, P_DEC, 18, P_REL, 6, P_BASE, 24, P_DIST, 78, P_BITS, 30, P_SRR, 30,
        P_SPU, 40, P_VOL, 90);
    BEAT(p, 1, "....X..x....X...|....X.......X..x|....X..x....X...|....X..x....X.x.", 60);
    static const uint8_t ghost16[] = {7, 31, 39, 55, 62};
    for (unsigned i = 0; i < sizeof ghost16; i++) lk(p, 1, ghost16[i], P_VOL, 50);   /* the ghosts, 9 dB down */
    lk(p, 1, 28, P_DEC, 30); lk(p, 1, 60, P_DEC, 30);
    lk(p, 1, 44, SC, 100); lk(p, 1, 44, SD, 90);
    lfo(p, 1, 0, P_SRR, 6, 3, 40, 80, 0);
    SET(p, 2, SA, 116, SG, 80, P_ATK, 0, P_DEC, 4, P_REL, 3, P_BASE, 44, P_SRR, 44, P_BITS, 44, P_VOL, 57, P_PAN, 58);
    BEAT(p, 2, "x?X.x?X.x?X.x?X2|x?X.x?X.x?X3x?X.|x?X.x?X.x?X.x?X2|x?X.x?X.x2X.x?X4", 60);
    lfo(p, 2, 0, P_DEC, 6, 3, 56, 84, 0);
    lfo(p, 2, 1, P_PAN, 0, 1, 20, 84, 0);
    for (int st = 9; st < 64; st += 18) { lk(p, 2, st, SC, 110); lk(p, 2, st, SD, 90); }
    lk(p, 2, 19, SB, 70); lk(p, 2, 51, SB, 76);
    SET(p, 3, SA, PCH(4), SB, PCH(7), SC, PCH(9), SD, 40, SF, 50, SG, 30, P_ATK, 60, P_HOLD, 127, P_DEC, 127,
        P_REL, 70, P_GATE, 127, P_WDTH, 30, P_LPQ, 0, P_DIST, 74, P_SRR, 20, P_BITS, 16, P_SPU, 60, P_VOL, 103);
    BEAT(p, 3, "x...............|x...............|x...............|x...............", 47, 49, 51, 53);
    chord(p, 3, 16, 5, 7, 10);                                              /* C#sus4 (7) */
    chord(p, 3, 32, 3, 5, 7);                                               /* G#m9, no third: D# F# G# A# */
    chord(p, 3, 48, 3, 6, 10);                                              /* C#9, rootless: F G# B D# */
    lfo(p, 3, 0, P_WDTH, 1, 0, 24, 74, 0);
    SET(p, 4, SA, 40, SB, 16, SE, 40, SF, 110, SG, 0, SH, 64, P_ATK, 0, P_HOLD, 127, P_DEC, 127, P_REL, 20,
        P_GATE, 30, P_WDTH, 34, P_LPQ, 0, P_DIST, 78, P_VOL, 127, P_LEVL, 127);
    BEAT(p, 4, "x..x..x.x...x.x.|x..x..x.s...x.x.|x..x..x.x...x.x.|x..x..x.s...x.x.",
         47, 44, 51, 49, 47, 42, 37, 44, 47, 49, 44, 42, 44, 51, 46, 49, 51, 44, 37, 41, 44, 47, 51, 49);
    for (int st = 0; st < 64; st += 16) lk(p, 4, st, P_GATE, 90);           /* each bar's bass note held */
    lfo(p, 4, 0, P_WDTH, 1, 1, 16, 74, 0);
    SET(p, 5, SA, 110, SC, 60, SD, 70, SE, 30, SF, 10, SG, 70, P_DEC, 8, P_REL, 4, P_BASE, 40, P_BITS, 40, P_SRR, 40,
        P_DTIM, 48, P_DSND, 36, P_DFB, 36, P_DWID, 30, P_SPU, 30, P_VOL, 86, P_PAN, 40);
    BEAT(p, 5, "..x..x?...x.x..x|.?x..x...xx...x.|..x..x?...x.x..x|.?x..x..x.x.2.x.", 72);
    lfo(p, 5, 0, SC, 6, 3, 48, 90, 0);                                      /* shaker to metal, hit to hit */
    lfo(p, 5, 1, P_PAN, 0, 1, 14, 90, 0);
    trim(p, 74);

    /* A11 A12 A13 A16, still the bank before (ea69d53), were written on SWAV-ENS's old page and the bus's old laws:
     * onto today's, once (every other part is written on today's) */
    static const uint8_t OLD_SLOTS[] = {10, 11, 12};
    for (unsigned i = 0; i < sizeof OLD_SLOTS; i++) {
        ens_v6(&pat[OLD_SLOTS[i]]);
        bus_v9(&pat[OLD_SLOTS[i]]);
    }
}
