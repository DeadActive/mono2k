/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's tracks: a machine, then the Monomachine's chain: HP and LP filters (base / width), amp envelope with
 * distortion, a parametric EQ band, sample rate reduction, the destroy stage (bits, fold, xor), pan, and sends to
 * the track's delay and the bus reverb. Machines are clean-room designs after what is published about the
 * Monomachine's machine families, not ports of its code. Control rate: every M2K_SUB samples. */

typedef struct { int32_t s1, s2, g, k, d; } svf_t;

static void svf_set(svf_t *f, int32_t q3, int32_t r12)
{
    q3 = CLAMP(q3, 0, 1016);
    int32_t g = T_CUT_G[q3], k = 2 * r12;
    int32_t den = 4096 + ((k * g) >> 12) + ((g * g) >> 12);
    f->g = g;
    f->k = k;
    f->d = (int32_t)((1u << 28) / (uint32_t)den);
}

/* one ZDF state-variable step; returns lp, writes hp and bp */
static inline int32_t svf_tick(svf_t *f, int32_t x, int32_t *hp_out, int32_t *bp_out)
{
    int32_t hp = (int32_t)(((int64_t)(x - (int32_t)(((int64_t)(f->k + f->g) * f->s1) >> 12) - f->s2) * f->d) >> 16);
    int32_t v1 = (int32_t)(((int64_t)f->g * hp) >> 12);
    int32_t bp = v1 + f->s1;
    f->s1 = CLAMP(v1 + bp, -(1 << 21), 1 << 21);
    int32_t v2 = (int32_t)(((int64_t)f->g * bp) >> 12);
    int32_t lp = v2 + f->s2;
    f->s2 = CLAMP(v2 + lp, -(1 << 21), 1 << 21);
    if (hp_out) *hp_out = hp;
    if (bp_out) *bp_out = bp;
    return lp;
}

/* the track filter's parts, after the Monomachine's (measured, README: Calibration): a ZDF two-pole and a ZDF one-pole
 * on Q24 coefficients and 64-bit products, their states held in range. Each update carries its remainder to the next
 * sample, so a low cutoff settles exactly instead of sticking a few hundred steps off zero */
#define ZS_MAX (1 << 28)
typedef struct { int32_t s1, s2, a1, a2, a3, k; int64_t r1, r2; } zsvf_t;   /* a1 a2 a3 Q30, k Q24; r: remainders */
typedef struct { int32_t s, G; int64_t r; } zop_t;                          /* G Q24 */

/* g, k Q24 (g = tan(pi fc / fs), k = 1 / Q); the one division a block is 32-bit. 1 / den from it alone holds 14
 * bits, and a low cutoff's damping is g k ~1e-5 of den: at 131 Hz HPQ 127 rounded to no damping at all. One Newton
 * step (multiplies only) takes it to Q30 */
static void zsvf_set(zsvf_t *f, uint32_t g, uint32_t k)
{
    int64_t den = (1LL << 24) + (((int64_t)g * ((int64_t)g + k)) >> 24);         /* Q24, 1 .. ~110 */
    int64_t a1 = (int64_t)(0x80000000u / (uint32_t)(den >> 10)) << 13;          /* Q30, ~14 bits */
    int64_t e = ((1LL << 54) - den * a1) >> 24;                                    /* Q30: 1 - den a1 */
    a1 += (a1 * e) >> 30;
    f->a1 = (int32_t)a1;
    f->a2 = (int32_t)(((int64_t)g * a1) >> 24);
    f->a3 = (int32_t)(((int64_t)g * f->a2) >> 24);
    f->k = (int32_t)k;
}

static inline void zsvf_tick(zsvf_t *f, int32_t v0, int32_t *lp, int32_t *bp, int32_t *hp)
{
    int32_t v3 = v0 - f->s2;
    int64_t p1 = (int64_t)f->a1 * f->s1 + (int64_t)f->a2 * v3 + f->r1;
    int64_t p2 = (int64_t)f->a2 * f->s1 + (int64_t)f->a3 * v3 + f->r2;
    int32_t v1 = (int32_t)(p1 >> 30), d2 = (int32_t)(p2 >> 30);
    f->r1 = p1 - (int64_t)v1 * (1LL << 30);                    /* a multiply: shifting a negative left is undefined */
    f->r2 = p2 - (int64_t)d2 * (1LL << 30);
    int32_t v2 = f->s2 + d2;
    f->s1 = CLAMP(2 * v1 - f->s1, -ZS_MAX, ZS_MAX);
    f->s2 = CLAMP(2 * v2 - f->s2, -ZS_MAX, ZS_MAX);
    if (lp) *lp = v2;
    if (bp) *bp = v1;
    if (hp) *hp = v0 - (int32_t)(((int64_t)f->k * v1) >> 24) - v2;
}

static inline int32_t zop_lp(zop_t *f, int32_t x)
{
    int64_t p = (int64_t)(x - f->s) * f->G + f->r;
    int32_t v = (int32_t)(p >> 24);
    f->r = p - (int64_t)v * (1LL << 24);
    int32_t y = v + f->s;
    f->s = CLAMP(y + v, -ZS_MAX, ZS_MAX);
    return y;
}

/* a Q24 coefficient at a filter position (1/96 octave over MIDI note 0, Q8), between table entries */
static uint32_t fg_at(const uint32_t *T, int32_t pos)
{
    if (pos <= 0) return T[0];
    int32_t i = pos >> 8;
    if (i >= F_TOP) return T[F_TOP];
    return T[i] + (uint32_t)(((uint64_t)(T[i + 1] - T[i]) * (uint32_t)(pos & 255)) >> 8);
}

#define DL_LEN 16384u                  /* bytes per track: 0.74 s at half rate, mu-law; two lines of half that for ping-pong */

