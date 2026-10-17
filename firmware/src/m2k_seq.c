/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's sequencer: six tracks of up to 64 steps, each step a trig with its note, accent and slide, and
 * parameter locks: any track parameter set for one step only, back to the kit on the next trig (as on the
 * Elektron machines). Swing, a pattern queued for the end of the bar, live recording, an arpeggiator per track. */

static struct {
    uint8_t playing, step, queued, rec;
    int32_t ctr;                       /* Q8 samples to the next step */
    uint32_t lockmap[M2K_TRACKS][2];   /* steps with locks, for the screen */
    /* live play */
    uint8_t held[8], nheld;            /* the keys held for the current track, in order */
    uint8_t ktrk;                      /* the track the keys play */
    int8_t rec_skip[M2K_TRACKS];       /* a step just recorded ahead: do not play it twice */
    /* trig conditions and ratchets */
    uint16_t loops;                    /* loops of the pattern since play */
    uint8_t fill, pre[M2K_TRACKS];
    uint8_t rt_left[M2K_TRACKS], rt_note[M2K_TRACKS];
    int32_t rt_ctr[M2K_TRACKS], rt_len[M2K_TRACKS];
    /* arp */
    int32_t arp_ctr;
    uint8_t arp_i, arp_dir;
    uint32_t tick;                     /* control blocks since start */
} sq;

static inline const uint8_t *G(void) { return pat[cur_pat].glob; }
static inline uint32_t seq_bpm(void) { return 30u + G()[G_TMPO]; }
static inline int seq_len(void) { int l = G()[G_LEN]; return l < 1 ? 1 : l > 64 ? 64 : l; }

static void seq_tempo(void)
{
    uint32_t b = seq_bpm();
    bpm10 = b * 10u;
    sps_q8 = 169344000u / b;
}

static int32_t step_len(int s)
{
    int32_t sw = 50 + (G()[G_SWNG] * 25) / 127;             /* percent of the pair for the even step */
    int32_t pair = (int32_t)(sps_q8 * 2u);                 /* <= 11.3 M: the product stays in 32 bits */
    int32_t even = (int32_t)(((uint32_t)pair * (uint32_t)sw) / 100u);
    return (s & 1) ? pair - even : even;
}

/* ------------------------------------------------------------------------------------------------- locks */

static void lockmap_build(void)
{
    m_set(sq.lockmap, 0, sizeof sq.lockmap);
    const m2k_pattern_t *p = &pat[cur_pat];
    for (int i = 0; i < M2K_LOCKS; i++)
        if (p->lock[i].trk < M2K_TRACKS) sq.lockmap[p->lock[i].trk][p->lock[i].step >> 5] |= 1u << (p->lock[i].step & 31);
}

static int lock_find(int t, int s, int prm)
{
    const m2k_pattern_t *p = &pat[cur_pat];
    for (int i = 0; i < M2K_LOCKS; i++)
        if (p->lock[i].trk == t && p->lock[i].step == s && p->lock[i].param == prm) return i;
    return -1;
}

static int lock_set(int t, int s, int prm, int v)
{
    m2k_pattern_t *p = &pat[cur_pat];
    int i = lock_find(t, s, prm);
    if (i < 0)
        for (i = 0; i < M2K_LOCKS && p->lock[i].trk != 0xFF; i++)
            ;
    if (i >= M2K_LOCKS) return 0;
    p->lock[i] = (m2k_lock_t){(uint8_t)t, (uint8_t)s, (uint8_t)prm, (uint8_t)v};
    sq.lockmap[t][s >> 5] |= 1u << (s & 31);
    return 1;
}

static void lock_clear_step(int t, int s)
{
    m2k_pattern_t *p = &pat[cur_pat];
    for (int i = 0; i < M2K_LOCKS; i++)
        if (p->lock[i].trk == t && p->lock[i].step == s) p->lock[i].trk = 0xFF;
    sq.lockmap[t][s >> 5] &= ~(1u << (s & 31));
}

static inline int has_locks(int t, int s) { return (sq.lockmap[t][s >> 5] >> (s & 31)) & 1; }

