/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's effects. Each track has its own delay, as on the Monomachine (time in 1/64 notes, feedback, and a
 * base / width band in the loop); here it runs at half rate in 8-bit mu-law, which is the memory budget and the
 * colour. The bus is the lofi part: an SPU-flavoured reverb at 22 kHz, a beat-repeat glitch, tape wow and flutter,
 * a 4-bit ADPCM crunch, hiss and crackle, and a drive into the soft clip. */

static uint8_t mulaw_enc(int32_t x)
{
    int sign = 0;
    if (x < 0) { sign = 0x80; x = -x; }
    if (x > 32635) x = 32635;
    x += 0x84;
    int exp = 7;
    for (int mask = 0x4000; (x & mask) == 0 && exp > 0; exp--, mask >>= 1)
        ;
    int man = (x >> (exp + 3)) & 0x0F;
    return (uint8_t)~(sign | (exp << 4) | man);
}

static uint32_t sps_q8 = 1411200;      /* samples per sixteenth, Q8; the sequencer sets it */
static uint8_t tape_rolling;           /* the sequencer runs or a voice sounds: the hiss and dust follow it */

/* one line's sample at a fractional position, Q8 of a line sample */
static inline int32_t dl_read(const uint8_t *b, uint32_t mask, int32_t pos)
{
    uint32_t j = (uint32_t)(pos >> 8);
    int32_t f = pos & 255;
    return (T_MULAW[b[j & mask]] * (256 - f) + T_MULAW[b[(j + 1) & mask]] * f) >> 8;
}

/* the track's delay, after the Monomachine's (measured): DTIM in 256ths of a note plus 7.6 ms; the send (d / 64)^2,
 * negative keeping the source's side, positive ping-pong; feedback DFB / 64, unity at 64 and saturating above; the
 * echoes through a one-pole HP at DBAS and a one-pole LP at DBAS + DWID. The line runs at half rate in 8-bit mu-law;
 * a time longer than the line slows its clock instead, the echoes darker, as a tape or a BBD would. Ping-pong
 * keeps a sum and a difference line, each half the memory */