typedef struct {
    uint8_t note, gate, vel, sounding, slide, let_go;   /* let_go: a key or a MIDI note-off, not the sequencer */
    int32_t pitch, ptarget;            /* 1/64 semitones */
    uint8_t astage, fstage;
    uint32_t aenv, fenv;               /* Q30 */
    int32_t hold;                      /* control blocks left in hold */
    int32_t amp0, amp1;                /* Q15, this block's ramp */
    int32_t gate_left;                 /* control blocks until the sequencer's note-off; <0: none */
    /* machine state */
    uint32_t ph[8];
    int32_t ms[6];
    uint32_t menv[3];                  /* Q30 envelopes owned by the machine */
    uint32_t lfsr;
    svf_t fm[3];                       /* formants (VOX) */
    /* the chain: EQ, the filter (LP1 before the clip, the HP's two-pole, its one-pole and LP2 after), SRR */
    zsvf_t eq, lp1, hp2;
    zop_t hp1, lp2a, lp2b;
    int32_t eqc;                       /* the EQ band's mix, Q24 */
    uint32_t srr_acc;
    int32_t srr_hold, srr_prev;
    /* delay: a sum line, and a difference line while the send is positive (ping-pong) */
    uint8_t *dl;
    uint32_t dw, dph;                  /* write index; the line's clock, Q16 of a line sample */
    int32_t dt_q8;                     /* smoothed delay time in half-rate samples, Q8 */
    int32_t dacc[2], dn, dprev[2], dcur[2], dhp[2], dlp[2];
    uint8_t dmode;                     /* 0: one line, the echoes keep the source's side; 1: ping-pong */
    /* LFOs */
    uint32_t lph[3];
    int32_t lval[3], lhold[3], lrnd[3];
    uint8_t ldone[3];
    /* params: the kit with this trig's locks, and the modulation in 1/256 steps */
    uint8_t eff[P_COUNT];
    int32_t mod[P_COUNT], mod_acc[P_COUNT];
    int32_t peak;
    /* the modulation effect */
    uint32_t mph;
    uint16_t mw;
    int32_t map[4], mfb;
} trk_t;

static trk_t trk[M2K_TRACKS];
static uint8_t dl_mem[M2K_TRACKS][DL_LEN] POOL;
#define MOD_LEN 1024u                  /* 23 ms: room for the chorus' sweep */
static int16_t mod_mem[M2K_TRACKS][MOD_LEN] POOL;
static int16_t blk[M2K_TRACKS][M2K_SUB];      /* machine output of this block */

/* a parameter with its modulation, 0..127*256 */
static inline int32_t P8(const trk_t *t, int id)
{
    int32_t v = (int32_t)t->eff[id] * 256 + t->mod[id];
    return CLAMP(v, 0, 127 * 256);
}
static inline int32_t P7(const trk_t *t, int id) { return P8(t, id) >> 8; }
/* bipolar, -64..63 (Q8) */
static inline int32_t PB8(const trk_t *t, int id) { return P8(t, id) - 64 * 256; }
/* a choice of n */
static inline int32_t PN(const trk_t *t, int id, int n) { return (P7(t, id) * n) >> 7; }

/* ------------------------------------------------------------------------------------------------ machines */

/* PolyBLEP saw, -32768..32767; inv = 0xFFFFFFFF / inc */
static inline int32_t blep_saw(uint32_t ph, uint32_t inc, uint32_t inv)
{
    int32_t s = (int32_t)(ph >> 16) - 32768;
    if (ph < inc) {
        uint32_t t = (ph * inv) >> 16;
        int32_t p = 2 * (int32_t)t - (int32_t)((t * t) >> 16) - 65536;
        s -= p >> 1;
    } else if (~ph < inc) {
        uint32_t u = (~ph * inv) >> 16, w = 65536 - u;
        s -= (int32_t)((w * w) >> 16) >> 1;
    }
    return s;
}

static inline int32_t fold16(int32_t y)
{
    int32_t p = (y + 32768) & 0x1FFFF;
    if (p > 65535) p = 131071 - p;
    return p - 32768;
}

static inline uint32_t noise_step(uint32_t *s) { uint32_t x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; *s = x; return x; }

static const int16_t SID_SHAPES[8] = {1, 2, 4, 8, 3, 5, 6, 7};   /* tri saw pul noi | t+s t+p s+p t+s+p */
static const int8_t SAW_SPREAD[7] = {0, -16, 16, -9, 9, -4, 4};
static const uint16_t METAL_HZ[6] = {205, 304, 370, 523, 540, 800};

static void mach_trig(trk_t *t, int m)
{
    t->menv[0] = t->menv[1] = t->menv[2] = 1u << 30;
    switch (m) {
    case M_GSIN: case M_FMS: case M_FMD:
        t->ph[0] = t->ph[1] = t->ph[2] = t->ph[3] = 0;
        t->ms[0] = t->ms[1] = 0;
        break;
    case M_WAV:
        t->ph[0] = t->ph[1] = 0;
        break;
    default:
        break;
    }
}

