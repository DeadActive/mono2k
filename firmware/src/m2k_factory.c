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
    p->glob[G_VERB] = 74; p->glob[G_DAMP] = 80; p->glob[G_DRIV] = 24;
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
        P_DEC, 127, P_REL, 60, P_GATE, 56, P_WDTH, 24, P_DIST, 74, P_SRR, 30, P_DTIM, 64, P_DSND, 30, P_DFB, 50,
        P_DBAS, 10, P_DWID, 30, P_SPU, 70, P_VOL, 48, P_PAN, 70);
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
        P_DEC, 127, P_REL, 50, P_GATE, 48, P_WDTH, 22, P_DIST, 76, P_SRR, 40, P_DTIM, 72, P_DSND, 34, P_DFB, 52,
        P_DBAS, 20, P_DWID, 30, P_SPU, 50, P_VOL, 76, P_PAN, 80);
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
        P_DEC, 127, P_REL, 50, P_GATE, 64, P_WDTH, 28, P_DIST, 74, P_SRR, 20, P_DTIM, 64, P_DSND, 34, P_DFB, 46,
        P_DBAS, 10, P_DWID, 40, P_SPU, 70, P_VOL, 55, P_PAN, 56);   /* osc 2 one step (12.5 c) sharp */
    BEAT(p, 3, "x.......x...x...|....x...........|x...........x...|............h...|", 60, 58, 56, 53, 56, 53, 51);
    SET(p, 4, SA, PCH(3), SB, PCH(7), SC, PCH(10), SD, 40, P_ATK, 92, P_HOLD, 127, P_DEC, 127, P_REL, 92,
        P_GATE, 127, P_WDTH, 28, P_LPQ, 0, P_SPU, 90, P_VOL, 72);
    BEAT(p, 4, "x.......x.......|x.......x.......|x.......x.......|x.......x.......", 53);
    chords(p, 4, 32, 64, 3, 8, 12);                                         /* Db over F */
    SET(p, 5, SE, 0, SC, 40, SG, 0, SH, 10, P_HOLD, 127, P_DEC, 127, P_REL, 20, P_GATE, 60, P_WDTH, 24, P_VOL, 67);
    BEAT(p, 5, "x.........x.....|x.........x.....|x.........x.....|x.........x.....", 41);
    trim(p, 77);

    /* A04 FACETS, after Aleksi Perälä: raw digital ambient in A minor at 124. The idea is one low machine pulse on one
     * of the owner's buzzing waves, eighths on A2 with a fifth or a seventh now and then, some on chance, its wave
     * read in coarse steps and crushed to 9 bits, its low-pass and its place in the field moving at random; under it a
     * crushed, dark pad, Am(add9) going to Dm7, a deep sub; a kick that keeps time, digital ticks crackling on chance */
    p = pat_v5(3, "FACETS", 124, MACHINES(M_GSIN, M_GNOI, M_WAV, M_ENS, M_GSIN, M_GSIN));
    p->glob[G_LEN] = 64; p->glob[G_SWNG] = 6; p->glob[G_VERB] = 70; p->glob[G_DAMP] = 70; p->glob[G_ADPC] = 30;
    p->glob[G_DRIV] = 32; p->glob[G_GLCH] = 16; p->glob[G_GSIZ] = GSIZE(2);
    SET(p, 0, SE, 56, SF, 24, SG, 24, SC, 24, P_DEC, 32, P_REL, 10, P_DIST, 72, P_SPU, 16, P_VOL, 127);
    BEAT(p, 0, "x.......x.......|x.......x.......|x.......x.......|x.......x...q...", 33);
    SET(p, 1, SA, 100, SB, 80, SG, 80, P_DEC, 6, P_REL, 4, P_BASE, 40, P_SPU, 30, P_VOL, 91);
    BEAT(p, 1, "..q.?..e..q.?...|?...q..e?...q..2|..q.?..e..q.?...|?..eq..e?..3q...", 60);
    lfo(p, 1, 0, P_PAN, 6, 3, 64, 110, 0);
    lfo(p, 1, 1, SB, 6, 2, 40, 90, 0);
    SET(p, 2, SA, OS(44), SB, 64, SC, 127, SE, 70, SF, 64, SG, 0, SH, 50, P_WBNK, 127, P_DEC, 50, P_REL, 20, P_GATE, 8,
        P_WDTH, 56, P_DIST, 76, P_SRR, 40, P_DTIM, 48, P_DSND, 36, P_DFB, 44, P_DBAS, 10, P_DWID, 30, P_SPU, 50,
        P_VOL, 79, P_PAN, 40);
    BEAT(p, 2, "x.x.x.Q.x.x.?.x.|x.x.x.Q.x.?.x.Q.|x.x.x.Q.x.x.?.x.|x.x.x.Q.x.?.x.Q.",
         45, 45, 52, 45, 45, 45, 55, 45, 45, 45, 52, 45, 45, 43, 45, 45,
         45, 45, 50, 45, 45, 45, 43, 45, 45, 45, 50, 45, 45, 43, 45, 45);
    lfo(p, 2, 0, P_WDTH, 6, 2, 24, 70, 0);        /* its low-pass at random, 3 to 7 octaves up (lower bent it) */
    lfo(p, 2, 1, P_PAN, 6, 2, 32, 96, 0);
    SET(p, 3, SA, PCH(2), SB, PCH(3), SC, PCH(7), SD, 50, P_ATK, 90, P_HOLD, 127, P_DEC, 127, P_REL, 96,
        P_GATE, 127, P_WDTH, 28, P_LPQ, 0, P_BITS, 40, P_SRR, 30, P_SPU, 80, P_VOL, 78);
    BEAT(p, 3, "x.......x.......|x.......x.......|x.......x.......|x.......x.......", 45, 45, 45, 45, 50, 50, 50, 50);
    chords(p, 3, 32, 64, 3, 7, 10);                                         /* Dm7 */
    lfo(p, 3, 0, P_WDTH, 1, 0, 8, 70, 0);
    SET(p, 4, SE, 0, SB, 20, SC, 40, SG, 0, SH, 0, P_HOLD, 127, P_DEC, 127, P_REL, 30, P_GATE, 60, P_WDTH, 24,
        P_DIST, 72, P_VOL, 118);
    BEAT(p, 4, "x...............|x.........x.....|x...............|x.........x.....", 33, 33, 40, 38, 38, 33);
    trim(p, 78);

    /* A05 GLITCH: glitch hop in C minor: a kick that stutters, a snare crushed to bits, pitched ticks scattered across
     * the field, 8-bit chord chops in sixteenth echoes, a SID bass that slides, an FM lead that ratchets; the bus
     * repeats in 32nds and crunches */
    p = pat_v5(4, "GLITCH", 96, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_WAV, M_SID, M_FMS));
    p->glob[G_LEN] = 64; p->glob[G_GLCH] = 60; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_ADPC] = 30; p->glob[G_DRIV] = 20;
    p->glob[G_VERB] = 50; p->glob[G_SWNG] = 20;
    SET(p, 0, SE, 70, SF, 30, SG, 50, SC, 20, P_DEC, 40, P_REL, 10, P_DIST, 72, P_VOL, 127);
    BEAT(p, 0, "x.......x.x..2..|x......x..x.....|x.......x.x..3..|x..x....x.x.4...", 36);
    SET(p, 1, SA, 60, SG, 80, P_DEC, 38, P_REL, 14, P_BITS, 30, P_XOR, 20, P_SPU, 40, P_VOL, 107);
    BEAT(p, 1, "....x.......x...|....x..q....x..2|....x.......x...|....x.4..x.3x..x", 60);
    SET(p, 2, SA, 90, SB, 80, P_DEC, 18, P_REL, 8, P_BASE, 16, P_VOL, 85);
    BEAT(p, 2, "x?x?2.x?xq3?x.x4|x?x?x.x?2q3?x.x?|x?x?2.x?xq3?x.x4|x?2?x.x?xq3?x4x?", 60);
    lfo(p, 2, 0, P_PAN, 6, 3, 64, 100, 0);
    SET(p, 3, SA, WAVE(26), SB, 64, SE, 40, SH, 60, P_DEC, 36, P_REL, 20, P_GATE, 8, P_WDTH, 48, P_LPQ, 30, P_WOF, 80,
        P_FDEC, 30, P_DTIM, 16, P_DSND, 96, P_DFB, 40, P_SPU, 40, P_VOL, 92);
    BEAT(p, 3, "x..x..x...x..x..|x..x..x...x.xx..|x..x..x...x..x..|x..x..x...xx.x.x",
         60, 63, 67, 63, 60, 56, 60, 63, 60, 56, 63, 63, 67, 70, 67, 63, 58, 62, 65, 62, 58, 65, 70);
    SET(p, 4, SA, SIDW(2), SB, 30, SC, 40, SD, SIDW(1), SE, 64, SF, 66, SG, MODE4(0), SH, 50, P_DEC, 36, P_REL, 10,
        P_GATE, 12, P_PORT, 20, P_WDTH, 26, P_LPQ, 40, P_WOF, 82, P_FDEC, 30, P_DIST, 74, P_VOL, 126, P_LEVL, 103);
    BEAT(p, 4, "x...x.s.x...x..x|x...x.s.x..xx...|x...x.s.x...x..x|x..xx.s.x.x.x...",
         36, 36, 39, 36, 43, 48, 32, 32, 36, 32, 39, 44, 39, 39, 43, 39, 46, 51, 34, 34, 34, 38, 34, 41, 46);
    SET(p, 5, SA, RATIO(9), SB, 50, SC, 50, SD, RATIO(6), SE, 30, SF, 30, SG, 10, SH, MODE4(0), P_DEC, 40, P_REL, 30,
        P_GATE, 12, P_DTIM, 24, P_DSND, 96, P_DFB, 50, P_DWID, 90, P_SPU, 50, P_VOL, 73, P_PAN, 72);
    BEAT(p, 5, "x..2..x..x.3....|x.....x.x.......|x..2..x..x.3....|x.....x.x..4....",
         72, 75, 79, 77, 75, 72, 70, 72, 75, 79, 82, 74, 77, 70, 72);
    trim(p, 52);

    /* A06 PRISM: melodic IDM in E dorian: an FM arpeggio of twelve notes turning against the sixteen-step bar, a
     * slow detuned digital line under it, a SID bass that slides and syncopates, a voice calling and answering;
     * kicks that never land the same, a crushed snare; the bus repeats in 32nds */
    p = pat_v5(5, "PRISM", 104, MACHINES(M_GSIN, M_GNOI, M_FMS, M_WAV, M_SID, M_VOX));
    p->glob[G_LEN] = 64; p->glob[G_GLCH] = 30; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_ADPC] = 20; p->glob[G_VERB] = 70;
    p->glob[G_DAMP] = 50; p->glob[G_DRIV] = 10;
    SET(p, 0, SE, 80, SF, 24, SG, 40, SC, 10, P_DEC, 32, P_REL, 8, P_DIST, 70, P_VOL, 127);
    BEAT(p, 0, "x..x....x.?..x..|x.....x..x..x...|x..x....x.?...x.|x.....x..x..3...", 36);
    SET(p, 1, SA, 72, SC, 30, SG, 60, P_DEC, 42, P_REL, 16, P_BITS, 40, P_SPU, 40, P_VOL, 103, P_PAN, 58);
    BEAT(p, 1, "....x.......x..2|....x..q....x...|....x.......x.2.|....x..x....x.4x", 60);
    SET(p, 2, SA, RATIO(6), SB, 60, SC, 40, SD, RATIO(13), SE, 26, SF, 16, SG, 8, SH, MODE4(0),
        P_DEC, 48, P_REL, 30, P_GATE, 10, P_DTIM, 40, P_DSND, 96, P_DFB, 46, P_DWID, 80, P_SPU, 50, P_VOL, 57);
    BEAT(p, 2, "xx.xx.xx.xx.xx.x|xx.xx.xx.xx.xx.x|xx.xx.xx.xx.xx.x|xx.xx.xx.xx.x?.x",
         76, 79, 83, 86, 85, 83, 81, 78, 76, 83, 79, 81);
    SET(p, 3, SA, WAVE(5), SB, 84, SC, 80, SF, 70, SG, 90, P_ATK, 60, P_HOLD, 127, P_DEC, 127, P_REL, 84,
        P_GATE, 127, P_WDTH, 40, P_LPQ, 20, P_SPU, 90, P_VOL, 51);
    BEAT(p, 3, "x...............|x...............|x...............|x.......x.......", 64, 62, 60, 59, 62);
    MOD(p, 3, 0, 16, 100, 80);
    SET(p, 4, SA, SIDW(2), SB, 30, SC, 50, SD, SIDW(1), SE, 64, SF, 67, SG, MODE4(0), SH, 60,
        P_DEC, 40, P_REL, 20, P_GATE, 20, P_PORT, 30, P_WDTH, 28, P_LPQ, 50, P_WOF, 82, P_FDEC, 40, P_DIST, 72,
        P_VOL, 117);
    BEAT(p, 4, "x..x..s...x.x...|x..x..s...x.x...|x..x..s...x.x...|x..x..s...x.x...",
         40, 40, 52, 43, 45, 38, 38, 50, 45, 47, 36, 36, 48, 43, 45, 35, 35, 47, 42, 43);
    SET(p, 5, SA, VOWEL(0), SB, 94, SC, 60, SE, 20, SF, 70, SG, 40, SH, 50, P_ATK, 30, P_HOLD, 127, P_DEC, 127,
        P_REL, 60, P_GATE, 40, P_DTIM, 64, P_DSND, 30, P_DFB, 40, P_SPU, 90, P_VOL, 83, P_PAN, 50);
    BEAT(p, 5, "................|....x.....x...x.|................|x.....x...x.....", 71, 69, 67, 76, 74, 71);

    trim(p, 57);

    /* A07 FOG: deep dub techno in F minor, all haze: one minor-ninth chord thrown into a three-eighths echo that
     * barely lets go, its filter drifting; a sub, hats and a shaker in the shuffle, a voice far off; tape and hiss */
    p = pat_v5(6, "FOG", 116, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_ENS, M_GSIN, M_VOX));
    p->glob[G_LEN] = 64; p->glob[G_HISS] = 30; p->glob[G_TAPE] = 24; p->glob[G_VERB] = 80; p->glob[G_DAMP] = 80;
    p->glob[G_SWNG] = 20;
    SET(p, 0, SE, 60, SF, 36, SC, 40, SG, 10, P_DEC, 40, P_REL, 12, P_VOL, 126);
    BEAT(p, 0, "x...x...x...x...|x...x...x...x..q|x...x...x...x...|x...x...x.q.x...", 36);
    SET(p, 1, SA, 90, P_DEC, 26, P_REL, 12, P_BASE, 28, P_SPU, 50, P_VOL, 59, P_PAN, 56);
    BEAT(p, 1, "..x...x...x...x.|..x..qx...x...x.|..x...x...x..qx.|..x...x...x...xq", 60);
    SET(p, 2, SA, 110, P_DEC, 10, P_REL, 6, P_BASE, 38, P_VOL, 63, P_PAN, 80);
    BEAT(p, 2, "qxqxqxqxqxqxqxqx|qxqxqxqxqxqxqxqx|qxqxqxqxqxqxqxqx|qxqxqxqxqxqxqxqx", 60);
    SET(p, 3, SA, SEMI(3), SB, SEMI(10), SC, SEMI(14), SD, 40, SE, 10, SG, 100, SH, 10, P_DEC, 50, P_REL, 30,
        P_GATE, 8, P_WDTH, 30, P_LPQ, 36, P_WOF, 80, P_FDEC, 40, P_DTIM, 96, P_DSND, 98, P_DFB, 60, P_DBAS, 44,
        P_DWID, 30, P_SPU, 90, P_VOL, 104);
    BEAT(p, 3, "x.....x.........|...x.......x....|x.....x.........|...x.....x......", 53, 53, 53, 53, 53, 53, 53, 51);
    lk(p, 3, 57, SA, SEMI(4)); lk(p, 3, 57, SB, SEMI(7)); lk(p, 3, 57, SC, SEMI(11));   /* Ebmaj7 */
    lfo(p, 3, 0, P_WDTH, 0, 0, 6, 74, 0);
    SET(p, 4, SC, 30, SH, 10, P_HOLD, 127, P_DEC, 127, P_REL, 30, P_GATE, 60, P_WDTH, 22, P_VOL, 72);
    BEAT(p, 4, "x.......x.......|x.......x.....x.|x.......x.......|x.......x...x...", 41, 41, 41, 41, 44, 41, 41, 41, 39, 39);
    SET(p, 5, SA, VOWEL(5), SB, 70, SC, 80, SE, 60, SF, 60, SG, 30, SH, 30, P_ATK, 70, P_HOLD, 127, P_DEC, 127,
        P_REL, 90, P_GATE, 127, P_SPU, 110, P_VOL, 85);
    BEAT(p, 5, "x...............|................|x...............|........x.......", 68, 65, 63);
    trim(p, 71);

    /* A08 LIQUID: liquid jungle in D-flat at 174: an electric-piano arpeggio through Dbmaj9 Bbm9 Gbmaj7 Absus, a
     * low saw sliding under it, a voice singing the melody into the reverb; a rolling break; the SPU's crunch */
    p = pat_v5(7, "LIQUID", 174, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_FMS, M_SAW, M_VOX));
    p->glob[G_LEN] = 64; p->glob[G_ADPC] = 30; p->glob[G_VERB] = 90; p->glob[G_DAMP] = 60; p->glob[G_TAPE] = 8;
    SET(p, 0, SE, 80, SF, 22, SG, 40, SC, 20, P_DEC, 28, P_REL, 8, P_DIST, 66, P_VOL, 127);
    BEAT(p, 0, "x.........x.....|..x.......x.....|x.........x.....|..x.....x.x.....", 36);
    SET(p, 1, SA, 80, SC, 30, SD, 50, SE, 30, SF, 20, SG, 20, P_DEC, 30, P_REL, 12, P_BASE, 20, P_EQG, 70,
        P_SPU, 70, P_VOL, 88);
    BEAT(p, 1, "....x.......x...|....x..q....x..q|....x.......x...|....x..q.x..x.2x", 60);
    SET(p, 2, SA, 120, P_DEC, 18, P_REL, 8, P_BASE, 36, P_VOL, 66, P_PAN, 40);
    BEAT(p, 2, "x.x.x.x.x.x.x.x.|x.xqx.x.x.xqx.x.|x.x.x.x.x.x.x.x.|x.xqx.x.x.xqx.xq", 60);
    SET(p, 3, SA, RATIO(3), SB, 40, SC, 50, SD, RATIO(15), SE, 16, SF, 10, SH, MODE4(0), P_DEC, 56, P_REL, 40,
        P_GATE, 14, P_SPU, 70, P_VOL, 50);
    BEAT(p, 3, "x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.",
         61, 65, 68, 72, 75, 72, 68, 65, 58, 61, 65, 68, 72, 68, 65, 61, 54, 58, 61, 65, 70, 65, 61, 58,
         56, 61, 63, 66, 68, 66, 63, 61);
    MOD(p, 3, 0, 30, 60, 60);
    SET(p, 4, SA, 80, SB, 40, SD, 20, SF, 60, SH, 64, P_HOLD, 127, P_DEC, 127, P_REL, 30, P_GATE, 100, P_PORT, 30,
        P_WDTH, 18, P_LPQ, 10, P_DIST, 70, P_VOL, 127, P_LEVL, 119);
    BEAT(p, 4, "x.......s.......|x.......s.......|x.......s.......|x.......s...s...", 37, 37, 34, 34, 30, 30, 32, 32, 35);
    SET(p, 5, SA, VOWEL(0), SB, 80, SC, 70, SE, 40, SF, 70, SG, 20, SH, 50, P_ATK, 40, P_HOLD, 127, P_DEC, 127,
        P_REL, 70, P_GATE, 40, P_DTIM, 96, P_DSND, 94, P_DFB, 44, P_SPU, 110, P_VOL, 61, P_PAN, 50);
    BEAT(p, 5, "x.......x...x...|x...............|x.......x...x...|x......x........", 77, 75, 72, 73, 77, 80, 77, 75, 73);
    trim(p, 87);

    /* A09 DREXC: deep-sea electro in E minor: an 808 kick long and low, a clap, a SID bass squelching and sliding,
     * a dark string pad slow through Em C D Bm, an aquatic digital lead in dotted echoes */
    p = pat_v5(8, "DREXC", 132, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_SID, M_ENS, M_WAV));
    p->glob[G_LEN] = 64; p->glob[G_VERB] = 70; p->glob[G_DAMP] = 60; p->glob[G_DRIV] = 14; p->glob[G_SWNG] = 4;
    SET(p, 0, SE, 50, SF, 60, SG, 10, SC, 40, SH, 10, P_DEC, 76, P_REL, 30, P_VOL, 91);
    BEAT(p, 0, "x.....x...x.....|x.....x.......x.|x.....x...x.....|x.....x...x..x..", 28);
    SET(p, 1, SA, 70, SC, 40, P_DEC, 30, P_REL, 12, P_BASE, 20, P_SPU, 60, P_VOL, 85);
    BEAT(p, 1, "....x.......x...|....x.......x...|....x.......x...|....x.......x..x", 60);
    SET(p, 2, SA, 118, P_DEC, 12, P_REL, 6, P_BASE, 36, P_VOL, 61, P_PAN, 54);
    BEAT(p, 2, "x.x.xqx.x.x.xqx.|x.x.xqx.x.x.xqx.|x.x.xqx.x.x.xqx.|x.x.xqx.x.xxxqx.", 60);
    SET(p, 3, SA, SIDW(1), SB, 40, SC, 20, SD, SIDW(1), SE, 64, SF, 68, SG, MODE4(0), SH, 60, P_DEC, 30, P_REL, 10,
        P_GATE, 10, P_PORT, 16, P_WDTH, 20, P_LPQ, 70, P_WOF, 90, P_FDEC, 24, P_DIST, 76, P_VOL, 127, P_LEVL, 125);
    BEAT(p, 3, "x.xs.x.xx.x.xs.x|x.xs.x.xx.x.xs.x|x.xs.x.xx.x.xs.x|x.xs.x.xx.xxxs.x",
         40, 40, 52, 43, 40, 47, 40, 45, 52, 40, 36, 36, 48, 40, 36, 43, 36, 40, 48, 36,
         38, 38, 50, 42, 38, 45, 38, 42, 50, 38, 35, 35, 47, 38, 35, 42, 35, 38, 47, 50, 52);
    SET(p, 4, SA, SEMI(3), SB, SEMI(7), SC, SEMI(12), SD, 60, SE, 40, SG, 100, SH, 20, P_ATK, 80, P_HOLD, 127,
        P_DEC, 127, P_REL, 96, P_GATE, 127, P_WDTH, 30, P_LPQ, 20, P_SPU, 90, P_VOL, 82);
    BEAT(p, 4, "x...............|x...............|x...............|x...............", 52, 48, 50, 47);
    lk(p, 4, 16, SA, SEMI(4)); lk(p, 4, 32, SA, SEMI(4));   /* C, D: major */
    MOD(p, 4, 0, 16, 90, 70);
    SET(p, 5, SA, WAVE(7), SB, 80, SC, 70, SF, 66, SG, 50, P_DEC, 60, P_REL, 40, P_GATE, 20, P_DTIM, 48, P_DSND, 96,
        P_DFB, 46, P_DWID, 90, P_SPU, 60, P_VOL, 127, P_LEVL, 104, P_PAN, 72);
    BEAT(p, 5, "x.....x.x.......|x.....x...x.....|x.....x.x.......|x...x...x...x...", 76, 79, 78, 76, 74, 71, 76, 79, 81, 79, 78, 76, 74);
    trim(p, 50);

    /* A10 STUTTR: glitch in G minor: every line stutters, by ratchet and by chance: the kick, the crushed snare,
     * hats that come and go, a SID arpeggio in sixteenth echoes, an FM bass, bit chords; the bus repeats in 64ths */
    p = pat_v5(9, "STUTTR", 110, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_SID, M_FMD, M_WAV));
    p->glob[G_LEN] = 64; p->glob[G_GLCH] = 90; p->glob[G_GSIZ] = GSIZE(0); p->glob[G_ADPC] = 50; p->glob[G_DRIV] = 24;
    p->glob[G_VERB] = 40;
    SET(p, 0, SE, 70, SF, 26, SG, 50, SC, 20, P_DEC, 34, P_REL, 8, P_DIST, 74, P_VOL, 127);
    BEAT(p, 0, "x..x..2.x..3....|x..x....x.x...4.|x..x..2.x..3....|x.x...x...x..4..", 36);
    SET(p, 1, SA, 64, SG, 60, P_DEC, 38, P_REL, 14, P_BITS, 40, P_XOR, 30, P_SPU, 40, P_VOL, 97);
    BEAT(p, 1, "....x...x..x.2..|....x.3.....x...|....x...x..x.2..|....x.....4.x.3x", 60);
    SET(p, 2, SA, 104, P_DEC, 10, P_REL, 6, P_BASE, 36, P_VOL, 50);
    BEAT(p, 2, "x?2?x?3?x?x?2?x4|x?2?x?3?x?x?2?x4|x?2?x?3?x?x?2?x4|x?2?x?3?x?x?2?44", 60);
    lfo(p, 2, 0, P_PAN, 6, 3, 64, 100, 0);
    SET(p, 3, SA, SIDW(2), SB, 50, SC, 50, SD, SIDW(0), SE, 96, SF, 64, SG, MODE4(0), SH, 40, P_DEC, 30, P_REL, 20,
        P_GATE, 8, P_WDTH, 46, P_LPQ, 30, P_WOF, 80, P_FDEC, 30, P_DTIM, 16, P_DSND, 96, P_DFB, 40, P_VOL, 88,
        P_PAN, 60);
    BEAT(p, 3, "xx2xxqxx3xxqxx2x|xx2xxqxx3xxqxx2x|xx2xxqxx3xxqxx2x|xx2xxqxx3xxqxx4x",
         67, 70, 74, 79, 74, 70, 67, 62, 67, 70, 74, 77, 74, 70, 67, 70, 63, 67, 70, 75, 70, 67, 63, 58, 63, 67, 70, 74, 70, 67, 63, 67,
         60, 63, 67, 72, 67, 63, 60, 55, 60, 63, 67, 70, 67, 63, 60, 63, 62, 66, 69, 74, 69, 66, 62, 57, 62, 66, 69, 72, 69, 66, 62, 66);
    SET(p, 4, SA, RATIO(3), SB, 50, SC, 30, SD, RATIO(6), SE, 30, SF, 20, SG, 30, SH, 70, P_DEC, 34, P_REL, 10,
        P_GATE, 10, P_WDTH, 28, P_LPQ, 30, P_DIST, 72, P_VOL, 127, P_LEVL, 103);
    BEAT(p, 4, "x.....x...x..x..|x.....x..x...x..|x.....x...x..x..|x.....x..x..xx.x",
         43, 43, 50, 46, 39, 39, 46, 43, 36, 36, 43, 39, 38, 38, 45, 42, 45, 50);
    SET(p, 5, SA, WAVE(25), SB, 64, SE, 50, SH, 70, P_DEC, 30, P_REL, 20, P_GATE, 6, P_WDTH, 50, P_LPQ, 20,
        P_DTIM, 8, P_DSND, 96, P_DFB, 50, P_SPU, 50, P_VOL, 111);
    BEAT(p, 5, "x...x.x.........|x...x.x.........|x...x.x.........|x...x.x...3...4.", 70, 74, 79, 67, 70, 75, 63, 67, 72, 66, 69, 74, 69, 74);
    trim(p, 40);

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

    /* A14 ROBOT: cold electro in C minor: a SID sequence in sixteenths through Cm Ab Bb G, a robot singing, metal
     * stabs in eighth echoes; an 808, a snare, metallic hats */
    p = pat_v5(13, "ROBOT", 118, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_SID, M_VOX, M_FMS));
    p->glob[G_LEN] = 64; p->glob[G_VERB] = 50; p->glob[G_DAMP] = 50; p->glob[G_DRIV] = 10;
    SET(p, 0, SE, 46, SF, 56, SG, 10, SC, 40, P_DEC, 66, P_REL, 26, P_VOL, 119);
    BEAT(p, 0, "x.......x.......|x.......x..x....|x.......x.......|x.......x..x..x.", 36);
    SET(p, 1, SA, 76, SC, 50, SD, 40, P_DEC, 36, P_REL, 14, P_BASE, 24, P_EQG, 70, P_SPU, 40, P_VOL, 95);
    BEAT(p, 1, "....x.......x...|....x.......x...|....x.......x...|....x.......x.xx", 60);
    SET(p, 2, SA, 110, SC, 90, SD, 70, P_DEC, 12, P_REL, 6, P_BASE, 36, P_VOL, 65, P_PAN, 46);
    BEAT(p, 2, "x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.|x.x.x.x.x.x.xxxx", 60);
    SET(p, 3, SA, SIDW(2), SB, 50, SC, 20, SD, SIDW(2), SE, 32, SF, 64, SG, MODE4(0), SH, 50, P_DEC, 26, P_REL, 8,
        P_GATE, 8, P_WDTH, 30, P_LPQ, 30, P_WOF, 80, P_FDEC, 26, P_VOL, 127, P_LEVL, 108);
    BEAT(p, 3, "xxxxxxxxxxxxxxxx|xxxxxxxxxxxxxxxx|xxxxxxxxxxxxxxxx|xxxxxxxxxxxxxxxx",
         36, 36, 48, 36, 39, 36, 43, 36, 36, 36, 48, 36, 46, 36, 43, 39, 32, 32, 44, 32, 36, 32, 39, 32, 32, 32, 44, 32, 42, 32, 39, 36,
         34, 34, 46, 34, 38, 34, 41, 34, 34, 34, 46, 34, 44, 34, 41, 38, 31, 31, 43, 31, 35, 31, 38, 31, 31, 31, 43, 31, 41, 31, 38, 35);
    SET(p, 4, SA, VOWEL(2), SB, 64, SD, 40, SE, 0, SF, 90, SG, 100, SH, 0, P_ATK, 4, P_HOLD, 127, P_DEC, 127,
        P_REL, 20, P_GATE, 24, P_DTIM, 64, P_DSND, 34, P_DFB, 30, P_SPU, 50, P_VOL, 127, P_LEVL, 127);
    BEAT(p, 4, "x...x...x.x.....|x...x...x.......|x...x...x.x.....|x...x...x...x...", 72, 70, 67, 70, 68, 67, 63, 70, 72, 74, 75, 74, 71, 67, 74);
    SET(p, 5, SA, RATIO(17), SB, 50, SC, 30, SD, RATIO(8), SE, 40, SF, 20, SG, 20, SH, MODE4(0), P_DEC, 30, P_REL, 16,
        P_GATE, 6, P_WDTH, 60, P_DTIM, 32, P_DSND, 96, P_DFB, 30, P_SPU, 40, P_VOL, 127, P_LEVL, 125, P_PAN, 76);
    BEAT(p, 5, "..x.....x.....x.|..x.....x.....x.|..x.....x.....x.|..x.....x...x.x.", 72, 75, 79, 68, 72, 75, 70, 74, 77, 71, 74, 79, 74);
    trim(p, 45);

    /* A15 BRAIN: an IDM break in D dorian: kicks on conditions so no two loops match, a snare that rolls, metal
     * flickering; a seven-note FM arpeggio turning against a ten-note rhythm, a SID bass, a metal pad; repeats */
    p = pat_v5(14, "BRAIN", 128, MACHINES(M_GSIN, M_GNOI, M_GNOI, M_FMD, M_SID, M_WAV));
    p->glob[G_LEN] = 64; p->glob[G_GLCH] = 50; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_ADPC] = 20; p->glob[G_DRIV] = 20;
    p->glob[G_VERB] = 60;
    SET(p, 0, SE, 76, SF, 26, SG, 40, SC, 20, P_DEC, 30, P_REL, 8, P_DIST, 70, P_VOL, 127);
    BEAT(p, 0, "x.h..x....x.pH..|x....h.x..x...k.|x.h..x....x.pH..|x..x..x...x.2.4.", 36);
    SET(p, 1, SA, 62, SC, 40, SD, 60, SG, 50, P_DEC, 34, P_REL, 14, P_BITS, 30, P_SPU, 40, P_VOL, 100);
    BEAT(p, 1, "....x..3....x..q|....x....2..x...|....x..3....x..q|...2x.....3.x.4x", 60);
    SET(p, 2, SA, 90, SC, 110, SD, 90, P_DEC, 10, P_REL, 6, P_BASE, 32, P_VOL, 64);
    BEAT(p, 2, "x?2qx.x?3.xqx?2x|x?2qx.x?3.xqx?2x|x?2qx.x?3.xqx?2x|x?2qx.x?3.xqx?44", 60);
    lfo(p, 2, 0, P_PAN, 6, 3, 64, 100, 0);
    SET(p, 3, SA, RATIO(6), SB, 60, SC, 30, SD, RATIO(11), SE, 30, SF, 20, SG, 20, SH, 80, P_DEC, 34, P_REL, 20,
        P_GATE, 8, P_DTIM, 40, P_DSND, 96, P_DFB, 46, P_SPU, 50, P_VOL, 77, P_PAN, 62);
    BEAT(p, 3, "x.xx.x.x.xx.x.x.|x.xx.x.x.xx.x.x.|x.xx.x.x.xx.x.x.|x.xx.x.x.xx.x?x?", 74, 77, 81, 84, 83, 81, 76);
    SET(p, 4, SA, SIDW(1), SB, 40, SC, 30, SD, SIDW(2), SE, 32, SF, 64, SH, 50, P_DEC, 30, P_REL, 10, P_GATE, 10,
        P_PORT, 20, P_WDTH, 24, P_LPQ, 50, P_WOF, 84, P_FDEC, 30, P_DIST, 74, P_VOL, 125);
    BEAT(p, 4, "x..x....s.x..x..|x..x....s.x.....|x..x....s.x..x..|x..x....s.xx.x.x",
         38, 38, 45, 41, 38, 36, 36, 43, 40, 38, 38, 45, 41, 38, 43, 43, 50, 47, 45, 43, 41);
    SET(p, 5, SA, WAVE(19), SB, 90, SC, 90, SF, 70, SG, 90, P_ATK, 50, P_HOLD, 127, P_DEC, 127, P_REL, 80,
        P_GATE, 127, P_WDTH, 38, P_LPQ, 20, P_SPU, 90, P_VOL, 66);
    BEAT(p, 5, "x...............|x...............|x...............|x.......x.......", 62, 60, 65, 67, 64);
    MOD(p, 5, 0, 16, 100, 60);
    trim(p, 52);

    /* A16 SHARDS: glitch in F# minor: a voice cut to shards, bit-crushed and ratcheting into sixteenth echoes; a
     * SID bass sliding through F#m D A E, FM bells arpeggiating each chord, a slow digital pad; a stuttering kick,
     * a crushed snare; the bus repeats in 32nds */
    p = pat_v5(15, "SHARDS", 100, MACHINES(M_GSIN, M_GNOI, M_VOX, M_SID, M_FMS, M_WAV));
    p->glob[G_LEN] = 64; p->glob[G_GLCH] = 70; p->glob[G_GSIZ] = GSIZE(1); p->glob[G_ADPC] = 40; p->glob[G_DRIV] = 16;
    p->glob[G_VERB] = 60; p->glob[G_SWNG] = 10;
    SET(p, 0, SE, 72, SF, 28, SG, 40, SC, 20, P_DEC, 34, P_REL, 8, P_DIST, 72, P_VOL, 127);
    BEAT(p, 0, "x.....x.x.....2.|x..x....x.....x.|x.....x.x...3...|x..x..x.x..4....", 36);
    SET(p, 1, SA, 66, SG, 70, P_DEC, 36, P_REL, 14, P_BITS, 36, P_XOR, 24, P_SPU, 40, P_VOL, 119);
    BEAT(p, 1, "....x...q...x.2.|....x.....3.x...|....x...q...x.2.|..2.x.....4.x.3x", 60);
    SET(p, 2, SA, VOWEL(1), SB, 100, SC, 10, SE, 10, SF, 70, SG, 60, SH, 0, P_DEC, 30, P_REL, 10, P_GATE, 6,
        P_BITS, 30, P_DTIM, 16, P_DSND, 96, P_DFB, 50, P_SPU, 40, P_VOL, 125);
    BEAT(p, 2, "x.2.x..x.3..x...|x.2.x..x....x.4.|x.2.x..x.3..x...|x.2.x..x.x..x.44", 66, 69, 71, 73, 76, 73, 71, 69);
    SET(p, 3, SA, SIDW(2), SB, 30, SC, 40, SD, SIDW(1), SE, 64, SF, 66, SG, MODE4(0), SH, 50, P_DEC, 34, P_REL, 10,
        P_GATE, 12, P_PORT, 20, P_WDTH, 26, P_LPQ, 46, P_WOF, 84, P_FDEC, 30, P_DIST, 74, P_VOL, 127, P_LEVL, 103);
    BEAT(p, 3, "x..x..s...x.x...|x..x..s...x.....|x..x..s...x.x...|x..x..s...x.x.xs",
         42, 42, 54, 45, 49, 38, 38, 50, 42, 45, 45, 57, 49, 52, 40, 40, 52, 44, 47, 52, 50);
    SET(p, 4, SA, RATIO(10), SB, 50, SC, 50, SD, RATIO(8), SE, 30, SF, 30, SH, MODE4(0), P_DEC, 50, P_REL, 40,
        P_GATE, 10, P_DTIM, 48, P_DSND, 34, P_DFB, 46, P_SPU, 70, P_VOL, 64, P_PAN, 72);
    BEAT(p, 4, "..x...x...x...x.|..x...x...x...x.|..x...x...x...x.|..x...x...x.x.x.", 73, 78, 81, 76, 74, 78, 81, 78, 76, 81, 85, 81, 76, 80, 83, 80, 83);
    SET(p, 5, SA, WAVE(8), SB, 80, SC, 80, SE, 30, SF, 70, SG, 80, P_ATK, 60, P_HOLD, 127, P_DEC, 127, P_REL, 84,
        P_GATE, 127, P_WDTH, 36, P_LPQ, 20, P_SPU, 90, P_VOL, 57);
    BEAT(p, 5, "x...............|x...............|x...............|x...............", 54, 50, 57, 52);
    MOD(p, 5, 0, 16, 100, 70);
    trim(p, 56);

    /* A05..A16, the bank before, were written on SWAV-ENS's old page and the bus's old laws: onto today's, once
     * (the new bank's parts, A01..A04 so far, are written on today's) */
    for (int i = 4; i < 16; i++) {
        ens_v6(&pat[i]);
        bus_v9(&pat[i]);
    }
}