static void apply_locks(int t, int s)
{
    const m2k_pattern_t *p = &pat[cur_pat];
    m_cpy(trk[t].eff, p->kit[t], P_COUNT);
    if (!has_locks(t, s)) return;
    for (int i = 0; i < M2K_LOCKS; i++)
        if (p->lock[i].trk == t && p->lock[i].step == s) trk[t].eff[p->lock[i].param] = p->lock[i].val;
}

static int32_t gate_blocks(int t)
{
    int32_t gl = pat[cur_pat].kit[t][P_GATE];
    if (gl < 1) gl = 1;
    return (int32_t)(((sps_q8 >> 8) * (uint32_t)gl) / (16u * M2K_SUB)) + 1;
}

/* --------------------------------------------------------------------------------------------- the clock */

/* does a trig with this condition play? PRE remembers the last condition tested on the track */
static int cond_pass(int t, int c)
{
    static const uint8_t CHANCE[6] = {255, 192, 128, 85, 64, 32};
    static const uint8_t AB[8][2] = {{1, 2}, {2, 2}, {1, 3}, {2, 3}, {3, 3}, {1, 4}, {2, 4}, {3, 4}};
    int r;
    if (c == C_ALWAYS) return 1;
    if (c <= C_P12) r = (int)(rnd32() & 255) < CHANCE[c];
    else if (c <= C_4_4) { int i = c - C_1_2; int a = c == C_4_4 ? 4 : AB[i][0], b = c == C_4_4 ? 4 : AB[i][1]; r = sq.loops % b == (unsigned)(a - 1); }
    else if (c == C_1ST) r = sq.loops == 0;
    else if (c == C_N1ST) r = sq.loops != 0;
    else if (c == C_PRE) return sq.pre[t];
    else if (c == C_NPRE) return !sq.pre[t];
    else if (c == C_FILL) r = sq.fill;
    else r = !sq.fill;
    sq.pre[t] = (uint8_t)r;
    return r;
}

static void fire_step(int s)
{
    m2k_pattern_t *p = &pat[cur_pat];
    for (int t = 0; t < M2K_TRACKS; t++) {
        m2k_step_t st = p->step[t][s];
        if (!(st.flags & ST_TRIG) || (p->mute >> t & 1)) continue;
        if (sq.rec_skip[t] == s) { sq.rec_skip[t] = -1; continue; }
        if (sq.nheld && t == sq.ktrk) continue;     /* the player has the track */
        if (!cond_pass(t, st.cond)) continue;
        apply_locks(t, s);
        voice_on(t, st.note, (st.flags & ST_ACC) ? 127 : 100, st.flags & ST_SLD);
        trk[t].gate_left = gate_blocks(t);
        int n = st.rtrg < 1 ? 1 : st.rtrg > 4 ? 4 : st.rtrg;
        sq.rt_left[t] = (uint8_t)(n - 1);
        if (n > 1) {
            sq.rt_len[t] = step_len(s) / n;
            sq.rt_ctr[t] = sq.rt_len[t];
            sq.rt_note[t] = st.note;
            trk[t].gate_left = sq.rt_len[t] / (M2K_SUB << 8) * 3 / 4 + 1;
        }
    }
    bus_step();
}