/* the machines: fill blk with 16 samples */
static void mach_block(int ti, int m, int32_t pitch)
{
    trk_t *t = &trk[ti];
    int16_t *o = blk[ti];
    const uint8_t *e = t->eff;
    (void)e;
    switch (m) {
    case M_GSIN: {                     /* TUNE FOLD SUB NOIS PDEP PDEC CLIK HARM */
        int32_t env = (int32_t)(t->menv[0] >> 15);
        int32_t pq = pitch + (PB8(t, P_SYN + 0) >> 7) + ((P7(t, P_SYN + 4) * 24 * env) >> 15);   /* TUNE: +-2 semitones */
        uint32_t inc = inc_of(pq);
        t->menv[0] = mulu_hi(t->menv[0], T_ENV_COEF[P7(t, P_SYN + 5)], 31);
        int32_t fold = P8(t, P_SYN + 1), sub = T_VOL[P7(t, P_SYN + 2)], noi = T_VOL[P7(t, P_SYN + 3)] >> 1;
        int32_t harm = T_VOL[P7(t, P_SYN + 7)];
        int32_t clk = (int32_t)(t->menv[1] >> 15) * P7(t, P_SYN + 6) >> 7;
        t->menv[1] = mulu_hi(t->menv[1], 1700000000u, 31);
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t s = sinp(t->ph[0]);
            if (harm) s += (sinp(t->ph[0] * 2) * harm) >> 16;
            if (fold) s = fold16((s * (256 + (fold >> 4))) >> 8);
            if (sub) s += (sinp(t->ph[1]) * sub) >> 15;
            uint32_t n = noise_step(&t->lfsr);
            if (noi) s += (((int32_t)n >> 16) * noi) >> 15;
            if (clk) s += (((int32_t)n >> 16) * clk) >> 15;
            t->ph[0] += inc;
            t->ph[1] += inc >> 1;
            o[i] = (int16_t)sat16(s);
        }
        break;
    }
    case M_GNOI: {                     /* COLR S&H META MSPR PDEP PDEC GRIT - */
        int32_t env = (int32_t)(t->menv[0] >> 15);
        int32_t pq = pitch + ((P7(t, P_SYN + 4) * 24 * env) >> 15);
        t->menv[0] = mulu_hi(t->menv[0], T_ENV_COEF[P7(t, P_SYN + 5)], 31);
        int32_t colr = PB8(t, P_SYN + 0) >> 8;       /* -64..63: darker (a lowpass falling to ~300 Hz) or thinner
                                                       * (a highpass rising to ~5 kHz) */
        int32_t a = colr < 0 ? T_OP[CLAMP(127 + colr * 3 / 2, 20, 127)] : T_OP[CLAMP(colr * 8 / 5, 0, 102)];
        int32_t sh = P7(t, P_SYN + 1);
        uint32_t shinc = sh ? inc_of(pq - 24 * 64 + sh * 32) : 0;
        int32_t meta = T_VOL[P7(t, P_SYN + 2)];
        int32_t spread = 256 + P7(t, P_SYN + 3) * 6;
        int32_t grit = P7(t, P_SYN + 6);
        uint32_t minc[6];
        if (meta) {
            uint32_t base = inc_of(pq) >> 2;                     /* 1 at C4-ish: scale the 808's set by the note */
            for (int k = 0; k < 6; k++) minc[k] = mulu_hi(base, (uint32_t)METAL_HZ[k] * (uint32_t)spread, 13);
        }
        if (!t->ms[2]) t->ms[2] = 1;
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t n;
            int clock = 1;
            if (sh) {
                uint32_t p = t->ph[0];
                t->ph[0] += shinc;
                clock = t->ph[0] < p;
            }
            if (clock) {
                int32_t w = (int32_t)noise_step(&t->lfsr) >> 16;
                if (grit) {            /* GRIT: towards the 93-step loop of an 8-bit console's short noise */
                    uint32_t x = (uint32_t)t->ms[2];
                    uint32_t bit = (x ^ (x >> 6)) & 1u;
                    x = (x >> 1) | (bit << 14);
                    t->ms[2] = (int32_t)x;
                    int32_t nes = (x & 1u) ? 24000 : -24000;
                    w += ((nes - w) * grit) >> 7;
                }
                t->ms[0] = w;
            }
            n = t->ms[0];
            /* colour: one-pole low (below 64) or the rest of it (above) */
            t->ms[1] += ((n - t->ms[1]) * a) >> 15;
            int32_t s = colr < 0 ? t->ms[1] : colr > 0 ? n - t->ms[1] : n;
            if (meta) {
                int32_t m6 = 0;
                for (int k = 0; k < 6; k++) { t->ph[2 + k] += minc[k]; m6 += (t->ph[2 + k] >> 31) ? 5400 : -5400; }
                s = s + (((m6 - s) * meta) >> 15);
            }
            o[i] = (int16_t)sat16(s);
        }
        break;
    }
    case M_SID: {                      /* WAV1 PW PWM WAV2 DTN2 FINE MODE MIX */
        int w1 = SID_SHAPES[PN(t, P_SYN + 0, 8)], w2 = SID_SHAPES[PN(t, P_SYN + 3, 8)];
        /* the PWM: its own slow triangle */
        t->ms[3] += 900;
        int32_t lt = (t->ms[3] >> 8) & 0x1FF; lt = lt < 256 ? lt : 511 - lt;   /* 0..255 */
        int32_t pw = (P7(t, P_SYN + 1) << 5) + (((lt - 128) * P7(t, P_SYN + 2)) >> 2);
        pw = CLAMP(pw, 64, 4032);
        uint32_t inc1 = inc_of(pitch);
        int32_t d2 = ((P7(t, P_SYN + 4) - 64) * 24 / 64) * 64 + (PB8(t, P_SYN + 5) >> 8);
        uint32_t inc2 = inc_of(pitch + d2);
        int mode = PN(t, P_SYN + 6, 4);
        int32_t mix = P7(t, P_SYN + 7);
        for (int i = 0; i < M2K_SUB; i++) {
            uint32_t p2 = t->ph[1];
            t->ph[1] += inc2;
            int wrap2 = t->ph[1] < p2;
            uint32_t p1o = t->ph[0];
            t->ph[0] += inc1;
            if ((mode & 1) && wrap2) t->ph[0] = 0;
            /* noise: clocked when bit 27 of the accumulator rises, as the chip clocks it on bit 19 of 24 */
            if ((t->ph[0] ^ p1o) & (1u << 27) && (t->ph[0] & (1u << 27))) t->ms[0] = (int32_t)(noise_step(&t->lfsr) >> 20);
            if ((t->ph[1] ^ p2) & (1u << 27) && (t->ph[1] & (1u << 27))) t->ms[1] = (int32_t)(noise_step(&t->lfsr) >> 20);
            int32_t v1 = 0xFFF, v2 = 0xFFF;
            uint32_t msb1 = t->ph[0] >> 31, msb2 = t->ph[1] >> 31;
            if (mode & 2) msb1 ^= msb2;
            if (w1 & 1) v1 &= (int32_t)(((t->ph[0] << 1) ^ (msb1 ? 0xFFFFFFFFu : 0)) >> 20);
            if (w1 & 2) v1 &= (int32_t)(t->ph[0] >> 20);
            if (w1 & 4) v1 &= (int32_t)(t->ph[0] >> 20) >= pw ? 0xFFF : 0;
            if (w1 & 8) v1 &= t->ms[0];
            if (w2 & 1) v2 &= (int32_t)(((t->ph[1] << 1) ^ (msb2 ? 0xFFFFFFFFu : 0)) >> 20);
            if (w2 & 2) v2 &= (int32_t)(t->ph[1] >> 20);
            if (w2 & 4) v2 &= (int32_t)(t->ph[1] >> 20) >= pw ? 0xFFF : 0;
            if (w2 & 8) v2 &= t->ms[1];
            int32_t s1 = (v1 - 2048) * 16, s2 = (v2 - 2048) * 16;
            o[i] = (int16_t)sat16(s1 + (((s2 - s1) * mix) >> 7));
        }
        break;
    }
    case M_FMS:
    case M_FMD: {                      /* RAT1 DEP1 DEC1 RAT2 DEP2 DEC2 FDBK ALGO|SWEP */
        uint32_t inc = inc_of(pitch);
        int32_t e1 = (int32_t)(t->menv[0] >> 15), e2 = (int32_t)(t->menv[1] >> 15);
        int d1 = P7(t, P_SYN + 2), d2 = P7(t, P_SYN + 5);
        if (d1 < 127) t->menv[0] = mulu_hi(t->menv[0], T_ENV_COEF[d1], 31);
        if (d2 < 127) t->menv[1] = mulu_hi(t->menv[1], T_ENV_COEF[d2], 31);
        uint32_t r1 = T_RATIO[PN(t, P_SYN + 0, 32)], r2 = T_RATIO[PN(t, P_SYN + 3, 32)];
        int algo = 0;
        if (m == M_FMD) {              /* the first modulator's ratio sweeps in from SWEP, with its envelope */
            int32_t sw = PB8(t, P_SYN + 7) >> 8;
            int32_t f = 256 + ((sw * 16 * e1) >> 15);
            r1 = (r1 * (uint32_t)CLAMP(f, 8, 4096)) >> 8;
        } else {
            algo = PN(t, P_SYN + 7, 4);
        }
        uint32_t i1 = mulu_hi(inc, r1, 8), i2 = mulu_hi(inc, r2, 8);
        uint32_t k1 = mulu_hi(T_FMK[P7(t, P_SYN + 1)], (uint32_t)e1, 15);
        uint32_t k2 = mulu_hi(T_FMK[P7(t, P_SYN + 4)], (uint32_t)e2, 15);
        uint32_t kf = T_FMK[P7(t, P_SYN + 6)] >> 1;
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t m2 = sinp(t->ph[2] + (uint32_t)(t->ms[1] + t->ms[0]) * (kf >> 1));
            t->ms[1] = t->ms[0];
            t->ms[0] = m2;
            int32_t s;
            if (algo == 0) {           /* M2 > M1 > C */
                int32_t m1 = sinp(t->ph[1] + (uint32_t)m2 * k2);
                s = sinp(t->ph[0] + (uint32_t)m1 * k1);
            } else if (algo == 1) {    /* M1 + M2 > C */
                int32_t m1 = sinp(t->ph[1]);
                s = sinp(t->ph[0] + (uint32_t)m1 * k1 + (uint32_t)m2 * k2);
            } else if (algo == 2) {    /* M1 > C, M2 as a second carrier */
                int32_t m1 = sinp(t->ph[1]);
                s = (sinp(t->ph[0] + (uint32_t)m1 * k1) + ((m2 * (e2 >> 1)) >> 14)) >> 1;
            } else {                   /* M1 > C, ring with M2 */
                int32_t m1 = sinp(t->ph[1]);
                int32_t c = sinp(t->ph[0] + (uint32_t)m1 * k1);
                s = c + ((((c * m2) >> 15) - c) * (e2 >> 7) >> 8);
            }
            t->ph[0] += inc;
            t->ph[1] += i1;
            t->ph[2] += i2;
            o[i] = (int16_t)s;
        }
        break;
    }
    case M_SAW: {                      /* UNIL UNIW UNIX DRFT SUBX SUB1 SUB2 TUNE */
        /* after the Monomachine's SWAVE-SAW, measured on its OS (tools/ref/swave_saw*.spec): a rising saw, free
         * running; the unison pair at -0.42 / +0.43 c a step of UNIW, each as loud as UNIL says; the extended pair at
         * half that distance, UNIL x UNIX; a square an octave down on its own curve; all through one pole of highpass
         * that follows the note (cutoff 1.0 .. 2.6 x f0: the saw's hollow fundamental); then sines an octave and two
         * down. Against GND-SIN's sine the saw stands at 0.158, SUB1 at 0.217, SUB2 at 0.082. DRFT is MONO2K's own. */
        int unil = P7(t, P_SYN + 0), uniw = P7(t, P_SYN + 1), drift = P7(t, P_SYN + 3);
        int32_t pq = pitch + (P7(t, P_SYN + 7) - 64) * 2;               /* TUNE: +-2 semitones, 3.125 c a step */
        if (drift) {
            t->ms[4] += (((int32_t)(rnd32() >> 20) - 2048) * drift) >> 9;
            t->ms[4] = CLAMP(t->ms[4], -drift * 8, drift * 8);
            pq += t->ms[4] >> 6;
        }
        uint32_t inc[5], inv[5];
        inc[0] = inc_of(pq);
        inc[1] = mulu_hi(inc[0], T_SW1LO[uniw], 24);
        inc[2] = mulu_hi(inc[0], T_SW1HI[uniw], 24);
        inc[3] = mulu_hi(inc[0], T_SW2LO[uniw], 24);
        inc[4] = mulu_hi(inc[0], T_SW2HI[uniw], 24);
        int nv = unil ? 5 : 1;
        for (int k = 0; k < nv; k++) inv[k] = 0xFFFFFFFFu / (inc[k] | 1);
        int32_t g1 = 5166 * unil / 127, g2 = (g1 * P7(t, P_SYN + 2)) / 127;
        int32_t gx = (3369 * T_SWSUBX[P7(t, P_SYN + 4)]) >> 15;
        int32_t gs1 = 7097 * P7(t, P_SYN + 5) / 127, gs2 = 2701 * P7(t, P_SYN + 6) / 127;
        uint32_t ih = inc[0] >> 1, invh = 0xFFFFFFFFu / (ih | 1);
        int32_t a = T_SWHP[CLAMP(pq >> 6, 0, 127)];
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t s = blep_saw(t->ph[0], inc[0], inv[0]) * 5166;
            if (unil) {
                s += (blep_saw(t->ph[1], inc[1], inv[1]) + blep_saw(t->ph[2], inc[2], inv[2])) * g1;
                if (g2) s += (blep_saw(t->ph[3], inc[3], inv[3]) + blep_saw(t->ph[4], inc[4], inv[4])) * g2;
            }
            s >>= 15;                                                     /* rising, a drop at the reset, as the original */
            if (gx) s += ((blep_saw(t->ph[5], ih, invh) - blep_saw(t->ph[5] + 0x80000000u, ih, invh)) * gx) >> 15;
            for (int k = 0; k < nv; k++) t->ph[k] += inc[k];
            /* the highpass: y = a (y + x - x') */
            t->ms[0] = (int32_t)(((int64_t)a * (t->ms[0] + s - t->ms[1])) >> 15);
            t->ms[1] = s;
            int32_t y = t->ms[0];
            if (gs1) y += (sinp(t->ph[5]) * gs1) >> 15;
            if (gs2) y += (sinp(t->ph[6]) * gs2) >> 15;
            t->ph[5] += ih;
            t->ph[6] += inc[0] >> 2;
            o[i] = (int16_t)sat16(y);
        }
        break;
    }
    case M_ENS: {                      /* INT1 INT2 INT3 DTUN SHPE PW LEV SUB */
        int32_t dt = P8(t, P_SYN + 3) >> 9;
        int32_t iv[4] = {0, PN(t, P_SYN + 0, 25), PN(t, P_SYN + 1, 25), PN(t, P_SYN + 2, 25)};
        int32_t shp = P7(t, P_SYN + 4), pw = 0x80000000u - (uint32_t)P7(t, P_SYN + 5) * 0x00F00000u;
        int32_t lev = T_VOL[P7(t, P_SYN + 6)], sub = T_VOL[P7(t, P_SYN + 7)] >> 1;
        uint32_t inc[4], inv[4];
        for (int k = 0; k < 4; k++) {
            inc[k] = inc_of(pitch + iv[k] * 64 + ((k & 1) ? dt : -dt) * (k + 1) / 2);
            inv[k] = 0xFFFFFFFFu / (inc[k] | 1);
        }
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t s = 0;
            for (int k = 0; k < 4; k++) {
                int32_t v = blep_saw(t->ph[k], inc[k], inv[k]);
                if (shp) v -= (blep_saw(t->ph[k] + (uint32_t)pw, inc[k], inv[k]) * shp) >> 7;
                s += k ? (v * lev) >> 15 : v;
                t->ph[k] += inc[k];
            }
            s = (s * 13000) >> 15;
            if (sub) {                 /* the phase wraps: unsigned, as a signed overflow is undefined */
                s += (((uint32_t)t->ms[5] & 0x80000000u) ? sub : -sub) >> 1;
                t->ms[5] = (int32_t)((uint32_t)t->ms[5] + (inc[0] >> 1));
            }
            o[i] = (int16_t)sat16(s);
        }
        break;
    }
    case M_WAV: {                      /* WAVE SCAN STIM SYNC BITS DTUN MIX2 STEP */
        int32_t env = (int32_t)(t->menv[0] >> 15);
        if (P7(t, P_SYN + 2) < 127) t->menv[0] = mulu_hi(t->menv[0], T_ENV_COEF[P7(t, P_SYN + 2)], 31);
        int32_t pos = (P8(t, P_SYN + 0) * 31) / 127 + ((PB8(t, P_SYN + 1) * 2 * env) >> 15) * 31 / 128;  /* Q8 wave index */
        pos = CLAMP(pos, 0, 31 * 256 - 1);
        const int16_t *wa = &T_WAVES[(pos >> 8) * 256], *wb = &T_WAVES[CLAMP((pos >> 8) + 1, 0, 31) * 256];
        int32_t wf = pos & 255;
        uint32_t inc = inc_of(pitch);
        uint32_t sinc = mulu_hi(inc, 256 + (uint32_t)P7(t, P_SYN + 3) * 14, 8);
        uint32_t inc2 = inc_of(pitch + (PB8(t, P_SYN + 5) >> 5));
        int bits = 16 - (P7(t, P_SYN + 4) * 14 >> 7);
        int32_t mix2 = T_VOL[P7(t, P_SYN + 6)];
        int stp = P7(t, P_SYN + 7) * 6 >> 7;   /* 0: smooth .. 5: 8 steps per cycle */
        uint32_t qmask = stp ? ~((1u << (24 + stp)) - 1) : 0xFFFFFFFFu;
        for (int i = 0; i < M2K_SUB; i++) {
            uint32_t pm = t->ph[2];
            t->ph[2] += inc;
            if (t->ph[2] < pm) t->ph[0] = 0;           /* hard sync to the master */
            uint32_t p = t->ph[0] & qmask;
            int32_t j = (int32_t)(p >> 24), f = stp ? 0 : (int32_t)((p >> 16) & 255);
            int32_t a = wa[j] + (((wa[(j + 1) & 255] - wa[j]) * f) >> 8);
            int32_t b = wb[j] + (((wb[(j + 1) & 255] - wb[j]) * f) >> 8);
            int32_t s = a + (((b - a) * wf) >> 8);
            if (mix2) {
                uint32_t q = t->ph[1] & qmask;
                int32_t j2 = (int32_t)(q >> 24);
                s += (wa[j2] * mix2) >> 15;
                s = (s * 3) >> 2;
            }
            t->ph[0] += sinc;
            t->ph[1] += inc2;
            if (bits < 16) s = (s >> (16 - bits)) * (1 << (16 - bits));     /* a multiply: no left shift of a negative */
            o[i] = (int16_t)sat16(s);
        }
        break;
    }
    case M_VOX: {                      /* VOWL VSWP VTIM SHFT BRTH RESO BUZZ VIBR */
        int32_t env = (int32_t)(t->menv[0] >> 15);
        t->menv[0] = mulu_hi(t->menv[0], T_ENV_COEF[P7(t, P_SYN + 2)], 31);
        int32_t vpos = (P8(t, P_SYN + 0) * 7) / 127 + ((PB8(t, P_SYN + 1) * 2 * env) >> 15) * 7 / 64;  /* Q8, 0..7 */
        vpos = CLAMP(vpos, 0, 7 * 256);
        int vi = vpos >> 8, vf = vpos & 255, vj = vi < 7 ? vi + 1 : 7;
        int32_t shift = PB8(t, P_SYN + 3) >> 6;      /* +-256 q3 = about an octave */
        int32_t r = T_RES_R[64 + (P7(t, P_SYN + 5) >> 1)];
        for (int k = 0; k < 3; k++) {
            int32_t c = T_VOWEL[vi][k] + (((T_VOWEL[vj][k] - T_VOWEL[vi][k]) * vf) >> 8) + shift;
            svf_set(&t->fm[k], c, r);
        }
        /* vibrato: 5.5 Hz, in after a moment */
        t->ph[3] += 8570000u;                       /* 5.5 Hz at the control rate */
        if (t->ms[3] < 4096) t->ms[3] += 16;
        int32_t vib = (sinp(t->ph[3]) * P7(t, P_SYN + 7) >> 15) * t->ms[3] >> 12;   /* +-127 */
        uint32_t inc = inc_of(pitch + vib / 2);
        uint32_t inv = 0xFFFFFFFFu / (inc | 1);
        int32_t br = T_VOL[P7(t, P_SYN + 4)], buzz = P7(t, P_SYN + 6);
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t src = blep_saw(t->ph[0], inc, inv);
            if (buzz) src -= (blep_saw(t->ph[0] + 0x30000000u, inc, inv) * buzz) >> 7;
            if (br) src += ((((int32_t)noise_step(&t->lfsr) >> 16) - src) * br) >> 15;
            t->ph[0] += inc;
            int32_t b0, b1, b2;
            svf_tick(&t->fm[0], src, 0, &b0);
            svf_tick(&t->fm[1], src, 0, &b1);
            svf_tick(&t->fm[2], src, 0, &b2);
            int32_t s = (((b0 * t->fm[0].k) >> 12) + (((b1 * t->fm[1].k) >> 12) * 5 >> 3) + (((b2 * t->fm[2].k) >> 12) * 3 >> 3));
            o[i] = (int16_t)sat16(s);
        }
        break;
    }
    default:
        m_set(o, 0, sizeof(int16_t) * M2K_SUB);
        break;
    }
}

