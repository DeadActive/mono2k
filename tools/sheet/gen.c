/* SPDX-License-Identifier: GPL-3.0-only */
#define main host_main
#include "host/m2k_host.c"
#undef main
/* The sound sheet's search: one track, a machine at random settings over their whole ranges, with motion (locks on
 * every hit, or LFOs on a held tone), two bars at 103 BPM. Not hand-written: the point is to leave my habits out.
 * usage: gen OUTDIR FIRST COUNT [EDIT..]  ->  OUTDIR/c%05d.wav and a line per candidate in OUTDIR/kits.tsv
 * edits, after the candidate is drawn (a refinement of a picked one): p=v sets kit parameter p; Lp=lo:hi squeezes
 * every lock of p into lo..hi (the motion kept, its range narrowed); T16 adds a trig on step 16 */
static uint32_t rs;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static int ri(int a, int b) { return a + (int)(rnd() % (uint32_t)(b - a + 1)); }   /* a..b */
static int chance(int pct) { return ri(0, 99) < pct; }
static int clampi(int v) { return v < 0 ? 0 : v > 127 ? 127 : v; }

static int16_t out[2 * 44100 * 8];

/* the sources, weighted: FM, noise, the drums and the waves more, the chip-like ones less */
static const struct { int m, w; } SRC[] = {
    {M_FMS, 3}, {M_FMD, 3}, {M_FMP, 3}, {M_GNOI, 2}, {M_GPRC, 2}, {M_GSIN, 2}, {M_WAV, 2}, {M_DKIK, 2}, {M_DSNR, 2},
    {M_DHAT, 2}, {M_DCYM, 2}, {M_SID, 1}, {M_SAW, 1}, {M_ENS, 1}, {M_PUL, 1},
};
static int tune_on_h(int m) { return m != M_GPRC && m != M_WAV && m < M_DKIK; }
static int drum(int m) { return m >= M_DKIK || m == M_GPRC; }

/* what moves: the machine's A..H (not a TUNE), the filter, the dirt, the place */
static int timbre(int m)
{
    static const int X[] = {P_BASE, P_WDTH, P_LPQ, P_DIST, P_FOLD, P_BITS, P_SRR, P_PAN, P_DSND, P_DEC, P_EQF, P_HPQ};
    for (;;) {
        int k = ri(0, 7 + 12);
        if (k < 8) { if (k == 7 && tune_on_h(m)) continue; return P_SYN + k; }
        return X[k - 8];
    }
}

static void euclid(int *hit, int k, int n, int rot)
{
    for (int i = 0; i < n; i++) hit[(i + rot) % n] = ((i * k) % n) < k;
}

/* a candidate: its machine and kit, the bus it was heard on, its note, its motion (locks drawn around base[i] by
 * spr[i], 127: anywhere) */
typedef struct { int m, held, note, nmov, mv[3], base[3], spr[3]; uint8_t kit[P_COUNT], glob[G_COUNT]; } cand_t;
static m2k_pattern_t scratch;
/* the globals sheet 1 was rendered on (the boot pattern's, then the candidate's own): fixed here so the sounds do not
 * move when the factory does */
static const uint8_t GLOB0[G_COUNT] = {73, 10, 32, 60, 0, 0, 0, 0, 0, 0, 10, 88, 82, 82, 64, 82, 82, 100};