static void seq_block(void)
{
    sq.tick++;
    seq_tempo();
    if (sq.playing) {
        sq.ctr -= M2K_SUB << 8;
        while (sq.ctr <= 0) {
            fire_step(sq.step);
            sq.ctr += step_len(sq.step);
            if (++sq.step >= seq_len()) {
                sq.step = 0;
                sq.loops++;
                if (sq.queued != cur_pat) {
                    cur_pat = sq.queued;
                    lockmap_build();
                    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[cur_pat].kit[t], P_COUNT);
                }
            }
        }
    }
    /* ratchets: the rest of a step's hits */
    for (int t = 0; t < M2K_TRACKS; t++) {
        if (!sq.rt_left[t]) continue;
        sq.rt_ctr[t] -= M2K_SUB << 8;
        if (sq.rt_ctr[t] <= 0) {
            sq.rt_ctr[t] += sq.rt_len[t];
            sq.rt_left[t]--;
            voice_on(t, sq.rt_note[t], 100, 0);
            trk[t].gate_left = sq.rt_len[t] / (M2K_SUB << 8) * 3 / 4 + 1;
        }
    }
    /* the arp, on its own clock */
    const uint8_t *k = pat[cur_pat].kit[sq.ktrk];
    int mode = (k[P_AMOD] * 6) >> 7;
    if (mode && sq.nheld) {
        static const uint8_t div[4] = {2, 4, 8, 16};     /* in 1/8 of a sixteenth: 1/32 .. 1/4 */
        int32_t len = (int32_t)((sps_q8 * div[(k[P_ASPD] * 4) >> 7]) >> 3);
        sq.arp_ctr -= M2K_SUB << 8;
        if (sq.arp_ctr <= 0) {
            sq.arp_ctr += len;
            int rng = 1 + ((k[P_ARNG] * 4) >> 7);
            int n = sq.nheld * rng;
            uint8_t sorted[8];
            m_cpy(sorted, sq.held, sq.nheld);
            if (mode != 5)
                for (int i = 1; i < sq.nheld; i++)
                    for (int j = i; j > 0 && sorted[j - 1] > sorted[j]; j--) { uint8_t x = sorted[j]; sorted[j] = sorted[j - 1]; sorted[j - 1] = x; }
            int i;
            switch (mode) {
            case 1: i = sq.arp_i % n; sq.arp_i++; break;
            case 2: i = n - 1 - (sq.arp_i % n); sq.arp_i++; break;
            case 3: {
                int per = n > 1 ? 2 * n - 2 : 1, q = sq.arp_i % per;
                i = q < n ? q : per - q;
                sq.arp_i++;
                break;
            }
            case 4: i = (int)(rnd32() % (uint32_t)n); break;
            default: i = sq.arp_i % n; sq.arp_i++; break;
            }
            int note = sorted[i % sq.nheld] + 12 * (i / sq.nheld);
            m_cpy(trk[sq.ktrk].eff, pat[cur_pat].kit[sq.ktrk], P_COUNT);
            voice_on(sq.ktrk, note, 100, 0);
            trk[sq.ktrk].gate_left = gate_blocks(sq.ktrk) / 2 + 1;
        }
    }
}

static int seq_running(void) { return sq.playing; }

static void seq_play(int on)
{
    if (on && !sq.playing) {
        sq.step = 0;
        sq.ctr = 0;
        sq.loops = 0;
        lockmap_build();
    }
    if (!on)
        for (int t = 0; t < M2K_TRACKS; t++) voice_off(t);
    sq.playing = (uint8_t)on;
}

/* ------------------------------------------------------------------------------------------- live play */

static void live_on(int note, int vel)
{
    int t = sq.ktrk;
    for (int i = 0; i < sq.nheld; i++) if (sq.held[i] == note) return;
    if (sq.nheld < 8) sq.held[sq.nheld++] = (uint8_t)note;
    int mode = (pat[cur_pat].kit[t][P_AMOD] * 6) >> 7;
    if (mode) {
        if (sq.nheld == 1) { sq.arp_ctr = 0; sq.arp_i = 0; }
        return;
    }
    m_cpy(trk[t].eff, pat[cur_pat].kit[t], P_COUNT);
    voice_on(t, note, vel, sq.nheld > 1 && pat[cur_pat].kit[t][P_PORT] > 0);
    trk[t].gate_left = -1;
    if (sq.rec && sq.playing) {        /* to the nearest step; ahead of us, it must not play again this time */
        int32_t len = step_len(sq.step ? sq.step - 1 : seq_len() - 1);
        int s = sq.step ? sq.step - 1 : seq_len() - 1;
        if (sq.ctr < len / 2) { s = sq.step; sq.rec_skip[t] = (int8_t)s; }
        m2k_step_t *st = &pat[cur_pat].step[t][s];
        st->note = (uint8_t)note;
        st->flags = (uint8_t)(ST_TRIG | (vel > 110 ? ST_ACC : 0));
    }
}