/* -------------------------------------------------------------------------------------------- envelopes */

static void env_block(trk_t *t)
{
    /* amp: attack, hold (127: while the gate is open), decay (127: none), release on note-off, timed as the
     * Monomachine times them (measured, tools/ref) */
    switch (t->astage) {
    case 1:
        t->aenv += T_AATK_INC[t->eff[P_ATK]];
        if (t->aenv >= (1u << 30)) {
            t->aenv = 1u << 30;
            t->astage = 2;
            int h = t->eff[P_HOLD];
            t->hold = h == 127 ? -1 : (int32_t)T_AHOLD[h];
        }
        break;
    case 2:
        if (t->hold < 0) { if (!t->gate) t->astage = 4; }
        else if (t->let_go) t->astage = 4;                  /* a key let go ends the hold, as on the Monomachine */
        else if (t->hold-- <= 0) t->astage = 3;
        break;
    case 3: {
        if (t->let_go) { t->astage = 4; break; }            /* and the decay */
        int d = P7(t, P_DEC);
        if (d < 127) t->aenv = mulu_hi(t->aenv, T_ADEC_COEF[d], 31);
        else if (!t->gate) t->astage = 4;
        if (t->aenv < (1u << 15)) { t->aenv = 0; t->astage = 0; }
        break;
    }
    case 4:
        t->aenv = mulu_hi(t->aenv, T_ADEC_COEF[P7(t, P_REL)], 31);
        if (t->aenv < (1u << 15)) { t->aenv = 0; t->astage = 0; }
        break;
    default:
        t->aenv = 0;
        break;
    }
    if (t->astage == 0) t->sounding = 0;
    /* filter: a straight attack, a straight decay to zero (127: stays), timed as the Monomachine's (measured) */
    if (t->fstage == 1) {
        t->fenv += T_FATK_INC[t->eff[P_FATK]];
        if (t->fenv >= (1u << 30)) { t->fenv = 1u << 30; t->fstage = 2; }
    } else if (t->fstage == 2) {
        uint32_t d = T_FDEC_DEC[P7(t, P_FDEC)];
        t->fenv = t->fenv > d ? t->fenv - d : 0;
    }
}