static void draw(int id, cand_t *c)
{
    rs = (uint32_t)id * 2654435761u + 12345u; for (int i = 0; i < 8; i++) rnd();
    m_cpy(c->glob, GLOB0, G_COUNT);
    int tw = 0; for (unsigned i = 0; i < sizeof SRC / sizeof *SRC; i++) tw += SRC[i].w;
    int pick = ri(0, tw - 1), m = SRC[0].m;
    for (unsigned i = 0; i < sizeof SRC / sizeof *SRC; i++) { if (pick < SRC[i].w) { m = SRC[i].m; break; } pick -= SRC[i].w; }
    kit_default(&scratch, 0, m);
    uint8_t *k = scratch.kit[0];
    for (int i = 0; i < 8; i++) k[P_SYN + i] = (uint8_t)ri(0, 127);
    if (tune_on_h(m)) k[P_SYN + 7] = 64;
    if (m == M_WAV) k[P_WBNK] = chance(50) ? 127 : 0;
    int held = !drum(m) && chance(50);
    /* the envelope */
    if (held) {
        k[P_ATK] = (uint8_t)(chance(50) ? ri(0, 20) : ri(20, 100)); k[P_HOLD] = 127; k[P_DEC] = 127;
        k[P_REL] = (uint8_t)ri(40, 127); k[P_GATE] = 127;
    } else {
        k[P_ATK] = (uint8_t)(chance(80) ? 0 : ri(1, 30)); k[P_HOLD] = (uint8_t)ri(0, 40);
        k[P_DEC] = (uint8_t)ri(10, 110); k[P_REL] = (uint8_t)ri(0, 90); k[P_GATE] = (uint8_t)ri(2, 40);
    }
    /* the filter, its envelope */
    k[P_BASE] = (uint8_t)ri(0, 80); k[P_WDTH] = (uint8_t)ri(30, 127);
    k[P_HPQ] = (uint8_t)ri(0, 100); k[P_LPQ] = (uint8_t)ri(0, 110);
    k[P_FATK] = (uint8_t)ri(0, 60); k[P_FDEC] = (uint8_t)ri(0, 127);
    k[P_BOF] = (uint8_t)clampi(64 + ri(-50, 50)); k[P_WOF] = (uint8_t)clampi(64 + ri(-50, 50));
    /* the dirt, the eq */
    k[P_DIST] = (uint8_t)(chance(50) ? 64 : ri(0, 127));
    if (chance(25)) k[P_FOLD] = (uint8_t)ri(0, 127);
    if (chance(25)) k[P_BITS] = (uint8_t)ri(0, 127);
    if (chance(25)) k[P_SRR] = (uint8_t)ri(0, 127);
    if (chance(10)) k[P_XOR] = (uint8_t)ri(0, 127);
    k[P_EQF] = (uint8_t)ri(0, 127); k[P_EQG] = (uint8_t)clampi(64 + ri(-30, 30));
    /* the place */
    k[P_PAN] = (uint8_t)clampi(64 + ri(-30, 30));
    if (chance(50)) { k[P_DSND] = (uint8_t)ri(20, 120); k[P_DTIM] = (uint8_t)ri(0, 127); k[P_DFB] = (uint8_t)ri(0, 100);
                      k[P_DWID] = (uint8_t)ri(40, 127); }
    if (chance(55)) k[P_SPU] = (uint8_t)ri(20, 127);
    if (chance(45)) { k[P_MODT] = enc(ri(0, 2), 3); k[P_MODR] = (uint8_t)ri(0, 127); k[P_MODD] = (uint8_t)ri(20, 127);
                      k[P_MODM] = (uint8_t)ri(30, 127); }
    k[P_LEVL] = 80;
    /* the pattern: two bars at 103 */
    c->glob[G_TMPO] = 103 - 30; c->glob[G_LEN] = 32; c->glob[G_DRIV] = BANK_DRIVE;
    c->glob[G_VERB] = (uint8_t)ri(20, 120); c->glob[G_DAMP] = (uint8_t)ri(10, 110);
    if (chance(15)) c->glob[G_TAPE] = (uint8_t)ri(0, 127);
    if (chance(15)) c->glob[G_ADPC] = (uint8_t)ri(0, 127);
    int note = drum(m) ? ri(36, 60) : chance(35) ? ri(26, 40) : chance(55) ? ri(41, 57) : ri(58, 76);
    int nmov = ri(1, 3), mv[3], base[3], spr[3];
    for (int i = 0; i < nmov; i++) { mv[i] = timbre(m); base[i] = k[mv[i]]; spr[i] = chance(30) ? 127 : ri(10, 60); }
    int nl = held ? ri(1, 3) : ri(0, 1);
    for (int l = 0; l < nl; l++) {
        uint8_t *b = &k[P_LFO + 8 * l];
        b[L_DEST] = (uint8_t)(timbre(m) + 1); b[L_WAVE] = enc(ri(0, 6), 7); b[L_MULT] = enc(ri(0, 4), 8);
        b[L_SPD] = (uint8_t)ri(0, 127); b[L_DPTH] = (uint8_t)clampi(64 + (chance(50) ? 1 : -1) * ri(10, 63));
        b[L_TRIG] = enc(chance(70) ? 0 : 1, 5);
    }
    c->m = m; c->held = held; c->note = note; c->nmov = nmov;
    for (int i = 0; i < nmov; i++) { c->mv[i] = mv[i]; c->base[i] = base[i]; c->spr[i] = spr[i]; }
    m_cpy(c->kit, k, P_COUNT);
}

/* the candidate's own two bars on track t, drawn on from where draw() left the generator: its hits, accents,
 * ratchets, a held tone's second note, the motion's locks on each hit (sheet 1's, exactly) */