static void live_off(int note)
{
    int t = sq.ktrk, j = 0;
    for (int i = 0; i < sq.nheld; i++) if (sq.held[i] != note) sq.held[j++] = sq.held[i];
    sq.nheld = (uint8_t)j;
    int mode = (pat[cur_pat].kit[t][P_AMOD] * 6) >> 7;
    if (mode) return;
    if (j && trk[t].note == note) {
        voice_on(t, sq.held[j - 1], 100, 1);         /* back to the last key still held */
        return;
    }
    if (!j || trk[t].note == note) voice_off(t);
}

/* the Monomachine's CC map (its MIDI implementation, as the gearmulator emulator encodes it): a channel per track,
 * SYN on 48-55, AMP 56-63, FILT 72-79, FX 80-87, LFO 1/2/3 on 88/104/112, level 7, mute 3. MONO2K's pages are laid
 * out the same, so a controller set up for a Monomachine drives the same parameters here. */
static int mm_cc_param(int cc)
{
    static const uint8_t base[7] = {48, 56, 72, 80, 88, 104, 112};
    static const uint8_t first[7] = {P_SYN, P_ATK, P_BASE, P_EQF, P_LFO, P_LFO + 8, P_LFO + 16};
    for (int i = 0; i < 7; i++)
        if (cc >= base[i] && cc < base[i] + 8) return first[i] + (cc - base[i]);
    if (cc == 7) return P_LEVL;
    return -1;
}

void m2k_midi(int status, int d1, int d2)
{
    int ch = status & 15, cmd = status & 0xF0;
    int t = ch < M2K_TRACKS ? ch : sq.ktrk;
    if (cmd == 0xB0) {
        if (d1 == 3) {
            pat[cur_pat].mute = (uint8_t)((pat[cur_pat].mute & ~(1u << t)) | (d2 ? 1u << t : 0));
            return;
        }
        int prm = mm_cc_param(d1);
        if (prm < 0) return;
        int v = d2 > 127 ? 127 : d2;
        if (prm == P_LFO + L_DEST || prm == P_LFO + 8 + L_DEST || prm == P_LFO + 16 + L_DEST) v = v > P_COUNT ? P_COUNT : v;
        pat[cur_pat].kit[t][prm] = (uint8_t)v;
        trk[t].eff[prm] = (uint8_t)v;
        return;
    }
    if (cmd == 0x90 && d2) {
        if (t == sq.ktrk) { live_on(d1, d2); return; }
        m_cpy(trk[t].eff, pat[cur_pat].kit[t], P_COUNT);
        voice_on(t, d1, d2, 0);
        trk[t].gate_left = -1;
    } else if (cmd == 0x80 || cmd == 0x90) {
        if (t == sq.ktrk) { live_off(d1); return; }
        if (trk[t].note == d1) voice_off(t);
    } else if (status == 0xFA) {
        seq_play(1);
    } else if (status == 0xFC) {
        seq_play(0);
    }
}

/* ------------------------------------------------------------------------------------------ the factory */

static inline uint8_t enc(int i, int n) { return (uint8_t)((i * 128 + 64) / n); }

static const uint8_t MACH_DEF[M_COUNT][8] = {
    /* GND-SIN  */ {64, 0, 0, 0, 40, 40, 30, 0},
    /* GND-NOIS */ {64, 0, 0, 40, 0, 40, 0, 0},
    /* SID-8BIT */ {40, 64, 30, 24, 64, 64, 0, 0},
    /* FM+STAT  */ {14, 60, 60, 26, 30, 50, 0, 0},
    /* FM+DYN   */ {42, 80, 45, 38, 40, 35, 20, 96},
    /* SWAV-SAW */ {0, 0, 0, 0, 0, 0, 0, 64},     /* the Monomachine's init: the base saw alone */
    /* SWAV-ENS */ {21, 36, 61, 30, 0, 64, 90, 0},
    /* DIGI-WAV */ {40, 64, 64, 0, 0, 70, 0, 0},
    /* VOX-6    */ {10, 80, 60, 64, 10, 70, 20, 20},
};