/* ---------------------------------------------------------------------------------------------- the LFOs */

static int32_t lfo_shape(int w, uint32_t ph, int32_t *rnd)
{
    int32_t x = (int32_t)(ph >> 16);
    switch (w) {
    case 0: return x < 32768 ? x * 2 - 32768 : (65535 - x) * 2 - 32768;
    case 1: return sinp(ph);
    case 2: return ph < 0x80000000u ? 32767 : -32767;
    case 3: return 32767 - x;                                   /* saw, falling */
    case 4: { int32_t y = 65535 - x; y = (y * y) >> 16; y = (y * y) >> 16; return y >> 1; }   /* exp, unipolar */
    case 5: return x >> 1;                                      /* ramp, unipolar */
    default: return *rnd;
    }
}

static uint32_t bpm10;                 /* tempo in 1/10 bpm, set by the sequencer */

static void lfo_block(int ti)
{
    trk_t *t = &trk[ti];
    for (int l = 0; l < 3; l++) {
        const int b = P_LFO + 8 * l;
        int dest = P7(t, b + L_DEST);
        if (dest == 0 || dest > P_COUNT) { t->lval[l] = 0; mon.lfo[ti][l] = 0; continue; }
        int wave = PN(t, b + L_WAVE, 7), trig = PN(t, b + L_TRIG, 5);
        uint32_t mult = 1u << PN(t, b + L_MULT, 8);
        uint32_t inc = (uint32_t)P7(t, b + L_SPD) * mult * bpm10 * 10u;    /* about SPD * MULT * bpm / 15360 Hz */
        uint32_t p = t->lph[l];
        if (!t->ldone[l]) {
            t->lph[l] += inc;
            if (t->lph[l] < p) {
                t->lrnd[l] = (int32_t)(rnd32() >> 16) - 32768;
                if (trig == 3) { t->ldone[l] = 1; t->lph[l] = 0xFFFFFFFFu; }
            }
            if (trig == 4 && t->lph[l] >= 0x80000000u && p < 0x80000000u) { t->ldone[l] = 1; t->lph[l] = 0x80000000u; }
        }
        int32_t v = lfo_shape(wave, t->lph[l], &t->lrnd[l]);
        if (trig == 2) v = t->lhold[l];
        t->lval[l] = v;
        mon.lfo[ti][l] = v;
        int32_t depth = PB8(t, b + L_DPTH) >> 8;    /* -64..63 */
        int dt = PN(t, b + L_TRCK, M2K_TRACKS);
        trk[dt].mod_acc[dest - 1] += (v * depth) >> 6;
    }
}

