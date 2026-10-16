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

#define DL_LEN 16384u                  /* bytes per track: 0.74 s at half rate, mu-law */

typedef struct {
    uint8_t note, gate, vel, sounding, slide;
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
    /* the chain */
    svf_t hp, lp, eq;
    int32_t eqk;
    uint32_t srr_acc;
    int32_t srr_hold;
    /* delay */
    uint8_t *dl;
    uint32_t dw;
    int32_t dt_q8;                     /* smoothed delay time in half-rate samples, Q8 */
    int32_t dacc, dprev, dcur, dhp, dlp;
    /* LFOs */
    uint32_t lph[3];
    int32_t lval[3], lhold[3], lrnd[3];
    uint8_t ldone[3];
    /* params: the kit with this trig's locks, and the modulation in 1/256 steps */
    uint8_t eff[P_COUNT];
    int32_t mod[P_COUNT], mod_acc[P_COUNT];
    int32_t peak;
} trk_t;

static trk_t trk[M2K_TRACKS];
static uint8_t dl_mem[M2K_TRACKS][DL_LEN] POOL;
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
    case M_SAW:
        if (P7(t, P_SYN + 7)) for (int i = 0; i < 7; i++) t->ph[i] = rnd32() >> (P7(t, P_SYN + 7) < 64 ? 3 : 0);
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
        int32_t pq = pitch + (PB8(t, P_SYN + 0) >> 2) + ((P7(t, P_SYN + 4) * 24 * env) >> 15);
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
        int32_t colr = PB8(t, P_SYN + 0) >> 8;       /* -64..63 */
        int32_t a = T_OP[CLAMP(64 + colr * 2, 0, 127)];
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
            int32_t s1 = (v1 - 2048) << 4, s2 = (v2 - 2048) << 4;
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
    case M_SAW: {                      /* DTUN VOIC MIX SUB SHPE DRFT OCTV STRT */
        int nv = 1 + PN(t, P_SYN + 1, 7);
        if (nv > 7) nv = 7;
        int32_t dt = P8(t, P_SYN + 0) >> 6;     /* 0..508: 1/64 semitone units, about +-1 semitone at the edge */
        int32_t side = T_VOL[P7(t, P_SYN + 2)], sub = T_VOL[P7(t, P_SYN + 3)] >> 1;
        int32_t shp = P7(t, P_SYN + 4), oct = T_VOL[P7(t, P_SYN + 6)] >> 1;
        int32_t drift = P7(t, P_SYN + 5);
        if (drift) {
            t->ms[4] += (((int32_t)(rnd32() >> 20) - 2048) * drift) >> 9;
            t->ms[4] = CLAMP(t->ms[4], -drift * 8, drift * 8);
        }
        uint32_t inc[7], inv[7];
        for (int k = 0; k < nv; k++) {
            inc[k] = inc_of(pitch + (SAW_SPREAD[k] * dt >> 4) + (t->ms[4] >> 6) * (k & 1 ? 1 : -1));
            inv[k] = 0xFFFFFFFFu / (inc[k] | 1);
        }
        uint32_t inco = inc[0] << 1, invo = 0xFFFFFFFFu / (inco | 1);
        int32_t norm = nv == 1 ? 32767 : (32767 * 2) / (nv + 1);
        for (int i = 0; i < M2K_SUB; i++) {
            int32_t s = 0;
            for (int k = 0; k < nv; k++) {
                int32_t v = blep_saw(t->ph[k], inc[k], inv[k]);
                if (shp) v -= (blep_saw(t->ph[k] + 0x80000000u, inc[k], inv[k]) * shp) >> 7;
                s += k ? (v * side) >> 15 : v;
                t->ph[k] += inc[k];
            }
            s = (s * norm) >> 15;
            if (oct) { s += (blep_saw(t->ph[7], inco, invo) * oct) >> 15; t->ph[7] += inco; }
            if (sub) { s += ((t->ms[5] & 0x80000000) ? sub : -sub) >> 1; t->ms[5] += (int32_t)(inc[0] >> 1); }
            o[i] = (int16_t)sat16(s);
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
            if (sub) { s += ((t->ms[5] & 0x80000000) ? sub : -sub) >> 1; t->ms[5] += (int32_t)(inc[0] >> 1); }
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
            if (bits < 16) s = (s >> (16 - bits)) << (16 - bits);
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
    /* amp: attack, hold (127: while the gate is open), decay (127: none), release on note-off */
    switch (t->astage) {
    case 1:
        t->aenv += T_ENV_INC[t->eff[P_ATK]];
        if (t->aenv >= (1u << 30)) {
            t->aenv = 1u << 30;
            t->astage = 2;
            int h = t->eff[P_HOLD];
            t->hold = h == 127 ? -1 : h == 0 ? 0 : (int32_t)((1u << 30) / T_ENV_INC[h]);
        }
        break;
    case 2:
        if (t->hold < 0) { if (!t->gate) t->astage = 4; }
        else if (t->hold-- <= 0) t->astage = 3;
        break;
    case 3: {
        int d = P7(t, P_DEC);
        if (d < 127) t->aenv = mulu_hi(t->aenv, T_ENV_COEF[d], 31);
        else if (!t->gate) t->astage = 4;
        if (t->aenv < (1u << 15)) { t->aenv = 0; t->astage = 0; }
        break;
    }
    case 4:
        t->aenv = mulu_hi(t->aenv, T_ENV_COEF[P7(t, P_REL)], 31);
        if (t->aenv < (1u << 15)) { t->aenv = 0; t->astage = 0; }
        break;
    default:
        t->aenv = 0;
        break;
    }
    if (t->astage == 0) t->sounding = 0;
    /* filter: attack, decay to zero (127: stays) */
    if (t->fstage == 1) {
        t->fenv += T_ENV_INC[t->eff[P_FATK]];
        if (t->fenv >= (1u << 30)) { t->fenv = 1u << 30; t->fstage = 2; }
    } else if (t->fstage == 2) {
        int d = P7(t, P_FDEC);
        if (d < 127) t->fenv = mulu_hi(t->fenv, T_ENV_COEF[d], 31);
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
}

/* one control block of one track: modulation, envelopes, the machine, then the chain, summed into lr */
static void track_block(int ti, int32_t *L, int32_t *R, int32_t *dsend, int32_t *rsend)
{
    trk_t *t = &trk[ti];
    const m2k_pattern_t *pp = &pat[cur_pat];
    int m = pp->machine[ti];
    if (t->gate_left > 0 && --t->gate_left == 0) voice_off(ti);
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

    /* filters: HP at base, LP at base + width, both moved by the envelope */
    int32_t fe = (int32_t)(t->fenv >> 15);
    int32_t base = P8(t, P_BASE) + ((PB8(t, P_BOF) * 4 * fe) >> 15);
    int32_t lpc = base + P8(t, P_WDTH) + ((PB8(t, P_WOF) * 4 * fe) >> 15);
    int32_t hq3 = base >> 5, lq3 = lpc >> 5;
    int hp_on = hq3 > 4, lp_on = lq3 < 1010 || P7(t, P_LPQ) > 0;
    if (hp_on) svf_set(&t->hp, hq3, T_RES_R[P7(t, P_HPQ)]);
    if (lp_on) svf_set(&t->lp, lq3, T_RES_R[P7(t, P_LPQ)]);
    mon.cut_hp[ti] = CLAMP(hq3, 0, 1016);
    mon.cut_lp[ti] = CLAMP(lq3, 0, 1016);
    /* EQ: a band around EQF, cut or boost */
    int32_t eqg = PB8(t, P_EQG) >> 8;
    int eq_on = eqg != 0;
    if (eq_on) {
        svf_set(&t->eq, P7(t, P_EQF) * 8, 2048);
        t->eqk = eqg < 0 ? eqg * 64 : eqg * 192;   /* Q12: -1 .. +3 */
    }
    /* amp */
    t->amp0 = t->amp1;
    t->amp1 = (int32_t)(t->aenv >> 15);
    mon.aenv[ti] = t->amp1;
    mon.astage[ti] = t->astage;
    int32_t vol = (T_VOL[P7(t, P_VOL)] * T_VOL[P7(t, P_LEVL)]) >> 15;
    int32_t acc = 256 + (t->vel > 100 ? (pp->glob[G_ACNT] * 2) : 0);
    vol = (vol * acc) >> 8;
    int32_t dist = P7(t, P_DIST), dg = 256 + dist * 24;
    int32_t srr = P7(t, P_SRR), sstep = (128 - srr) * (128 - srr) * 4;
    int bits = 16 - (P7(t, P_BITS) * 14 >> 7);
    int32_t fold = P7(t, P_FOLD), xr = P7(t, P_XOR), xm = (xr * xr * 2) & 0x7FFF;
    int pan = P7(t, P_PAN);
    int32_t gl = T_PAN[pan], gr = T_PAN[127 - pan];
    int32_t ds = T_VOL[P7(t, P_DSND)], rs = T_VOL[P7(t, P_SPU)];
    int32_t pk = 0;
    for (int i = 0; i < M2K_SUB; i++) {
        int32_t s = blk[ti][i];
        if (hp_on) { int32_t h; svf_tick(&t->hp, s, &h, 0); s = h; }
        if (lp_on) s = svf_tick(&t->lp, s, 0, 0);
        if (fold) s = fold16((s * (256 + fold * 6)) >> 8);
        if (xr) s = (int32_t)(int16_t)((uint16_t)sat16(s) ^ (uint16_t)xm);
        if (dist) s = sat_soft((s * dg) >> 8);
        int32_t a = t->amp0 + (((t->amp1 - t->amp0) * i) >> 4);
        s = (s * a) >> 15;
        s = (s * vol) >> 15;
        if (eq_on) { int32_t b; svf_tick(&t->eq, s, 0, &b); s += (int32_t)(((int64_t)b * t->eqk) >> 12); }
        if (srr) {
            t->srr_acc += (uint32_t)sstep;
            if (t->srr_acc >= 65536u) { t->srr_acc -= 65536u; t->srr_hold = s; }
            s = t->srr_hold;
        }
        if (bits < 16) s = (sat16(s) >> (16 - bits)) << (16 - bits);
        int32_t as = s < 0 ? -s : s;
        if (as > pk) pk = as;
        L[i] += (s * gl) >> 15;
        R[i] += (s * gr) >> 15;
        dsend[i] = (s * ds) >> 15;
        rsend[i] += (s * rs) >> 15;
    }
    t->peak = pk;
    if (pk > mon.peak[ti]) mon.peak[ti] = pk;
}