static void lay(const cand_t *c, m2k_pattern_t *q, int t, int *hit)
{
    int held = c->held, note = c->note, nmov = c->nmov;
    const int *mv = c->mv, *base = c->base, *spr = c->spr;
    if (held) {
        hit[0] = 1; if (chance(40)) hit[16] = 1;
    } else {
        int h16[16] = {0}; euclid(h16, ri(3, 11), 16, ri(0, 15));
        for (int s = 0; s < 32; s++) hit[s] = h16[s % 16];
        if (chance(30)) for (int s = 16; s < 32; s++) hit[s] = chance(50) ? h16[s % 16] : chance(25);
    }
    int second = note + (chance(50) ? ri(-7, 7) : 0);
    for (int s = 0; s < 32; s++) {
        if (!hit[s]) continue;
        m2k_step_t *st = &q->step[t][s];
        st->flags = ST_TRIG | (chance(20) ? ST_ACC : 0);
        st->note = (uint8_t)(held && s == 16 ? second : note);
        if (!held && chance(12)) st->rtrg = (uint8_t)ri(2, 4);
        if (!held) for (int i = 0; i < nmov; i++)
            lock_set(t, s, mv[i], spr[i] == 127 ? ri(0, 127) : clampi(base[i] + ri(-spr[i], spr[i])));
    }
}

#ifndef GEN_NO_MAIN
int main(int argc, char **argv)
{
    if (argc < 4) return 1;
    int first = atoi(argv[2]), count = atoi(argv[3]);
    char path[600];
    snprintf(path, sizeof path, "%s/kits.tsv", argv[1]);
    FILE *kf = fopen(path, "a");
    for (int id = first; id < first + count; id++) {
        cand_t cd, *c = &cd;
        m2k_init(7);
        m2k_pattern_t *q = PAT(cur_pat);
        steps_clear(q, -1);
        q->mute = 0x3E;
        draw(id, c);
        kit_default(q, 0, c->m);
        m_cpy(q->kit[0], c->kit, P_COUNT);
        m_cpy(q->glob, c->glob, G_COUNT);
        uint8_t *k = q->kit[0];
        int m = c->m, held = c->held, note = c->note, nmov = c->nmov, *mv = c->mv, *base = c->base, *spr = c->spr;
        (void)m;
        int hit[32] = {0};
        lay(c, q, 0, hit);
        for (int a = 4; a < argc; a++) {
            int pp, v, lo, hi;
            if (sscanf(argv[a], "L%d=%d:%d", &pp, &lo, &hi) == 3) {
                for (int i = 0; i < M2K_LOCKS; i++)
                    if (q->lock[i].trk == 0 && q->lock[i].param == pp) q->lock[i].val = (uint8_t)(lo + q->lock[i].val * (hi - lo) / 127);
            } else if (!strcmp(argv[a], "T16")) {
                q->step[0][16].flags = ST_TRIG; q->step[0][16].note = q->step[0][0].note; hit[16] = 1;
            } else if (sscanf(argv[a], "%d=%d", &pp, &v) == 2 && pp >= 0 && pp < P_COUNT) k[pp] = (uint8_t)v;
        }
        lockmap_build();
        m_cpy(trk[0].eff, q->kit[0], P_COUNT);
        int n = (int)(5.4 * 44100) & ~127;
        seq_play(1);
        for (int f = 0; f < n; f += 128) m2k_render(out + 2 * f, 128);
        seq_play(0);
        snprintf(path, sizeof path, "%s/c%05d.wav", argv[1], id);
        wav_write(path, out, n);
        fprintf(kf, "%d\t%d\t%d\t%d\t", id, m, held, note);
        for (int i = 0; i < P_COUNT; i++) fprintf(kf, "%d%c", k[i], i + 1 < P_COUNT ? ',' : '\t');
        for (int i = 0; i < G_COUNT; i++) fprintf(kf, "%d%c", q->glob[i], i + 1 < G_COUNT ? ',' : '\t');
        for (int s = 0; s < 32; s++) fprintf(kf, "%c", hit[s] ? (q->step[0][s].rtrg > 1 ? 'r' : q->step[0][s].flags & ST_ACC ? 'X' : 'x') : '.');
        fprintf(kf, "\t");
        for (int i = 0; i < nmov && !held; i++) fprintf(kf, "%d:%d:%d%s", mv[i], base[i], spr[i], i + 1 < nmov ? "," : "");
        fprintf(kf, "\n");
    }
    fclose(kf);
    return 0;
}
#endif