static void lfo_trig(trk_t *t)
{
    for (int l = 0; l < 3; l++) {
        const int b = P_LFO + 8 * l;
        int trig = (t->eff[b + L_TRIG] * 5) >> 7;
        if (trig == 0) continue;
        if (trig == 2) {
            t->lhold[l] = lfo_shape((t->eff[b + L_WAVE] * 7) >> 7, t->lph[l], &t->lrnd[l]);
            continue;
        }
        t->lph[l] = (uint32_t)t->eff[b + L_PHAS] << 25;
        t->ldone[l] = 0;
    }
}

/* ---------------------------------------------------------------------------------------------- voices */

/* the store: the patterns, then the settings */
static struct {
    m2k_pattern_t p[M2K_PATTERNS];
    uint8_t set[S_COUNT];
} store_ POOL;
#define pat (store_.p)
static int cur_pat;
static uint8_t master_vol = 100;

static void voice_on(int ti, int note, int vel, int slide)
{
    trk_t *t = &trk[ti];
    t->note = (uint8_t)note;
    t->ptarget = note * 64;
    t->vel = (uint8_t)vel;
    if (slide && t->sounding) {
        t->slide = 1;
        t->gate = 1;
        return;
    }
    t->slide = 0;
    if (!t->sounding || t->eff[P_PORT] == 0) t->pitch = t->ptarget;
    t->gate = 1;
    t->let_go = 0;
    t->sounding = 1;
    t->astage = 1;
    t->fstage = 1;
    t->fenv = 0;
    if (t->eff[P_ATK] == 0) t->aenv = 0;      /* percussive: start from zero, the click is the point */
    mach_trig(t, pat[cur_pat].machine[ti]);
    lfo_trig(t);
    mon.trig[ti]++;
}