static void delay_block(int ti, const int32_t *in, int32_t *L, int32_t *R)
{
    trk_t *t = &trk[ti];
    int dual = P7(t, P_DSND) > 64;
    if (dual != t->dmode) {            /* the lines change their layout: start them empty */
        m_set(t->dl, 0x7F, DL_LEN);
        t->dmode = (uint8_t)dual;
        t->dacc[0] = t->dacc[1] = t->dn = 0;
        t->dhp[0] = t->dhp[1] = t->dlp[0] = t->dlp[1] = 0;
        t->dquiet = DL_LEN;
    }
    uint32_t n = dual ? DL_LEN / 2 : DL_LEN, mask = n - 1;
    /* nothing sent, every entry of the line written silent since, the filters settled (an integer one-pole can stall a
     * step or two off zero): nothing to hear, nothing to work out (most tracks, most of the time; the FM-1's time) */
    int quiet = t->dquiet >= n;
    for (int i = 0; quiet && i < M2K_SUB; i++) quiet = in[i] == 0;
    if (quiet) {
        int32_t s = 0;
        for (int c = 0; c < 2; c++) {
            int32_t v[4] = {t->dhp[c], t->dlp[c], t->dprev[c], t->dacc[c]};
            for (int k = 0; k < 4; k++) s |= v[k] > 3 || v[k] < -3;
        }
        if (!s) {
            t->dhp[0] = t->dhp[1] = t->dlp[0] = t->dlp[1] = t->dprev[0] = t->dprev[1] = t->dacc[0] = t->dacc[1] = 0;
            mon.dly[ti] = 0;
            return;
        }
    }
    /* the time, half-rate samples Q8: whole samples once it settles (a read between two samples dulls the top) */
    int32_t target = ((int32_t)((uint32_t)P7(t, P_DTIM) * (sps_q8 >> 5)) + (167 << 8) + 128) & ~255;
    int32_t dd = target - t->dt_q8;
    t->dt_q8 = dd > -64 && dd < 64 ? target : t->dt_q8 + (dd >> 5);
    uint32_t th = (uint32_t)t->dt_q8 >> 8, room = n - 4;
    uint32_t rate = th <= room ? 65536u : (room << 16) / th;                  /* the line's clock, Q16 */
    int32_t back = th <= room ? t->dt_q8 : (int32_t)(room << 8);               /* the delay in line samples, Q8 */
    int32_t fb = P7(t, P_DFB) * 512;
    int dbas = P7(t, P_DBAS);
    int32_t ahp = T_DLY_HP[dbas], alp = T_DLY_LP[CLAMP(dbas + P7(t, P_DWID), 0, 127)];
    int pan = P7(t, P_PAN);
    int32_t gl = T_PAN[pan], gr = T_PAN[127 - pan], lev = T_VOL[P7(t, P_LEVL)];
    uint8_t *b = t->dl;
    int32_t pk = 0;
    for (int i = 0; i < M2K_SUB; i += 2) {
        int32_t x = (in[i] + in[i + 1]) >> 1;
        int32_t pos = (int32_t)((t->dw << 8) + (t->dph >> 8)) - back;
        int32_t e[2];
        for (int c = 0; c <= dual; c++) {
            int32_t y = dl_read(b + c * n, mask, pos);
            t->dhp[c] += ((y - t->dhp[c]) * ahp) >> 15;
            t->dlp[c] += ((y - t->dhp[c] - t->dlp[c]) * alp) >> 15;
            e[c] = t->dlp[c];
        }
        int32_t l, r;
        if (dual) {                    /* sum and difference: the echo crosses sides on every pass */
            /* the echo comes off its filters past 16 bits and DFB runs to 1.75: its product takes 64 bits */
            t->dacc[0] += ((x * (gl + gr)) >> 15) + (int32_t)(((int64_t)e[0] * fb) >> 15);
            t->dacc[1] += ((x * (gl - gr)) >> 15) - (int32_t)(((int64_t)e[1] * fb) >> 15);
            l = (e[0] - e[1]) >> 1;
            r = (e[0] + e[1]) >> 1;
        } else {
            t->dacc[0] += x + (int32_t)(((int64_t)e[0] * fb) >> 15);
            l = (e[0] * gl) >> 15;
            r = (e[0] * gr) >> 15;
        }
        t->dn++;
        t->dph += rate;
        if (t->dph >= 65536u) {        /* the clock ticks: the inputs since the last tick, averaged, go in */
            t->dph -= 65536u;
            int loud = 0;
            for (int c = 0; c <= dual; c++) {
                uint8_t q = mulaw_enc(t->dacc[c] / t->dn);
                b[c * n + (t->dw & mask)] = q;
                loud |= q != 0xFF && q != 0x7F;           /* mu-law's two silences */
            }
            t->dquiet = loud ? 0 : t->dquiet < 0x40000000u ? t->dquiet + 1 : t->dquiet;
            t->dw++;
            t->dacc[0] = t->dacc[1] = t->dn = 0;
        }
        l = (l * lev) >> 15;
        r = (r * lev) >> 15;
        L[i] += (t->dprev[0] + l) >> 1;
        R[i] += (t->dprev[1] + r) >> 1;
        L[i + 1] += l;
        R[i + 1] += r;
        t->dprev[0] = l;
        t->dprev[1] = r;
        int32_t a = (l < 0 ? -l : l) + (r < 0 ? -r : r);
        if (a > pk) pk = a;
    }
    mon.dly[ti] = pk;
}

/* ------------------------------------------------------------------------------------------------ the bus */