static void kit_default(m2k_pattern_t *p, int t, int m)
{
    uint8_t *k = p->kit[t];
    static const uint8_t D[P_COUNT] = {
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 64, 64, 0, 100, 64, 0,           /* ATK HOLD DEC REL (the Monomachine's init) DIST VOL PAN PORT */
        0, 127, 0, 0, 0, 64, 64, 64,           /* BASE WDTH HPQ LPQ FATK FDEC BOF WOF */
        64, 64, 0, 12, 0, 50, 0, 110,          /* EQF EQG SRR DTIM DSND DFB DBAS DWID */
        0, 0, 0, 0,                            /* BITS FOLD XOR SPU */
        64, 110, 0, 64, 0, 14,                 /* PTCH LEVL AMOD ASPD ARNG GATE */
    };
    m_cpy(k, D, P_COUNT);
    k[P_MODR] = 40;                    /* the modulation: a slow chorus, off until its MIX is up */
    k[P_MODD] = 64;
    for (int l = 0; l < 3; l++) {
        uint8_t *b = &k[P_LFO + 8 * l];
        b[L_TRCK] = enc(t, M2K_TRACKS);
        b[L_DEST] = 0;
        b[L_WAVE] = 0;
        b[L_TRIG] = 0;
        b[L_MULT] = 0;
        b[L_SPD] = 32;
        b[L_PHAS] = 0;
        b[L_DPTH] = 64;
    }
    p->machine[t] = (uint8_t)m;
    m_cpy(k, MACH_DEF[m], 8);
}

static void pat_init(m2k_pattern_t *p, const char *name)
{
    m_set(p, 0, sizeof *p);
    for (int i = 0; i < 7 && name[i]; i++) p->name[i] = name[i];
    static const uint8_t M[M2K_TRACKS] = {M_GSIN, M_FMD, M_GNOI, M_SID, M_VOX, M_ENS};
    for (int t = 0; t < M2K_TRACKS; t++) {
        kit_default(p, t, M[t]);
        for (int s = 0; s < M2K_STEPS; s++) p->step[t][s].note = 60;
    }
    for (int i = 0; i < M2K_LOCKS; i++) p->lock[i].trk = 0xFF;
    p->glob[G_TMPO] = 90;
    p->glob[G_LEN] = 16;
    p->glob[G_ACNT] = 60;
    p->glob[G_VERB] = 60;
    p->glob[G_DAMP] = 50;
}

#include "m2k_factory.c"
#include "m2k_proj.c"

/* ------------------------------------------------------------------------------------------------ store */

static uint8_t dirty;

void m2k_setting(int i, int v)
{
    if (i >= 0 && i < S_COUNT) store_.set[i] = (uint8_t)CLAMP(v, 0, 127);
}

int m2k_dirty(void)
{
    int d = dirty;
    dirty = 0;
    return d;
}

static void ui_init(void);

void m2k_init(uint32_t seed)
{
    rng = seed | 1u;
    m_set(trk, 0, sizeof trk);
    m_set(&bus, 0, sizeof bus);
    m_set(&sq, 0, sizeof sq);
    m_set(dl_mem, 0x7F, sizeof dl_mem);          /* mu-law silence: 0xFF and 0x7F both decode to ~0 */
    m_set(gl_buf, 0, sizeof gl_buf);
    m_set(tp_buf, 0, sizeof tp_buf);
    m_set(cb_buf, 0, sizeof cb_buf);
    m_set(ap_buf, 0, sizeof ap_buf);
    for (int t = 0; t < M2K_TRACKS; t++) {
        trk[t].dl = dl_mem[t];
        trk[t].lfsr = 0x1234567u + (uint32_t)t * 7919u;
        trk[t].gate_left = -1;
        trk[t].dt_q8 = 400 << 8;
    }
    bus.hs = 0xACE1u;
    for (int i = 0; i < M2K_TRACKS; i++) sq.rec_skip[i] = -1;
    m_set(store_.set, 0, sizeof store_.set);
    projects_factory();
    cur_pat = 0;
    sq.queued = 0;
    lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[0].kit[t], P_COUNT);
    seq_tempo();
    ui_init();
}