static void voice_off(int ti)
{
    trk_t *t = &trk[ti];
    t->gate = 0;
    t->gate_left = -1;
    t->let_go = 1;
}

/* the modulation effect, one sample: a chorus (7..17 ms, swept), a flanger (0.5..5 ms, fed back) or a phaser (four
 * allpasses swept from 0.1 to 0.9, fed back), all on one triangle LFO; mix 0..254 of the wet */
static inline int32_t mod_tick(trk_t *t, int16_t *buf, int32_t x, int type, uint32_t inc, int32_t dep, int32_t mix)
{
    t->mph += inc;
    int32_t tri = (int32_t)(t->mph >> 16);
    tri = tri < 32768 ? tri : 65535 - tri;                       /* 0..32767 */
    int32_t y;
    if (type == 2) {
        int32_t a = 3277 + (int32_t)(((int64_t)tri * (dep * 206)) >> 15);
        int32_t v = x + (t->mfb >> 1);
        for (int k = 0; k < 4; k++) {
            int32_t u = v - ((a * t->map[k]) >> 15);
            v = ((a * u) >> 15) + t->map[k];
            t->map[k] = CLAMP(u, -65536, 65536);
        }
        y = v;
        t->mfb = CLAMP(y, -65536, 65536);
    } else {
        int32_t d8 = type == 0 ? (309 << 8) + (int32_t)(((int64_t)tri * dep * 441) >> 14)    /* Q8 samples */
                               : (20 << 8) + (int32_t)(((int64_t)tri * dep * 200) >> 14);
        int32_t pos = ((int32_t)t->mw << 8) - d8;
        uint32_t j = (uint32_t)(pos >> 8);
        int32_t f = pos & 255;
        y = (buf[j & (MOD_LEN - 1)] * (256 - f) + buf[(j + 1) & (MOD_LEN - 1)] * f) >> 8;
        int32_t w = type == 1 ? x + ((y * 18000) >> 15) : x;
        buf[t->mw & (MOD_LEN - 1)] = (int16_t)sat16(w);
        t->mw++;
    }
    return x + (((y - x) * mix) >> 8);
}

#define EQ_K 7626007                   /* 1 / 2.2, Q24: the EQ band's damping */
#define T_CLIP 32244                   /* a track's ceiling, per channel after the pan: just under full at LEV 127 */