#define GL_LEN 16384u
static int16_t gl_buf[GL_LEN][2] POOL;
static int16_t tp_buf[2048][2] POOL;
static const uint16_t CB_LEN[4] = {557, 593, 641, 677};
static const uint16_t AP_LEN[4] = {225, 341, 248, 367};
static int16_t cb_buf[557 + 593 + 641 + 677] POOL;
static int16_t ap_buf[225 + 341 + 248 + 367] POOL;
static const int8_t ADPCM_IDX[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

static struct {
    /* reverb */
    uint16_t cb_pos[4], ap_pos[4];
    int32_t cb_lp[4];
    int32_t rv_acc, rv_l, rv_r;
    /* glitch */
    uint32_t gw, g_start, g_len, g_left, g_pos, g_rate;   /* g_pos, g_rate: Q12 */
    uint8_t g_on, g_mode, g_phase;
    int32_t g_fade;
    int32_t gacc_l, gacc_r, gout_l, gout_r;
    /* tape */
    uint32_t tw, wow, flut;
    int32_t tlp_l, tlp_r;
    /* adpcm */
    int32_t ad_pred[2], ad_idx[2], ad_hold[2];
    uint16_t ad_acc;                   /* Q8 samples since the 4-bit sample was taken */
    /* hiss */
    uint32_t hs;
    int32_t hlp, crackle, hgain;       /* hgain: Q15, follows tape_rolling over ~0.3 s */
} bus;

static inline int32_t adpcm_step(int c, int32_t x)
{
    int32_t step = T_ADPCM_STEP[bus.ad_idx[c]], diff = x - bus.ad_pred[c];
    int code = 0;
    if (diff < 0) { code = 8; diff = -diff; }
    int32_t delta = step >> 3;
    if (diff >= step) { code |= 4; diff -= step; delta += step; }
    step >>= 1;
    if (diff >= step) { code |= 2; diff -= step; delta += step; }
    step >>= 1;
    if (diff >= step) { code |= 1; delta += step; }
    bus.ad_pred[c] = sat16(bus.ad_pred[c] + ((code & 8) ? -delta : delta));
    bus.ad_idx[c] = CLAMP(bus.ad_idx[c] + ADPCM_IDX[code & 7], 0, 88);
    return bus.ad_pred[c];
}

/* the sequencer calls this on every sixteenth: maybe start a repeat */
static void bus_step(void)
{
    const uint8_t *g = PAT(cur_pat)->glob;
    if (!g[G_GLCH] || bus.g_on) return;
    if ((rnd32() & 0xFFFF) >= T_GLCH_P[g[G_GLCH]]) return;     /* a sixteenth's chance: 0.3 % at 1, 3.9 % at 64, 50 % at 127 */
    static const uint8_t frac4[5] = {1, 2, 4, 8, 16};          /* 1/64 .. 1/4 in quarters of a sixteenth */
    int n = (g[G_GSIZ] * 5) >> 7;
    uint32_t half_step = sps_q8 >> 9;                           /* a sixteenth in half-rate samples */
    uint32_t len = (half_step * frac4[n]) >> 2;
    len = CLAMP(len, 64u, 8192u);
    uint32_t dur = half_step > len ? half_step : len * 2;
    if (rnd32() & 1) dur *= 2;
    dur = CLAMP(dur, len, GL_LEN - len - 256);
    bus.g_len = len;
    bus.g_start = bus.gw - len;
    bus.g_left = dur;
    bus.g_pos = 0;
    bus.g_rate = 4096;
    bus.g_mode = (uint8_t)(rnd32() & 3);
    bus.g_on = 1;
    bus.g_fade = 0;
}

static void bus_block(int32_t *L, int32_t *R, const int32_t *rsend, int16_t *out)
{
    const uint8_t *g = PAT(cur_pat)->glob;
    /* reverb at half rate */
    int verb = g[G_VERB];
    if (verb) {
        int32_t fb = 22938 + verb * 72;
        int32_t damp = T_OP_H[127 - g[G_DAMP]];
        for (int i = 0; i < M2K_SUB; i += 2) {
            int32_t x = (rsend[i] + rsend[i + 1]) >> 2;
            int32_t sum = 0, off = 0;
            for (int c = 0; c < 4; c++) {
                int16_t *b = &cb_buf[off];
                int32_t y = b[bus.cb_pos[c]];
                bus.cb_lp[c] += ((y - bus.cb_lp[c]) * damp) >> 15;
                b[bus.cb_pos[c]] = (int16_t)sat16(x + ((bus.cb_lp[c] * fb) >> 15));
                if (++bus.cb_pos[c] >= CB_LEN[c]) bus.cb_pos[c] = 0;
                sum += y;
                off += CB_LEN[c];
            }
            int32_t ch[2] = {sum, sum};                   /* the input came in at a quarter, for the combs' headroom */
            off = 0;
            for (int a = 0; a < 4; a++) {
                int16_t *b = &ap_buf[off];
                int c = a >> 1;
                int32_t d = b[bus.ap_pos[a]];
                int32_t v = ch[c] + (d >> 1);
                b[bus.ap_pos[a]] = (int16_t)sat16(v);
                ch[c] = d - (v >> 1);
                if (++bus.ap_pos[a] >= AP_LEN[a]) bus.ap_pos[a] = 0;
                off += AP_LEN[a];
            }
            bus.rv_l = ch[0];
            bus.rv_r = ch[1];
            L[i] += bus.rv_l; R[i] += bus.rv_r;
            L[i + 1] += bus.rv_l; R[i + 1] += bus.rv_r;
        }
    }
    /* the controls on their laws (tools/gen_tables.py, the bus's controls) */
    int tape = g[G_TAPE], adpc = g[G_ADPC], hiss = g[G_HISS];
    int32_t tq = T_TAPE_Q8[tape];                   /* the depth, in the old control's units, Q8 */
    int32_t drv = T_DRIV[g[G_DRIV]];
    int32_t mvol = T_MVOL[master_vol];
    int32_t ad_mix = adpc * 2 > 127 ? 127 : adpc * 2, ad_div = T_ADPC_DIV[adpc];
    uint32_t wow_inc = 54000, flut_inc = 620000;    /* 0.55 Hz, 6.3 Hz */
    for (int i = 0; i < M2K_SUB; i++) {
        int32_t l = L[i], r = R[i];
        /* glitch: the ring is written at half rate; a repeat replaces the mix */
        if (!(bus.g_phase ^= 1)) {
            gl_buf[bus.gw & (GL_LEN - 1)][0] = (int16_t)sat16((bus.gacc_l + l) >> 1);
            gl_buf[bus.gw & (GL_LEN - 1)][1] = (int16_t)sat16((bus.gacc_r + r) >> 1);
            bus.gw++;
            if (bus.g_on) {
                uint32_t p = bus.g_pos >> 12;
                uint32_t k = bus.g_mode == 1 ? bus.g_start + bus.g_len - 1 - p : bus.g_start + p;
                bus.gout_l = gl_buf[k & (GL_LEN - 1)][0];
                bus.gout_r = gl_buf[k & (GL_LEN - 1)][1];
                bus.g_pos += bus.g_rate;
                if (bus.g_mode == 2 && bus.g_rate > 1024) bus.g_rate -= 1;   /* tape stop */
                if ((bus.g_pos >> 12) >= bus.g_len) {
                    bus.g_pos -= bus.g_len << 12;
                    if (bus.g_mode == 3 && bus.g_len > 128) bus.g_len >>= 1;
                }
                if (bus.g_fade < 32) bus.g_fade++;
                if (bus.g_left-- <= 32) bus.g_fade = (int32_t)bus.g_left;
                if (bus.g_left == 0) bus.g_on = 0;
            }
        } else {
            bus.gacc_l = l;
            bus.gacc_r = r;
        }
        mon.glitch = bus.g_on;
        if (bus.g_on || bus.g_fade > 0) {
            int32_t f = bus.g_on ? bus.g_fade : 0;
            l += ((bus.gout_l - l) * f) >> 5;
            r += ((bus.gout_r - r) * f) >> 5;
        }
        /* tape: wow and flutter on a short line, head loss, a little compression */
        if (tape) {
            tp_buf[bus.tw & 2047][0] = (int16_t)sat16(l);
            tp_buf[bus.tw & 2047][1] = (int16_t)sat16(r);
            bus.wow += wow_inc;
            bus.flut += flut_inc;
            /* the pitch a swing of the line makes grows with its rate: the flutter (6.3 Hz, 11x the wow's) takes 1/48
             * of the wow's swing, so it wobbles about a quarter as far in pitch (at TAPE 127: wow ~+-40 c, flutter ~+-10 c) */
            int32_t m = (sinp(bus.wow) * 48 + sinp(bus.flut)) >> 6;           /* +-24600 */
            int32_t d = (420 << 8) + (int32_t)(((int64_t)m * tq * 3) >> 15);  /* Q8 samples */
            int32_t pos = (int32_t)(bus.tw << 8) - d;
            uint32_t j = (uint32_t)(pos >> 8);
            int32_t f = pos & 255;
            int32_t tl = (tp_buf[j & 2047][0] * (256 - f) + tp_buf[(j + 1) & 2047][0] * f) >> 8;
            int32_t tr = (tp_buf[j & 2047][1] * (256 - f) + tp_buf[(j + 1) & 2047][1] * f) >> 8;
            bus.tw++;
            int32_t li = 127 * 1024 - tq, lj = li >> 10;                     /* the head loss: 127 - old / 4, Q10 */
            int32_t a = T_OP[lj] + (((T_OP[lj < 127 ? lj + 1 : 127] - T_OP[lj]) * (li & 1023)) >> 10);
            bus.tlp_l += ((tl - bus.tlp_l) * a) >> 15;
            bus.tlp_r += ((tr - bus.tlp_r) * a) >> 15;
            l = bus.tlp_l;
            r = bus.tlp_r;
        }
        /* the 4-bit crunch, its rate falling to a third at the top */
        if (adpc) {
            bus.ad_acc += 256;
            if (bus.ad_acc >= ad_div) {
                bus.ad_acc -= (uint16_t)ad_div;
                bus.ad_hold[0] = adpcm_step(0, sat16(l));
                bus.ad_hold[1] = adpcm_step(1, sat16(r));
            }
            l += ((bus.ad_hold[0] - l) * ad_mix) >> 7;
            r += ((bus.ad_hold[1] - r) * ad_mix) >> 7;
        }
        /* hiss and dust: only while the tape rolls, fading in and out with it */
        bus.hgain += ((tape_rolling ? 32767 : 0) - bus.hgain) >> 13;
        if (hiss && bus.hgain > 64) {
            int32_t n = (int32_t)noise_step(&bus.hs) >> 16;
            bus.hlp += ((n - bus.hlp) * 9000) >> 15;
            int32_t h = (bus.hlp * T_HISS_G[hiss]) >> 15;       /* the hiss and the dust on their own laws */
            /* a crackle's chance: the noise's low 18 bits against the old value, three more bits its fraction (so a
             * whole old value picks its clicks, and their levels, as it did) */
            uint32_t cr = ((bus.hs & 0x3FFFF) << 3) | ((bus.hs >> 18) & 7);
            if (cr < T_CRK_P[hiss] && !bus.crackle) bus.crackle = (int32_t)(1500 + (bus.hs >> 21)) * T_CRK_A[hiss] >> 10;
            if (bus.crackle) { h += (bus.hs & 0x100000) ? bus.crackle : -bus.crackle; bus.crackle = (bus.crackle * 5) >> 3; if (bus.crackle < 100) bus.crackle = 0; }
            h = (h * bus.hgain) >> 15;
            l += h;
            r += h;
        }
        l = lim_soft((int32_t)(((int64_t)((l * drv) >> 8) * mvol) >> 15));
        r = lim_soft((int32_t)(((int64_t)((r * drv) >> 8) * mvol) >> 15));
        out[2 * i] = (int16_t)l;
        out[2 * i + 1] = (int16_t)r;
        if ((i & 7) == 0) mon.scope[mon.scope_w++] = (int16_t)((l + r) >> 1);
    }
}

/* ------------------------------------------------------------------------------------------- the render */

static void seq_block(void);
static int seq_running(void);

/* the second core's share of a block: tracks par_from..5, into their own sums (par_L ...) */
int (*m2k_par_begin)(void (*job)(void));
void (*m2k_par_end)(void);
static int par_from = M2K_TRACKS;
static int32_t par_L[M2K_SUB], par_R[M2K_SUB], par_rs[M2K_SUB], par_ds[M2K_SUB];

static void par_job(void)
{
    m_set(par_L, 0, sizeof par_L);
    m_set(par_R, 0, sizeof par_R);
    m_set(par_rs, 0, sizeof par_rs);
    for (int t = par_from; t < M2K_TRACKS; t++) {
        m_set(par_ds, 0, sizeof par_ds);
        track_block(t, par_L, par_R, par_ds, par_rs);
        delay_block(t, par_ds, par_L, par_R);
    }
}

/* where the tracks split: an FX machine stays with the track it hears (the one before it, FX-RING's EXT the one before
 * that), and the sounding tracks are shared out, the first core keeping the bus (worth about two) */
static int par_split(void)
{
    const uint8_t *mc = PAT(cur_pat)->machine;
    int best = M2K_TRACKS, cost = 99, n[M2K_TRACKS + 1] = {0};
    for (int t = 0; t < M2K_TRACKS; t++) n[t + 1] = n[t] + (trk[t].sounding ? 2 : 0) + 1;
    for (int s = 1; s < M2K_TRACKS; s++) {
        if (mc[s] == M_FXC || mc[s] == M_FXR || mc[s] == M_FXD || (s + 1 < M2K_TRACKS && mc[s + 1] == M_FXR)) continue;
        int a = n[s] + 4, b = n[M2K_TRACKS] - n[s], c = a > b ? a : b;
        if (c < cost) { cost = c; best = s; }
    }
    return best;
}

void m2k_render(int16_t *lr, int frames)
{
    static int32_t L[M2K_SUB], R[M2K_SUB], rs[M2K_SUB], ds[M2K_SUB];
    for (int f = 0; f + M2K_SUB <= frames; f += M2K_SUB) {
        PROF_START();
        seq_block();
        tape_rolling = seq_running();
        for (int t = 0; t < M2K_TRACKS; t++) if (trk[t].sounding) tape_rolling = 1;
        for (int t = 0; t < M2K_TRACKS; t++) lfo_block(t);
        mod_apply();
        m_set(L, 0, sizeof L);
        m_set(R, 0, sizeof R);
        m_set(rs, 0, sizeof rs);
        hold_bpm = ext_bpm ? ext_bpm : 30u + PAT(cur_pat)->glob[G_TMPO];
        PROF(0);
        int split = M2K_TRACKS;
        if (m2k_par_begin) {
            par_from = par_split();
            if (par_from < M2K_TRACKS && m2k_par_begin(par_job)) split = par_from;
        }
        for (int t = 0; t < split; t++) {
            m_set(ds, 0, sizeof ds);
            track_block(t, L, R, ds, rs);
            delay_block(t, ds, L, R);
            PROF(3);
        }
        if (split < M2K_TRACKS) {
            m2k_par_end();
            for (int i = 0; i < M2K_SUB; i++) { L[i] += par_L[i]; R[i] += par_R[i]; rs[i] += par_rs[i]; }
        }
        bus_block(L, R, rs, lr + 2 * f);
        PROF(4);
    }
}