/* one control block of one track: modulation, envelopes, the machine, then the chain, summed into lr */
static void track_block(int ti, int32_t *L, int32_t *R, int32_t *dsend, int32_t *rsend)
{
    trk_t *t = &trk[ti];
    const m2k_pattern_t *pp = &pat[cur_pat];
    int m = pp->machine[ti];
    if (t->gate_left > 0 && --t->gate_left == 0) { t->gate = 0; t->gate_left = -1; }   /* the step's gate: hold and decay go on */
    env_block(t);
    if (!t->sounding && t->astage == 0) {
        t->amp0 = t->amp1 = 0;
        t->peak = 0;
        mon.aenv[ti] = 0;
        mon.astage[ti] = 0;
        return;
    }
    /* pitch: the note, its glide, the track's PTCH */
    int g = t->slide && t->eff[P_PORT] == 0 ? 40 : P7(t, P_PORT);
    t->pitch += (int32_t)(((int64_t)(t->ptarget - t->pitch) * T_GLIDE[g]) >> 16);
    int32_t pitch = t->pitch + (PB8(t, P_PTCH) >> 2);
    mach_block(ti, m, pitch);

    /* the filter follows the note (and the tuned pitch): a cutoff 2^((step - 16) / 8) x f0, HP at BASE, LP at BASE + WDTH; the envelope adds
     * 7 d|d| / 256 steps at full (d: BOFS / WOFS from their centre). Positions in 1/96 octave over note 0, Q8 */
    int32_t fe = (int32_t)(t->fenv >> 15);
    int32_t db = PB8(t, P_BOF), dw = PB8(t, P_WOF);
    int32_t ob = (int32_t)((((int64_t)7 * db * (db < 0 ? -db : db)) >> 16) * fe >> 15);
    int32_t ow = (int32_t)((((int64_t)7 * dw * (dw < 0 ? -dw : dw)) >> 16) * fe >> 15);
    int32_t sb = P8(t, P_BASE) + ob, sl = sb + P8(t, P_WDTH) + ow;          /* steps, Q8 */
    int32_t key = t->note * 8 * 256;
    if (m == M_GSIN) key += (PB8(t, P_SYN + 0) >> 7) * 32;               /* and GND-SIN's TUNE (chain_tune13) */
    int32_t ph = key + 12 * (sb - 16 * 256), pl = key + 12 * (sl - 16 * 256);
    int lq = P7(t, P_LPQ), hq = P7(t, P_HPQ);
    zsvf_set(&t->lp1, fg_at(T_FG, pl + T_LPC[lq]), T_LPK[lq]);
    zsvf_set(&t->hp2, fg_at(T_FG, ph + T_HPC[hq]), T_HPK[hq]);
    t->lp2a.G = t->lp2b.G = (int32_t)fg_at(T_FG1, pl + F_LP2 * 256);
    t->hp1.G = (int32_t)fg_at(T_FG1, ph + F_HP1 * 256);
    mon.cut_hp[ti] = CLAMP(((ph >> 8) * 1087 >> 10) - 132, 0, 1016);       /* on the screen's 20 Hz .. 20 kHz */
    mon.cut_lp[ti] = CLAMP(((pl >> 8) * 1087 >> 10) - 132, 0, 1016);
    /* DIST: headroom below 64, a gain into the clip above 72; the clip's ceiling falls from +5.8 dB to -3.2 dB of a
     * full-scale machine between 64 and 80. Between steps, interpolated */
    int32_t d8 = P8(t, P_DIST), di = d8 >> 8, df = d8 & 255, dn = di < 127 ? di + 1 : 127;
    int64_t dh = T_DIST_H[di] + ((((int64_t)T_DIST_H[dn] - T_DIST_H[di]) * df) >> 8);
    int32_t ceil = T_DIST_C[di] + (((T_DIST_C[dn] - T_DIST_C[di]) * df) >> 8);
    /* EQ: a band added to the signal, Q 2.2, +36 / -31 dB; a cut is the inverse of a boost as deep (its band widened
     * by the gain) */
    int eqv = P7(t, P_EQG), eq_on = eqv != 64;
    if (eq_on) {
        uint32_t gb = T_EQGAIN[eqv];
        uint32_t k = eqv > 64 ? EQ_K : (uint32_t)(((uint64_t)EQ_K * gb) >> 16);
        zsvf_set(&t->eq, T_EQG[P7(t, P_EQF)], k);
        int32_t c = (int32_t)(((int64_t)(gb - 65536) * EQ_K) >> 16);
        t->eqc = eqv > 64 ? c : -c;
    }
    /* amp */
    t->amp0 = t->amp1;
    t->amp1 = (int32_t)(t->aenv >> 15);
    mon.aenv[ti] = t->amp1;
    mon.astage[ti] = t->astage;
    int32_t vol = T_VOLK[P7(t, P_VOL)];
    int32_t acc = 256 + (t->vel > 100 ? (pp->glob[G_ACNT] * 2) : 0);
    vol = (vol * acc) >> 8;
    int32_t lev = T_VOL[P7(t, P_LEVL)];
    int srr = P7(t, P_SRR);
    uint32_t sinc = T_SRR_INC[srr];
    int bits = 16 - (P7(t, P_BITS) * 14 >> 7);
    int32_t fold = P7(t, P_FOLD), xr = P7(t, P_XOR), xm = (xr * xr * 2) & 0x7FFF;
    int pan = P7(t, P_PAN);
    int32_t gl = T_PAN[pan], gr = T_PAN[127 - pan];
    int32_t sd = P7(t, P_DSND) - 64, ds = sd * sd * 8;                      /* the send: (d / 64)^2, Q15 */
    int32_t rs = T_VOL[P7(t, P_SPU)];
    int mtype = PN(t, P_MODT, 3), mmix = P7(t, P_MODM) * 2, mdep = P7(t, P_MODD);
    uint32_t minc = T_MOD_INC[P7(t, P_MODR)];
    int32_t pk = 0;
    for (int i = 0; i < M2K_SUB; i++) {
        /* the filter: gain, EQ, LP1, the clip, the HP's two-pole (its resonance held by the ceiling too), its
         * one-pole, LP2 */
        int32_t u = (int32_t)CLAMP((blk[ti][i] * dh) >> 24, -(1 << 24), 1 << 24);
        if (eq_on) { int32_t b; zsvf_tick(&t->eq, u, 0, &b, 0); u = CLAMP(u + (int32_t)(((int64_t)b * t->eqc) >> 24), -(1 << 26), 1 << 26); }
        int32_t v;
        zsvf_tick(&t->lp1, u, &v, 0, 0);
        u = CLAMP(v, -ceil, ceil);
        zsvf_tick(&t->hp2, u, 0, 0, &u);
        t->hp2.s1 = CLAMP(t->hp2.s1, -ceil, ceil);
        t->hp2.s2 = CLAMP(t->hp2.s2, -ceil, ceil);
        u -= zop_lp(&t->hp1, u);
        int32_t s = zop_lp(&t->lp2b, zop_lp(&t->lp2a, u));
        /* MONO2K's own breakage */
        if (fold) s = fold16((sat16(s) * (256 + fold * 6)) >> 8);
        if (xr) s = (int32_t)(int16_t)((uint16_t)sat16(s) ^ (uint16_t)xm);
        int32_t a = t->amp0 + (((t->amp1 - t->amp0) * i) >> 4);
        s = (int32_t)(((int64_t)s * a) >> 15);
        if (mmix) s = mod_tick(t, mod_mem[ti], s, mtype, minc, mdep, mmix);
        s = (int32_t)(((int64_t)s * vol) >> 15);
        /* the track clips here, a channel at a time after the pan: one ceiling for both while the pan holds the
         * louder side at or under it */
        if (srr) {                     /* resampled, and joined by straight lines as the Monomachine does (its images
                                        * fall twice as far in dB as a plain hold's) */
            t->srr_acc += sinc;
            if (t->srr_acc >= 65536u) { t->srr_acc -= 65536u; t->srr_prev = t->srr_hold; t->srr_hold = s; }
            s = t->srr_prev + (int32_t)(((int64_t)(t->srr_hold - t->srr_prev) * (int32_t)t->srr_acc) >> 16);
        }
        if (bits < 16) s = (sat16(s) >> (16 - bits)) * (1 << (16 - bits));
        int32_t l = (int32_t)CLAMP(((int64_t)s * gl) >> 15, -T_CLIP, T_CLIP), r = (int32_t)CLAMP(((int64_t)s * gr) >> 15, -T_CLIP, T_CLIP);
        int32_t as = l < 0 ? -l : l, ar = r < 0 ? -r : r;
        if (ar > as) as = ar;
        if (as > pk) pk = as;
        L[i] += (l * lev) >> 15;
        R[i] += (r * lev) >> 15;
        int32_t m = CLAMP(s, -T_CLIP, T_CLIP);
        dsend[i] = (m * ds) >> 15;
        rsend[i] += (((m * rs) >> 15) * lev) >> 15;
    }
    t->peak = pk;
    if (pk > mon.peak[ti]) mon.peak[ti] = pk;
}
