/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's screen and controls. One grid for every page, cut at the golden section of 240 (148 / 92):
 *   0..15    header: the page tag, the track and its machine badge, the pattern and the transport
 *   18..121  the focus: the last knob's value in big type (left, 148), the page's live picture (right, 92)
 *   124..146 the current track's 16 steps, the four pages under them
 *   148..239 the four knobs: label, value, a slab
 * Controls: page buttons (EDIT SYN, ENV AMP, SEL FILT, FX FX/DEL/DSTR, LFO 1-3, ARP, GLO, HOME), SEQ toggles the
 * keys between playing and the 16 steps (hold a step and turn a knob: a lock), SELECT the track, ALGORITHM the
 * machine, PRESETS the pattern, KNOB 1-4 the page's values, MASTER the volume. */

/* ------------------------------------------------------------------------------------------- parameters */

enum {
    K_NUM, K_BI, K_TIME, K_HOLD, K_DECT, K_HZ, K_PAN, K_DTIM, K_BITS, K_SEMI, K_RATIO, K_SEMI24, K_INT25, K_SIDW, K_SIDM,
    K_ALGO, K_VOIC, K_WAVE, K_VOWL, K_TRK, K_DEST, K_LWAV, K_LTRG, K_MULT, K_AMOD, K_ASPD, K_ARNG, K_GATE,
    K_BPM, K_SWNG, K_LEN, K_PCT, K_GSIZ, K_FINE
};
typedef struct { const char *s4, *name; uint8_t kind; } pi_t;

static const pi_t PI_COMMON[P_LFO - 8] = {
    {"ATCK", "ATTACK", K_TIME}, {"HOLD", "HOLD", K_HOLD}, {"DEC", "DECAY", K_DECT}, {"REL", "RELEASE", K_TIME},
    {"DIST", "DISTORTION", K_NUM}, {"VOL", "VOLUME", K_NUM}, {"PAN", "PAN", K_PAN}, {"PORT", "PORTAMENTO", K_NUM},
    {"BASE", "FILTER BASE", K_HZ}, {"WDTH", "FILTER WIDTH", K_NUM}, {"HPQ", "HP RESONANCE", K_NUM}, {"LPQ", "LP RESONANCE", K_NUM},
    {"ATCK", "FILTER ATTACK", K_TIME}, {"DEC", "FILTER DECAY", K_DECT}, {"B-OF", "BASE BY ENV", K_BI}, {"W-OF", "WIDTH BY ENV", K_BI},
    {"EQF", "EQ FREQUENCY", K_HZ}, {"EQG", "EQ GAIN", K_BI}, {"SRR", "RATE REDUCTION", K_NUM}, {"DTIM", "DELAY TIME", K_DTIM},
    {"DSND", "DELAY SEND", K_NUM}, {"DFB", "DELAY FEEDBACK", K_NUM}, {"DBAS", "DELAY BASE", K_HZ}, {"DWID", "DELAY WIDTH", K_NUM},
    {"BITS", "BIT DEPTH", K_BITS}, {"FOLD", "WAVEFOLD", K_NUM}, {"XOR", "XOR MANGLE", K_NUM}, {"SPU", "SPU SEND", K_NUM},
    {"PTCH", "TRACK PITCH", K_SEMI}, {"LEVL", "TRACK LEVEL", K_NUM}, {"MODE", "ARP MODE", K_AMOD}, {"SPD", "ARP SPEED", K_ASPD},
    {"RNG", "ARP RANGE", K_ARNG}, {"GATE", "GATE LENGTH", K_GATE},
};
static const pi_t PI_LFO[8] = {
    {"TRCK", "LFO TRACK", K_TRK}, {"DEST", "LFO DESTINATION", K_DEST}, {"WAVE", "LFO WAVE", K_LWAV}, {"TRIG", "LFO TRIG MODE", K_LTRG},
    {"MULT", "LFO MULTIPLIER", K_MULT}, {"SPD", "LFO SPEED", K_NUM}, {"PHAS", "START PHASE", K_NUM}, {"DPTH", "LFO DEPTH", K_BI},
};
static const pi_t PI_GLOB[G_COUNT] = {
    {"BPM", "TEMPO", K_BPM}, {"SWNG", "SWING", K_SWNG}, {"LEN", "PATTERN LENGTH", K_LEN}, {"ACNT", "ACCENT", K_NUM},
    {"TAPE", "TAPE WOW", K_NUM}, {"ADPC", "ADPCM CRUNCH", K_NUM}, {"GLCH", "GLITCH CHANCE", K_PCT}, {"SIZE", "REPEAT SIZE", K_GSIZ},
    {"VERB", "SPU DECAY", K_NUM}, {"DAMP", "SPU DAMPING", K_NUM}, {"HISS", "HISS AND DUST", K_NUM}, {"DRIV", "BUS DRIVE", K_NUM},
};

typedef struct {
    const char *name, *badge, *style, *cls;
    uint8_t stat[3];                   /* punch, metal, plastic: 0..5 */
    pi_t syn[8];
} mi_t;

static const mi_t MI[M_COUNT] = {
    {"GND-SIN", "GND", "SINE", "PERC", {5, 1, 3},
     {{"TUNE", "FINE TUNE", K_FINE}, {"FOLD", "SINE FOLD", K_NUM}, {"SUB", "SUB LEVEL", K_NUM}, {"NOIS", "NOISE", K_NUM},
      {"PDEP", "PITCH DROP", K_NUM}, {"PDEC", "DROP TIME", K_TIME}, {"CLIK", "CLICK", K_NUM}, {"HARM", "2ND HARMONIC", K_NUM}}},
    {"GND-NOIS", "GND", "NOISE", "PERC", {3, 4, 1},
     {{"COLR", "COLOUR", K_BI}, {"S&H", "PITCHED HOLD", K_NUM}, {"META", "METAL MIX", K_NUM}, {"MSPR", "METAL SPREAD", K_NUM},
      {"PDEP", "PITCH DROP", K_NUM}, {"PDEC", "DROP TIME", K_TIME}, {"GRIT", "8-BIT GRIT", K_NUM}, {"-", "-", K_NUM}}},
    {"SID-8BIT", "SID", "CHIP", "SYNTH", {3, 2, 4},
     {{"WAV1", "OSC 1 WAVE", K_SIDW}, {"PW", "PULSE WIDTH", K_NUM}, {"PWM", "PWM DEPTH", K_NUM}, {"WAV2", "OSC 2 WAVE", K_SIDW},
      {"DTN2", "OSC 2 TUNE", K_SEMI24}, {"FINE", "OSC 2 FINE", K_FINE}, {"MODE", "SYNC / RING", K_SIDM}, {"MIX", "OSC MIX", K_NUM}}},
    {"FM+STAT", "FM+", "STATIC", "SYNTH", {4, 5, 3},
     {{"RAT1", "RATIO 1", K_RATIO}, {"DEP1", "DEPTH 1", K_NUM}, {"DEC1", "DECAY 1", K_DECT}, {"RAT2", "RATIO 2", K_RATIO},
      {"DEP2", "DEPTH 2", K_NUM}, {"DEC2", "DECAY 2", K_DECT}, {"FDBK", "FEEDBACK", K_NUM}, {"ALGO", "ALGORITHM", K_ALGO}}},
    {"FM+DYN", "FM+", "DYNAMIC", "SYNTH", {4, 4, 5},
     {{"RAT1", "RATIO 1", K_RATIO}, {"DEP1", "DEPTH 1", K_NUM}, {"DEC1", "DECAY 1", K_DECT}, {"RAT2", "RATIO 2", K_RATIO},
      {"DEP2", "DEPTH 2", K_NUM}, {"DEC2", "DECAY 2", K_DECT}, {"FDBK", "FEEDBACK", K_NUM}, {"SWEP", "RATIO SWEEP", K_BI}}},
    {"SWAV-SAW", "SWV", "SUPER", "SYNTH", {2, 1, 2},
     {{"DTUN", "DETUNE", K_NUM}, {"VOIC", "VOICES", K_VOIC}, {"MIX", "SIDE MIX", K_NUM}, {"SUB", "SUB OSC", K_NUM},
      {"SHPE", "SAW TO SQUARE", K_NUM}, {"DRFT", "DRIFT", K_NUM}, {"OCTV", "OCTAVE UP", K_NUM}, {"STRT", "PHASE START", K_NUM}}},
    {"SWAV-ENS", "SWV", "CHORD", "SYNTH", {2, 1, 3},
     {{"INT1", "INTERVAL 1", K_INT25}, {"INT2", "INTERVAL 2", K_INT25}, {"INT3", "INTERVAL 3", K_INT25}, {"DTUN", "DETUNE", K_NUM},
      {"SHPE", "SAW TO PULSE", K_NUM}, {"PW", "PULSE WIDTH", K_NUM}, {"LEV", "CHORD LEVEL", K_NUM}, {"SUB", "SUB OSC", K_NUM}}},
    {"DIGI-WAV", "DGW", "DIGITAL", "SYNTH", {3, 3, 4},
     {{"WAVE", "WAVE", K_WAVE}, {"SCAN", "SCAN BY ENV", K_BI}, {"STIM", "SCAN TIME", K_DECT}, {"SYNC", "HARD SYNC", K_NUM},
      {"BITS", "WAVE BITS", K_NUM}, {"DTUN", "OSC 2 DETUNE", K_BI}, {"MIX2", "OSC 2 LEVEL", K_NUM}, {"STEP", "PHASE STEPS", K_NUM}}},
    {"VOX-6", "VOX", "VOICE", "VOICE", {1, 2, 5},
     {{"VOWL", "VOWEL", K_VOWL}, {"VSWP", "VOWEL SWEEP", K_BI}, {"VTIM", "SWEEP TIME", K_TIME}, {"SHFT", "FORMANT SHIFT", K_BI},
      {"BRTH", "BREATH", K_NUM}, {"RESO", "RESONANCE", K_NUM}, {"BUZZ", "BUZZ", K_NUM}, {"VIBR", "VIBRATO", K_NUM}}},
};

static const char *const SIDW_N[8] = {"TRI", "SAW", "PUL", "NOI", "T+S", "T+P", "S+P", "ALL"};
static const char *const SIDM_N[4] = {"OFF", "SYNC", "RING", "S+R"};
static const char *const ALGO_N[4] = {"STCK", "PAR", "2CAR", "RING"};
static const char *const LWAV_N[7] = {"TRI", "SIN", "SQR", "SAW", "EXP", "RMP", "RND"};
static const char *const LTRG_N[5] = {"FREE", "TRIG", "HOLD", "ONE", "HALF"};
static const char *const AMOD_N[6] = {"OFF", "UP", "DOWN", "U/D", "RND", "ORDR"};
static const char *const ASPD_N[4] = {"1/32", "1/16", "1/8", "1/4"};
static const char *const GSIZ_N[5] = {"1/64", "1/32", "1/16", "1/8", "1/4"};
static const char *const COND_N[C_COUNT] = {"100%", "75%", "50%", "33%", "25%", "12%", "1:2", "2:2", "1:3", "2:3", "3:3",
                                           "1:4", "2:4", "3:4", "4:4", "1ST", "!1ST", "PRE", "!PRE", "FILL", "!FIL"};
static const uint8_t COND_DENS[6] = {64, 48, 32, 21, 16, 8};
static const char *const NOTE_N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

static const pi_t *pinfo(int t, int id)
{
    if (id >= 0x80) return &PI_GLOB[id & 0x7F];
    if (id < 8) return &MI[pat[cur_pat].machine[t]].syn[id];
    if (id < P_LFO) return &PI_COMMON[id - 8];
    return &PI_LFO[(id - P_LFO) & 7];
}

static int kind_choices(int k)
{
    switch (k) {
    case K_RATIO: case K_WAVE: return 32;
    case K_SIDW: return 8;
    case K_SIDM: case K_ALGO: case K_ASPD: case K_ARNG: return 4;
    case K_VOIC: case K_LWAV: return 7;
    case K_TRK: case K_AMOD: return 6;
    case K_LTRG: case K_GSIZ: return 5;
    case K_MULT: return 8;
    case K_INT25: return 25;
    default: return 0;
    }
}

static int kind_max(int k) { return k == K_BPM ? 255 : k == K_LEN ? 64 : k == K_DEST ? P_COUNT : 127; }
static int kind_min(int k) { return k == K_LEN ? 1 : 0; }

/* ------------------------------------------------------------------------------------------------ strings */

static char *s_cpy(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char *s_int(char *d, int v)
{
    char t[12];
    int n = 0;
    if (v < 0) { *d++ = '-'; v = -v; }
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *d++ = t[--n];
    *d = 0;
    return d;
}
static char *s_dec1(char *d, int v10) { d = s_int(d, v10 / 10); *d++ = '.'; *d++ = (char)('0' + v10 % 10); *d = 0; return d; }
static char *s_note(char *d, int n) { d = s_cpy(d, NOTE_N[n % 12]); return s_int(d, n / 12 - 1); }

/* the value as the screen shows it, and its unit */
static void fmt(const pi_t *pi, int t, int v, char *out, const char **unit)
{
    *unit = "";
    int n = kind_choices(pi->kind), c = n ? (v * n) >> 7 : 0;
    switch (pi->kind) {
    case K_BI: s_int(out, v - 64); break;
    case K_FINE: s_int(out, ((v - 64) * 100) / 64); *unit = "CENT"; break;
    case K_TIME: case K_HOLD: case K_DECT: {
        if ((pi->kind == K_HOLD || pi->kind == K_DECT) && v == 127) { s_cpy(out, "INF"); break; }
        uint32_t ms10 = T_ENV_MS10[v];
        if (ms10 < 100) { s_dec1(out, (int)ms10); *unit = "MS"; }
        else if (ms10 < 10000) { s_int(out, (int)(ms10 / 10)); *unit = "MS"; }
        else { s_dec1(out, (int)(ms10 / 1000)); *unit = "S"; }
        break;
    }
    case K_HZ: {
        int hz = T_HZ[v];
        if (hz < 1000) { s_int(out, hz); *unit = "HZ"; }
        else { s_dec1(out, hz / 100); *unit = "KHZ"; }
        break;
    }
    case K_PAN:
        if (v == 64) s_cpy(out, "C");
        else { char *e = s_cpy(out, v < 64 ? "L" : "R"); s_int(e, v < 64 ? 64 - v : v - 64); }
        break;
    case K_DTIM: {
        if (v == 0) { s_cpy(out, "COMB"); break; }
        /* in 1/64 notes: simplify */
        int num = v, den = 64;
        while (!(num & 1) && den > 1) { num >>= 1; den >>= 1; }
        char *e = s_int(out, num);
        if (den > 1) { *e++ = '/'; s_int(e, den); }
        *unit = "NOTE";
        break;
    }
    case K_BITS: s_int(out, 16 - (v * 14 >> 7)); *unit = "BIT"; break;
    case K_SEMI: s_int(out, v - 64); *unit = "SEMI"; break;
    case K_RATIO: s_cpy(out, T_RATIO_NAME[c]); *unit = "RATIO"; break;
    case K_SEMI24: s_int(out, ((v - 64) * 24) / 64); *unit = "SEMI"; break;
    case K_INT25: { char *e = s_cpy(out, "+"); s_int(e, c); *unit = "SEMI"; break; }
    case K_SIDW: s_cpy(out, SIDW_N[c]); break;
    case K_SIDM: s_cpy(out, SIDM_N[c]); break;
    case K_ALGO: s_cpy(out, ALGO_N[c]); break;
    case K_VOIC: s_int(out, c + 1); *unit = "VOICES"; break;
    case K_WAVE: s_cpy(out, T_WAVE_NAME[c]); break;
    case K_VOWL: s_cpy(out, T_VOWEL_NAME[(v * 7 + 63) / 127]); *unit = "VOWEL"; break;
    case K_TRK: { char *e = s_cpy(out, "T"); s_int(e, c + 1); break; }
    case K_DEST:
        if (v == 0) s_cpy(out, "OFF");
        else s_cpy(out, pinfo(t, v - 1)->s4);
        break;
    case K_LWAV: s_cpy(out, LWAV_N[c]); break;
    case K_LTRG: s_cpy(out, LTRG_N[c]); break;
    case K_MULT: { char *e = s_cpy(out, "X"); s_int(e, 1 << c); break; }
    case K_AMOD: s_cpy(out, AMOD_N[c]); break;
    case K_ASPD: s_cpy(out, ASPD_N[c]); *unit = "NOTE"; break;
    case K_ARNG: s_int(out, c + 1); *unit = "OCT"; break;
    case K_GATE: { int v10 = (v * 10) / 16; s_dec1(out, v10); *unit = "STEPS"; break; }
    case K_BPM: s_int(out, 30 + v); *unit = "BPM"; break;
    case K_SWNG: s_int(out, 50 + (v * 25) / 127); *unit = "%"; break;
    case K_LEN: s_int(out, v); *unit = "STEPS"; break;
    case K_PCT: s_int(out, (v * 100) / 256); *unit = "%"; break;
    case K_GSIZ: s_cpy(out, GSIZ_N[(v * 5) >> 7]); *unit = "NOTE"; break;
    default: s_int(out, v); break;
    }
}

/* -------------------------------------------------------------------------------------------- the pages */

enum { PG_HOME, PG_SYN, PG_AMP, PG_FILT, PG_FX, PG_LFO, PG_ARP, PG_GLO, PG_COUNT };
static const uint8_t NSUB[PG_COUNT] = {1, 2, 2, 2, 3, 6, 1, 3};
#define GL(g) (0x80 | (g))
#define NONE 0xFF

static int knob_param(int page, int sub, int k)
{
    switch (page) {
    case PG_HOME: { static const uint8_t m[4] = {P_LEVL, P_PTCH, GL(G_TMPO), GL(G_SWNG)}; return m[k]; }
    case PG_SYN: return P_SYN + sub * 4 + k;
    case PG_AMP: return P_ATK + sub * 4 + k;
    case PG_FILT: return P_BASE + sub * 4 + k;
    case PG_FX: return P_EQF + sub * 4 + k;
    case PG_LFO: return P_LFO + (sub >> 1) * 8 + (sub & 1) * 4 + k;
    case PG_ARP: return P_AMOD + k;
    case PG_GLO: return GL(sub * 4 + k);
    default: return NONE;
    }
}

static void page_label(int page, int sub, char *o)
{
    static const char *const N[PG_COUNT] = {"HOME", "SYN", "AMP", "FILT", "FX", "LFO", "ARP", "GLO"};
    switch (page) {
    case PG_FX: s_cpy(o, sub == 0 ? "FX" : sub == 1 ? "DELAY" : "DSTR"); break;
    case PG_LFO: { char *e = s_cpy(o, "LFO"); e = s_int(e, (sub >> 1) + 1); s_cpy(e, (sub & 1) ? "B" : "A"); break; }
    case PG_GLO: s_cpy(o, sub == 0 ? "TIME" : sub == 1 ? "BUS" : "SPU"); break;
    case PG_SYN: case PG_AMP: case PG_FILT: { char *e = s_cpy(o, N[page]); *e++ = ' '; e = s_int(e, sub + 1); s_cpy(e, "/2"); break; }
    default: s_cpy(o, N[page]); break;
    }
}

/* --------------------------------------------------------------------------------------------- UI state */

static struct {
    uint32_t now, boot_t0;
    uint8_t page, sub, trk, grid, spage, follow;
    int8_t oct;
    uint8_t note[M2K_TRACKS];          /* the last note played on each track: new trigs take it */
    /* steps held in GRID */
    uint8_t held[8], nheld, held_added[8], held_edit;
    uint8_t btn[B_COUNT], glo_used, keys[M2K_KEYS];
    uint32_t rec_t0;
    /* the focus */
    uint8_t focus;
    uint32_t focus_t;
    char roll_old[12];
    uint32_t roll_t;
    int32_t ease[4];                   /* Q8 */
    /* animations */
    uint32_t trans_t, glitch_t, card_t, save_t, toast_t;
    uint8_t trans_len, glitch_amt, in_trans, trans_down;
    uint32_t glitch_seed;
    char toast[24], toast2[24];
    uint8_t seen_trig[M2K_TRACKS];
    uint32_t flash_t[M2K_TRACKS];
    int32_t icon_ph, icon_spd;         /* Q8 frames, and frames per second Q8 */
    int32_t meter[M2K_TRACKS];
    int16_t lfo_hist[88];
    uint8_t lfo_w;
    uint8_t leds[M2K_KEYS + B_COUNT];
    int8_t kw[M2K_KEYS], kb[M2K_KEYS];
} ui;

static void ui_init(void)
{
    m_set(&ui, 0, sizeof ui);
    for (int k = 0, w = 0, b = 0; k < M2K_KEYS; k++) {
        int black = (0x54a >> ((k + 5) % 12)) & 1;
        ui.kw[k] = (int8_t)(black ? -1 : w++);
        ui.kb[k] = (int8_t)(black ? b++ : -1);
    }
    for (int t = 0; t < M2K_TRACKS; t++) ui.note[t] = 60;
    ui.note[0] = 36;
    ui.follow = 1;
    ui.page = PG_HOME;
    ui.icon_spd = 6 << 8;
    ui.boot_t0 = 0xFFFFFFFFu;
}

static void start_glitch(int amt);

static void start_trans_dir(int len, int down)
{
    ui.trans_down = (uint8_t)down;
    start_glitch(2);
    m_cpy(fb_prev, fb_out, sizeof fb_prev);
    ui.trans_t = ui.now;
    ui.trans_len = (uint8_t)len;
    ui.in_trans = 1;
}

static void start_trans(int len) { start_trans_dir(len, 0); }
static void start_glitch(int amt)
{
    if (ui.glitch_t && ui.now - ui.glitch_t < 120 && amt <= ui.glitch_amt) return;
    ui.glitch_t = ui.now | 1u;
    ui.glitch_amt = (uint8_t)amt;
    ui.glitch_seed = rnd32();
}

static void toast(const char *a, const char *b)
{
    s_cpy(ui.toast, a);
    s_cpy(ui.toast2, b ? b : "");
    ui.toast_t = ui.now | 1u;
}

static void go(int page)
{
    if (page == ui.page) ui.sub = (uint8_t)((ui.sub + 1) % NSUB[page]);
    else { ui.page = (uint8_t)page; ui.sub = 0; }
    ui.focus_t = 0;
    start_trans(200);
}

/* the value a knob shows: a held step's lock, or the kit */
static int knob_value(int code)
{
    const m2k_pattern_t *p = &pat[cur_pat];
    if (code >= 0x80) return p->glob[code & 0x7F];
    if (ui.grid && ui.nheld) {
        int i = lock_find(ui.trk, ui.held[0], code);
        if (i >= 0) return p->lock[i].val;
    }
    return p->kit[ui.trk][code];
}

static int knob_locked(int code)
{
    return code < 0x80 && ui.grid && ui.nheld && lock_find(ui.trk, ui.held[0], code) >= 0;
}

static void edit(int k, int d)
{
    int code = knob_param(ui.page, ui.sub, k);
    if (code == NONE) return;
    m2k_pattern_t *p = &pat[cur_pat];
    const pi_t *pi = pinfo(ui.trk, code);
    if (pi->s4[0] == '-') return;
    char old[12];
    const char *u;
    int v = knob_value(code);
    fmt(pi, ui.trk, v, old, &u);
    int n = kind_choices(pi->kind), nv;
    if (n) nv = enc(CLAMP(((v * n) >> 7) + d, 0, n - 1), n);
    else nv = CLAMP(v + d, kind_min(pi->kind), kind_max(pi->kind));
    if (code >= 0x80) {
        p->glob[code & 0x7F] = (uint8_t)nv;
    } else if (ui.grid && ui.nheld) {
        for (int i = 0; i < ui.nheld; i++) lock_set(ui.trk, ui.held[i], code, nv);
        trk[ui.trk].eff[code] = (uint8_t)nv;
        ui.held_edit = 1;
    } else if (sq.rec && sq.playing) {
        int s = sq.step ? sq.step - 1 : seq_len() - 1;
        if (p->step[ui.trk][s].flags & ST_TRIG) lock_set(ui.trk, s, code, nv);
        else p->kit[ui.trk][code] = (uint8_t)nv;
        trk[ui.trk].eff[code] = (uint8_t)nv;
    } else {
        p->kit[ui.trk][code] = (uint8_t)nv;
        trk[ui.trk].eff[code] = (uint8_t)nv;
    }
    if (nv != v) {
        if (ui.focus != k || ui.now - ui.roll_t > 90) s_cpy(ui.roll_old, old);
        ui.roll_t = ui.now;
    }
    ui.focus = (uint8_t)k;
    ui.focus_t = ui.now | 1u;
}

static void set_machine(int m)
{
    m2k_pattern_t *p = &pat[cur_pat];
    p->machine[ui.trk] = (uint8_t)m;
    m_cpy(p->kit[ui.trk], MACH_DEF[m], 8);
    m_cpy(trk[ui.trk].eff, p->kit[ui.trk], 8);
    start_glitch(4);
    ui.card_t = ui.now | 1u;
    ui.focus_t = 0;
}

static void held_remove(int s)
{
    int j = 0;
    for (int i = 0; i < ui.nheld; i++)
        if (ui.held[i] != s) { ui.held[j] = ui.held[i]; ui.held_added[j] = ui.held_added[i]; j++; }
    ui.nheld = (uint8_t)j;
}

void m2k_key(int k, int down)
{
    if (k < 0 || k >= M2K_KEYS) return;
    ui.keys[k] = (uint8_t)down;
    m2k_pattern_t *p = &pat[cur_pat];
    int w = ui.kw[k], b = ui.kb[k];
    if (ui.btn[B_GLO] && w >= 0 && w < M2K_TRACKS) {   /* GLO + white 1..6: mute */
        if (down) { p->mute ^= (uint8_t)(1u << w); ui.glo_used = 1; }
        return;
    }
    if (!ui.grid) {
        int note = 53 + k + 12 * ui.oct;
        sq.ktrk = ui.trk;
        if (down) { live_on(note, 100); ui.note[ui.trk] = (uint8_t)note; }
        else live_off(note);
        return;
    }
    if (b >= 0) {
        if (!down) return;
        if (b < M2K_TRACKS) { if (b != ui.trk) { ui.trk = (uint8_t)b; sq.ktrk = ui.trk; start_trans_dir(140, 1); } }
        else if (b < 10) { ui.spage = (uint8_t)(b - 6); ui.follow = 0; }
        else p->mute ^= (uint8_t)(1u << ui.trk);
        return;
    }
    int s = ui.spage * 16 + w;
    m2k_step_t *st = &p->step[ui.trk][s];
    if (down) {
        if (ui.nheld >= 8) return;
        int added = 0;
        if (!(st->flags & ST_TRIG)) {
            st->flags = ST_TRIG;
            st->note = ui.note[ui.trk];
            added = 1;
        }
        if (!ui.nheld) ui.held_edit = 0;
        ui.held[ui.nheld] = (uint8_t)s;
        ui.held_added[ui.nheld] = (uint8_t)added;
        ui.nheld++;
        ui.focus_t = 0;
    } else {
        int added = 1;
        for (int i = 0; i < ui.nheld; i++) if (ui.held[i] == s) added = ui.held_added[i];
        held_remove(s);
        if (!added && !ui.held_edit) {
            st->flags = 0;
            lock_clear_step(ui.trk, s);
        }
    }
}

static void held_step_edit(int dcond, int drtrg)
{
    m2k_pattern_t *p = &pat[cur_pat];
    for (int i = 0; i < ui.nheld; i++) {
        m2k_step_t *st = &p->step[ui.trk][ui.held[i]];
        if (dcond) st->cond = (uint8_t)CLAMP(st->cond + dcond, 0, C_COUNT - 1);
        if (drtrg) st->rtrg = (uint8_t)CLAMP((st->rtrg ? st->rtrg : 1) + drtrg, 1, 4);
    }
    ui.held_edit = 1;
    ui.roll_t = ui.now;
}

static void held_note(int d)
{
    m2k_pattern_t *p = &pat[cur_pat];
    for (int i = 0; i < ui.nheld; i++) {
        m2k_step_t *st = &p->step[ui.trk][ui.held[i]];
        st->note = (uint8_t)CLAMP(st->note + d, 12, 120);
        ui.note[ui.trk] = st->note;
    }
    ui.held_edit = 1;
    /* audition */
    m_cpy(trk[ui.trk].eff, p->kit[ui.trk], P_COUNT);
    if (ui.nheld) { apply_locks(ui.trk, ui.held[0]); voice_on(ui.trk, p->step[ui.trk][ui.held[0]].note, 100, 0); trk[ui.trk].gate_left = gate_blocks(ui.trk); }
}

void m2k_button(int b, int down)
{
    if (b < 0 || b >= B_COUNT) return;
    ui.btn[b] = (uint8_t)down;
    if (b == B_REC) {                  /* a tap arms recording; held, it is FILL */
        if (down) { ui.rec_t0 = ui.now | 1u; return; }
        if (ui.now - ui.rec_t0 < 300) sq.rec ^= 1;
        ui.rec_t0 = 0;
        sq.fill = 0;
        return;
    }
    if (b == B_GLO) {
        if (down) ui.glo_used = 0;
        else if (!ui.glo_used) go(PG_GLO);
        return;
    }
    if (!down) return;
    switch (b) {
    case B_FX: go(PG_FX); break;
    case B_SEL: go(PG_FILT); break;
    case B_ENV: go(PG_AMP); break;
    case B_LFO: go(PG_LFO); break;
    case B_EDIT: go(PG_SYN); break;
    case B_HOME: go(PG_HOME); break;
    case B_ARP: go(PG_ARP); break;
    case B_SAVE: {
        dirty = 1;
        ui.save_t = ui.now | 1u;
        char a[8] = "A0";
        s_int(a + 2, cur_pat + 1);
        toast("SAVED", a);
        break;
    }
    case B_SEQ:
        ui.grid ^= 1;
        ui.nheld = 0;
        toast(ui.grid ? "GRID" : "KEYS", ui.grid ? "STEPS ON THE KEYS" : "PLAY THE TRACK");
        start_trans(200);
        break;
    case B_PLAY: seq_play(!sq.playing); break;
    case B_OCTD: case B_OCTU: {
        int d = b == B_OCTU ? 1 : -1;
        if (ui.grid && ui.nheld) held_step_edit(0, d);
        else if (ui.grid) { ui.spage = (uint8_t)CLAMP(ui.spage + d, 0, 3); ui.follow = 0; }
        else { ui.oct = (int8_t)CLAMP(ui.oct + d, -3, 3); char o[8]; char *e = s_cpy(o, "OCT "); if (ui.oct > 0) *e++ = '+'; s_int(e, ui.oct); toast(o, 0); }
        break;
    }
    default: break;
    }
}

void m2k_encoder(int e, int d)
{
    if (!d) return;
    m2k_pattern_t *p = &pat[cur_pat];
    switch (e) {
    case E_SELECT:
        if (ui.grid && ui.nheld) { held_note(d); break; }
        for (int i = 0; i < 8 && sq.nheld; i++) live_off(sq.held[0]);
        ui.trk = (uint8_t)((ui.trk + M2K_TRACKS + (d > 0 ? 1 : -1)) % M2K_TRACKS);
        sq.ktrk = ui.trk;
        start_trans_dir(140, 1);
        break;
    case E_ALGO:
        if (ui.grid && ui.nheld) {
            for (int i = 0; i < ui.nheld; i++) {
                m2k_step_t *st = &p->step[ui.trk][ui.held[i]];
                int f = ((st->flags >> 1) + (d > 0 ? 1 : 3)) & 3;
                st->flags = (uint8_t)((st->flags & ST_TRIG) | (f << 1));
            }
            ui.held_edit = 1;
            break;
        }
        set_machine((p->machine[ui.trk] + M_COUNT + (d > 0 ? 1 : -1)) % M_COUNT);
        break;
    case E_PRESET: {
        if (ui.grid && ui.nheld) { held_step_edit(d > 0 ? 1 : -1, 0); break; }
        int n = ((sq.playing ? sq.queued : cur_pat) + M2K_PATTERNS + (d > 0 ? 1 : -1)) % M2K_PATTERNS;
        char a[8] = "A0";
        s_int(a + 2, n + 1);
        if (sq.playing) { sq.queued = (uint8_t)n; toast(a, "QUEUED"); }
        else {
            cur_pat = n;
            sq.queued = (uint8_t)n;
            lockmap_build();
            for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[n].kit[t], P_COUNT);
            toast(a, pat[n].name);
            start_glitch(4);
        }
        break;
    }
    case E_MASTER: {
        master_vol = (uint8_t)CLAMP(master_vol + d, 0, 127);
        char o[12];
        s_int(o, master_vol);
        toast("VOLUME", o);
        break;
    }
    default:
        if (e >= E_K1 && e <= E_K4) edit(e - E_K1, d);
        break;
    }
}

/* ---------------------------------------------------------------------------------------------- drawing */

#define GX 148                         /* the golden section of 240 */
#define FOCUS_Y 18
#define STRIP_Y 124
#define FOOT_Y 148

static void dotted_v(int x, int y, int h) { for (int j = 0; j < h; j += 2) px(x, y + j, 1); }
static void dotted_h(int x, int y, int w) { for (int i = 0; i < w; i += 2) px(x + i, y, 1); }

static void badge(int x, int y, const char *s)          /* after the Machinedrum's boxed machine logos */
{
    int w = text_w(&F_S, s, 1) + 12;
    frame(x, y, w, 13, 1);
    for (int i = 0; i < 3; i++) vline(x + 2 + i * 2, y + 3, 7, 1);
    text(&F_S, x + 9, y + 1, s, 1, 1);
}

static void header(void)
{
    /* right: transport, rec, pattern, grid */
    int xr = 239;
    if (sq.playing) {
        for (int j = 0; j < 9; j++) { int w = j < 5 ? j : 8 - j; hline(xr - 8, 3 + j, w + 1, 1); }
    } else {
        rect(xr - 7, 4, 7, 7, 1);
    }
    xr -= 12;
    if (sq.rec) {
        if ((ui.now >> 8) & 1) disc(xr - 3, 7, 3, 1);
        else frame(xr - 6, 4, 7, 7, 1);
        xr -= 10;
    }
    char pa[8] = "A0";
    s_int(pa + 2, cur_pat + 1);
    xr -= text_r(&F_S, xr, 2, pa, 1, 1) + 5;
    if (ui.grid) { int w = text_w(&F_S, "GRID", 1) + 6; tag(xr - w, 0, "GRID", 0); xr -= w + 4; }
    if (sq.fill) { int w = text_w(&F_S, "FILL", 1) + 6; tag(xr - w, 0, "FILL", 1); xr -= w + 4; }
    /* left: the page, the track, its machine; what does not fit is left out */
    char o[24];
    page_label(ui.page, ui.sub, o);
    int x = tag(0, 0, o, 1) + 4;
    m2k_pattern_t *p = &pat[cur_pat];
    const mi_t *mi = &MI[p->machine[ui.trk]];
    clip(0, 0, xr - 2, 15);
    if (ui.page == PG_HOME) {
        text(&F_S, x, 2, p->name, 1, 1);
    } else {
        char t[4] = {'T', (char)('1' + ui.trk), 0, 0};
        x += text(&F_S, x, 2, t, 1, 1) + 4;
        badge(x, 1, mi->badge);
        x += text_w(&F_S, mi->badge, 1) + 16;
        if (x + text_w(&F_S, mi->style, 1) < xr - 2) text(&F_S, x, 2, mi->style, 1, 1);
    }
    noclip();
    hline(0, 15, 240, 1);
}

/* the big value: an outline with the value's own density inside (the poster's dithered type) */
/* the big value: an outline with a vertical dither gradient inside, from the value's own density at the top to
 * solid at the foot (the poster's gradient type). It changes in place: text never leaves its line. */
static void big_value(int x, int y, int w, const char *s, int density, uint32_t roll)
{
    const m2k_font_t *f = &F_XL;
    if (text_w(f, s, 0) > w) f = &F_L;
    int yy = f == &F_XL ? y : y + 20;
    (void)roll;
    text_grad(yy, f->h - 6, CLAMP(density, 4, 60) - 4, 64);
    text(f, x, yy, s, TX_GRAD, 0);
}

static void focus_left(int code)
{
    char v[16], o[32];
    const char *u;
    const pi_t *pi = pinfo(ui.trk, code);
    int val = knob_value(code);
    fmt(pi, ui.trk, val, v, &u);
    char *e = o;
    if (code < 8) { *e++ = (char)('A' + code); *e++ = ' '; *e++ = '/'; *e++ = ' '; }
    s_cpy(e, pi->name);
    text(&F_S, 4, FOCUS_Y + 2, o, 1, 1);
    int dens = kind_choices(pi->kind) || pi->kind == K_DEST ? 40 : 8 + (val * 52) / kind_max(pi->kind);
    big_value(2, FOCUS_Y + 16, GX - 6, v, dens, ui.now - ui.roll_t);
    int x = 4;
    if (*u) x += text(&F_S, 4, 106, u, 1, 1) + 6;
    if (knob_locked(code)) tag(x, 104, "LOCK", 1);
    else if (sq.rec && sq.playing && code < 0x80) tag(x, 104, "REC", (ui.now >> 8) & 1);
}

/* the machine card, after Model:Cycles' engine screens: a rounded ink card, its class, style and stats */
static void machine_card(void)
{
    const mi_t *mi = &MI[pat[cur_pat].machine[ui.trk]];
    rbox(2, FOCUS_Y + 1, GX - 6, 90, 7, PT_SOLID);
    rect_grad(2, FOCUS_Y + 91, GX - 6, 12, 60, 0, 1);
    text(&F_M, 10, FOCUS_Y + 7, mi->name, 0, 1);
    char o[24];
    s_cpy(s_cpy(o, "CLASS:"), mi->cls);
    text(&F_S, 10, FOCUS_Y + 27, o, 0, 1);
    s_cpy(s_cpy(o, "STYLE:"), mi->style);
    text(&F_S, 10, FOCUS_Y + 39, o, 0, 1);
    static const char *const ST[3] = {"PNCH", "MTL", "PLST"};
    uint32_t age = ui.card_t ? ui.now - ui.card_t : 1000;
    for (int r = 0; r < 3; r++) {
        int y = FOCUS_Y + 53 + r * 12;
        text(&F_S, 10, y, ST[r], 0, 1);
        text(&F_S, 46, y, ":", 0, 0);
        for (int i = 0; i < 5; i++) {
            int shown = age > (uint32_t)(60 + (r * 5 + i) * 25);
            int full = i < mi->stat[r];
            int cx = 58 + i * 11, cy = y + 5;
            if (!shown) continue;
            for (int j = -3; j <= 3; j++) {
                int w = 3 - (j < 0 ? -j : j);
                for (int q = -w; q <= w; q++)
                    if (full || q == -w || q == w) px(cx + q, cy + j, 0);
            }
        }
    }
}

static void mark_3d(int x, int y, int m)
{
    int f = (ui.icon_ph >> 8) % ICON_FRAMES;
    /* a dithered shadow on the floor, thinner at the ends */
    for (int j = 0; j < 5; j++) {
        int w = 44 - j * 6;
        rect_grad(x + 32 - w / 2, y + ICON_N + 1 + j, w / 2, 1, 4, 28 - j * 5, 0);
        rect_grad(x + 32, y + ICON_N + 1 + j, w / 2, 1, 28 - j * 5, 4, 0);
    }
    blit(ICON_BITS[m][f], ICON_N, ICON_N, x, y, 1);
}

static void machine_dots(int cx, int y, int m)
{
    int x0 = cx - (M_COUNT * 9) / 2 + 4;
    for (int i = 0; i < M_COUNT; i++) {
        int x = x0 + i * 9;
        if (i == m) disc(x, y, 3, 1);
        else { frame(x - 3, y - 3, 7, 7, 1); px(x - 3, y - 3, 0); px(x + 3, y - 3, 0); px(x - 3, y + 3, 0); px(x + 3, y + 3, 0); }
    }
}

/* the right column: what the page is about, live */
static void picture(void)
{
    const int X = GX + 2, Y = FOCUS_Y + 2, W = 88;
    trk_t *t = &trk[ui.trk];
    m2k_pattern_t *p = &pat[cur_pat];
    const uint8_t *k = p->kit[ui.trk];
    int m = p->machine[ui.trk];
    switch (ui.page) {
    case PG_SYN:
    case PG_HOME: {
        mark_3d(X + (W - ICON_N) / 2, Y + 2, m);
        if (ui.page == PG_SYN) machine_dots(X + W / 2, Y + 80, m);
        else {
            char o[16] = {'T', (char)('1' + ui.trk), ' ', 0};
            s_cpy(o + 3, MI[m].name);
            text_c(&F_S, X + W / 2, Y + 68, o, 1, 1);
            for (int i = 0; i < M2K_TRACKS; i++) {
                int h = (ui.meter[i] * 18) >> 15;
                int x = X + 2 + i * 14;
                frame(x, Y + 82, 12, 18, 1);
                rect_grad(x + 1, Y + 99 - h, 10, h, i == ui.trk ? 30 : 8, i == ui.trk ? 64 : 40, 1);
            }
        }
        break;
    }
    case PG_AMP: {
        /* attack, hold, decay, release as lengths on a log scale; the voice's position on it */
        int wa = 6 + k[P_ATK] * 24 / 127, wh = k[P_HOLD] == 127 ? 18 : 2 + k[P_HOLD] * 18 / 127;
        int wd = k[P_DEC] == 127 ? 18 : 12 + k[P_DEC] * 30 / 127, wr = 8 + k[P_REL] * 24 / 127;
        int tot = wa + wh + wd + wr;
        wa = wa * (W - 4) / tot; wh = wh * (W - 4) / tot; wd = wd * (W - 4) / tot; wr = (W - 4) - wa - wh - wd;
        int x0 = X + 2, yb = Y + 86, top = Y + 14, hgt = yb - top;
        int xs[5] = {x0, x0 + wa, x0 + wa + wh, x0 + wa + wh + wd, x0 + wa + wh + wd + wr};
        int py = yb;
        for (int x = xs[0]; x <= xs[4] && x < X + W; x++) {
            int y;
            if (x <= xs[1]) y = yb - hgt * (x - xs[0]) / (wa ? wa : 1);
            else if (x <= xs[2]) y = top;
            else if (x <= xs[3]) { int q = (x - xs[2]) * 256 / (wd ? wd : 1); int l = 256 - q; l = k[P_DEC] == 127 ? 256 : (l * l) >> 8; y = yb - (hgt * l >> 8); }
            else { int q = (x - xs[3]) * 256 / (wr ? wr : 1); int l0 = k[P_DEC] == 127 ? 256 : 40; int l = (l0 * (256 - q)) >> 8; y = yb - (hgt * l >> 8); }
            for (int j = y; j <= yb; j++) if (T_BAYER[j & 7][x & 7] < 44 - (j - y) * 40 / (yb - top + 1)) px(x, j, 1);
            line(x - 1, py, x, y, 1);
            py = y;
        }
        hline(X, yb + 1, W, 1);
        static const char *const L[4] = {"A", "H", "D", "R"};
        for (int i = 0; i < 4; i++) if (xs[i + 1] - xs[i] > 5) text(&F_S, xs[i] + 1, yb + 4, L[i], 1, 0);
        /* the live voice */
        int st = mon.astage[ui.trk], lv = mon.aenv[ui.trk];
        if (st) {
            int y = yb - (hgt * lv >> 15);
            int x = st == 1 ? xs[0] + wa * lv / 32768 : st == 2 ? xs[1] + wh / 2 : st == 3 ? xs[2] + wd - wd * lv / 32768 : xs[3] + 2;
            vline(x, top - 4, yb - top + 4, 2);
            disc(x, y, 3, 1);
        }
        break;
    }
    case PG_FILT: {
        int hp = mon.cut_hp[ui.trk], lp = mon.cut_lp[ui.trk];
        if (!t->sounding) { hp = k[P_BASE] * 8; lp = CLAMP((k[P_BASE] + k[P_WDTH]) * 8, 0, 1016); }
        int yb = Y + 90, top = Y + 30, prev = yb;
        for (int i = 0; i < W; i++) {
            int q = i * 1016 / (W - 1), g = 256;
            if (q < hp) g = CLAMP(256 - (hp - q) * 3, 0, 256);
            if (q > lp) g = CLAMP(256 - (q - lp) * 3, 0, g);
            int dh = q - hp, dl = q - lp;
            if (dh > -40 && dh < 40) g += (k[P_HPQ] * (40 - (dh < 0 ? -dh : dh))) >> 4;
            if (dl > -40 && dl < 40) g += (k[P_LPQ] * (40 - (dl < 0 ? -dl : dl))) >> 4;
            int y = yb - ((yb - top) * g >> 8);
            if (y < Y) y = Y;
            for (int j = y + 1; j <= yb; j++) if (T_BAYER[j & 7][(X + i) & 7] < 48 - (j - y) * 44 / (yb - top + 1)) px(X + i, j, 1);
            line(X + i - 1, prev, X + i, y, 1);
            prev = y;
        }
        hline(X, yb + 1, W, 1);
        text(&F_S, X, yb + 4, "HP", 1, 1);
        text_r(&F_S, X + W, yb + 4, "LP", 1, 1);
        break;
    }
    case PG_FX: {
        if (ui.sub == 1) {             /* echoes: one bar is 64/64; each lights when it sounds */
            int dt = k[P_DTIM] ? k[P_DTIM] : 1;
            int fb = k[P_DFB];
            int lvl = 256;
            uint32_t ms_step = 15000u / seq_bpm();       /* a sixteenth in ms */
            uint32_t since = ui.now - ui.flash_t[ui.trk];
            for (int e = 0; e < 12; e++) {
                int x = X + 6 + (dt * (e + 1) * (W - 8)) / 32;
                if (x >= X + W) break;
                int h = (80 * lvl) >> 8;
                uint32_t at = (uint32_t)dt * (uint32_t)(e + 1) * ms_step / 4u;
                int lit = since >= at && since < at + 90;
                rect_pat(x, Y + 88 - h, 4, h, lit ? PT_SOLID : PT_CHECK, 0);
                lvl = (lvl * fb) >> 7;
                if (lvl < 8) break;
            }
            rect(X, Y + 2, 4, 86, 1);
            hline(X, Y + 89, W, 1);
        } else if (ui.sub == 2) {      /* a sine through this track's destruction */
            int bits = 16 - (k[P_BITS] * 14 >> 7), fold = k[P_FOLD], xm = (k[P_XOR] * k[P_XOR] * 2) & 0x7FFF;
            int srr = k[P_SRR], step = (128 - srr) * (128 - srr) * 4, acc = 0, hold = 0, prev = 0;
            uint32_t ph = (uint32_t)ui.now * 2000000u;
            for (int i = 0; i < W; i++) {
                int32_t s = sinp(ph + (uint32_t)i * (0xFFFFFFFFu / W) * 2) * 7 / 8;
                if (fold) s = fold16((s * (256 + fold * 6)) >> 8);
                if (xm) s = (int16_t)((uint16_t)s ^ (uint16_t)xm);
                if (bits < 16) s = (s >> (16 - bits)) << (16 - bits);
                acc += step;
                if (acc >= 65536 || i == 0) { acc -= 65536; hold = s; }
                if (srr) s = hold;
                int y = Y + 46 - (s * 40 >> 15);
                if (i) line(X + i - 1, prev, X + i, y, 1);
                prev = y;
            }
            dotted_h(X, Y + 46, W);
        } else {                       /* EQ: the band and its gain */
            int f = k[P_EQF] * (W - 1) / 127, g = k[P_EQG] - 64, prev = 0;
            for (int i = 0; i < W; i++) {
                int d = i - f;
                int bump = d > -22 && d < 22 ? (g * (22 - (d < 0 ? -d : d))) / 22 : 0;
                int y = Y + 46 - bump * 40 / 64;
                for (int j = (y < Y + 46 ? y : Y + 46); j <= (y < Y + 46 ? Y + 46 : y); j++) if (pat_ink(PT_DOTS, X + i, j)) px(X + i, j, 1);
                if (i) line(X + i - 1, prev, X + i, y, 1);
                prev = y;
            }
            dotted_h(X, Y + 46, W);
            /* the rate reduction, as steps */
            int n = 4 + (127 - k[P_SRR]) * 40 / 127;
            for (int i = 0; i < W; i++) {
                int q = (i * n / W) * W / n;
                int y = Y + 92 - (sinp((uint32_t)q * (0xFFFFFFFFu / W)) * 6 >> 15);
                px(X + i, y, 1);
            }
        }
        break;
    }
    case PG_LFO: {
        int l = ui.sub >> 1;
        const uint8_t *b = &k[P_LFO + 8 * l];
        int wave = (b[L_WAVE] * 7) >> 7, prev = 0;
        int32_t r = 0;
        for (int i = 0; i < W; i++) {           /* the shape, dotted */
            int32_t v = lfo_shape(wave, (uint32_t)i * (0xFFFFFFFFu / W), &r);
            if (wave == 6) v = ((i / 11) * 7919 % 64 - 32) * 1000;
            int y = Y + 30 - (v * 26 >> 15);
            if ((i & 1) == 0) px(X + i, y, 1);
        }
        for (int i = 0; i < W; i++) {           /* the live value, scrolling */
            int v = ui.lfo_hist[(ui.lfo_w + i) % W];
            int y = Y + 76 - (v * 22 >> 15);
            if (i) line(X + i - 1, prev, X + i, y, 1);
            prev = y;
        }
        dotted_h(X, Y + 56, W);
        hline(X, Y + 98, W, 1);
        break;
    }
    case PG_ARP: {
        int n = sq.nheld, rng = 1 + ((k[P_ARNG] * 4) >> 7), tot = n * rng;
        for (int i = 0; i < tot && i < 24; i++) {
            int x = X + (i % 8) * 11, y = Y + 70 - (i / 8) * 26 - (i % 8) * 2;
            int on = (int)(sq.arp_i ? (sq.arp_i - 1) % (tot ? tot : 1) : -1) == i;
            if (on) rect(x, y, 9, 9, 1); else frame(x, y, 9, 9, 1);
        }
        if (!n) { text_c(&F_S, X + W / 2, Y + 34, "HOLD", 1, 1); text_c(&F_S, X + W / 2, Y + 48, "SOME KEYS", 1, 1); }
        break;
    }
    case PG_GLO: {
        if (ui.sub == 1) {             /* the master, and the repeat when it bites */
            int prev = 0;
            for (int i = 0; i < W; i++) {
                int v = mon.scope[(uint8_t)(mon.scope_w - W * 2 + i * 2)];
                int y = Y + 44 - (v * 40 >> 15);
                if (i) line(X + i - 1, prev, X + i, y, 1);
                prev = y;
            }
            if (mon.glitch) tag(X + 2, Y + 88, "REPEAT", 1);
            else tag(X + 2, Y + 88, "REPEAT", 0);
        } else if (ui.sub == 2) {      /* the room: squares out from the middle on each beat */
            int ph = sq.playing ? (int)(sq.step & 3) : 0;
            int n = 2 + p->glob[G_VERB] / 20;
            for (int i = 0; i < n; i++) {
                int r = 6 + ((i * 8 + ph * 2) % (n * 8));
                frame(X + W / 2 - r, Y + 46 - r, 2 * r, 2 * r, 1);
            }
        } else {                       /* four beats */
            int beat = sq.playing ? (sq.step ? sq.step - 1 : 0) / 4 % 4 : -1;
            for (int i = 0; i < 4; i++) {
                int x = X + 2 + i * 22;
                if (i == beat) rect(x, Y + 20, 18, 40, 1);
                else rect_pat(x, Y + 20, 18, 40, PT_DOTS, 0), frame(x, Y + 20, 18, 40, 1);
            }
            char o[12];
            s_int(o, (int)seq_bpm());
            text_c(&F_L, X + W / 2, Y + 66, o, 1, 0);
        }
        break;
    }
    default:
        break;
    }
}

static int play_step(void) { int s = (int)sq.step - 1; return s < 0 ? seq_len() - 1 : s; }

static void step_cell(int x, int y, int w, int h, int t, int s)
{
    m2k_pattern_t *p = &pat[cur_pat];
    m2k_step_t st = p->step[t][s];
    int playing = sq.playing && play_step() == s;
    int held = 0;
    if (ui.grid && t == ui.trk) for (int i = 0; i < ui.nheld; i++) if (ui.held[i] == s) held = 1;
    if (s >= seq_len()) { rect_pat(x, y, w, h, PT_SPARSE, 0); return; }
    if (st.flags & ST_TRIG) {
        /* the fill says how sure the trig is: solid always, Bayer by chance, textures for the conditions */
        int c = st.cond, lk = has_locks(t, s);
        if (c == C_ALWAYS && !lk) rect(x, y, w, h, 1);
        else {
            int pt = c == C_ALWAYS ? PT_CHECK : c <= C_P12 ? COND_DENS[c] : c <= C_4_4 ? PT_DIAG : c <= C_NPRE ? PT_VLINES : PT_LINES;
            frame(x, y, w, h, 1);
            rect_pat(x + 1, y + 1, w - 2, h - 2, pt, 1);
            if (lk && c != C_ALWAYS) { rect(x + w - 4, y, 4, 4, 1); }
        }
        if (st.flags & ST_ACC) rect(x + 2, y + 2, 3, 3, 0);
        if (st.flags & ST_SLD) for (int j = 0; j < h; j += 2) px(x + w - 2, y + j, 0);
        for (int i = 1; i < st.rtrg && i < 4; i++) rect(x + 1 + i * 3, y + h - 3, 2, 2, 2);
    } else {
        px(x + w / 2, y + h / 2, 1);
    }
    if (playing) frame(x - 1, y - 1, w + 2, h + 2, 2);
    if (held && ((ui.now >> 7) & 1)) rect(x, y, w, h, 2);
}

static void strip(void)
{
    int pg = ui.spage;
    for (int i = 0; i < 16; i++) step_cell(i * 15, STRIP_Y + 2, 14, 13, ui.trk, pg * 16 + i);
    for (int i = 0; i < 4; i++) {
        int x = i * 60;
        if (i == pg) rect(x, STRIP_Y + 18, 59, 3, 1);
        else dotted_h(x, STRIP_Y + 19, 59);
    }
    if (sq.playing) {
        int s = play_step();
        rect(s * 240 / 64, STRIP_Y + 17, 4, 5, 2);
    }
}

static void footer(void)
{
    rect(0, FOOT_Y, 240, 2, 1);
    for (int k = 0; k < 4; k++) {
        int x = k * 60, code = knob_param(ui.page, ui.sub, k);
        if (k) dotted_v(x - 1, FOOT_Y + 4, 86);
        if (code == NONE) continue;
        const pi_t *pi = pinfo(ui.trk, code);
        if (pi->s4[0] == '-') { text(&F_S, x + 4, FOOT_Y + 6, "-", 1, 1); continue; }
        int v = knob_value(code);
        int focus = ui.focus_t && ui.focus == k && ui.now - ui.focus_t < 2500;
        if (focus) { rect(x + 2, FOOT_Y + 4, 56, 15, 1); text(&F_S, x + 5, FOOT_Y + 6, pi->s4, 0, 1); }
        else text(&F_S, x + 4, FOOT_Y + 6, pi->s4, 1, 1);
        char o[16];
        const char *u;
        fmt(pi, ui.trk, v, o, &u);
        const m2k_font_t *f = text_w(&F_M, o, 0) > 52 ? &F_S : &F_M;
        if (knob_locked(code)) { int w = text_w(f, o, 0); rect(x + 2, FOOT_Y + 21, w + 4, f->h + 2, 1); text(f, x + 4, FOOT_Y + 22, o, 0, 0); }
        else text(f, x + 4, FOOT_Y + 22, o, 1, 0);
        /* the slab */
        int bx = x + 4, by = FOOT_Y + 44, bw = 52, bh = 42;
        int n = kind_choices(pi->kind);
        if (n && n <= 8) {
            int c = (v * n) >> 7, sh = (bh - (n - 1)) / n;
            for (int i = 0; i < n; i++) {
                int y = by + bh - (i + 1) * (sh + 1) + 1;
                if (i == c) rect(bx, y, bw, sh, 1);
                else if (i < c) rect_pat(bx, y, bw, sh, PT_DOTS, 0);
                else frame(bx, y, bw, sh, 1);
            }
            continue;
        }
        int mx = kind_max(pi->kind), mn = kind_min(pi->kind);
        int target = ((v - mn) * 256 * bh) / (mx - mn ? mx - mn : 1);
        ui.ease[k] += (target - ui.ease[k]) / 3;
        int h = ui.ease[k] >> 8;
        rect_pat(bx, by, bw, bh, PT_SPARSE, 0);
        frame(bx, by, bw, bh, 1);
        int bip = pi->kind == K_BI || pi->kind == K_PAN || pi->kind == K_SEMI || pi->kind == K_FINE || pi->kind == K_SEMI24;
        if (bip) {
            int mid = by + bh / 2;
            hline(bx - 2, mid, bw + 4, 1);
            if (h > bh / 2) rect_grad(bx, by + bh - h, bw, h - bh / 2, 18, 64, 1);
            else rect_grad(bx, mid, bw, bh / 2 - h, 64, 18, 1);
        } else {
            rect(bx, by + bh - 2 > by + bh - h ? by + bh - 2 : by + bh - h, bw, h < 2 ? h : 2, 1);
            rect_grad(bx, by + bh - h, bw, h, 18, 64, 1);
        }
    }
}

static void step_card(void);

static void home(void)
{
    m2k_pattern_t *p = &pat[cur_pat];
    int fresh = ui.focus_t && ui.now - ui.focus_t < 1800;
    if (ui.grid && ui.nheld && !fresh) {
        step_card();
    } else if (fresh) {
        focus_left(knob_param(PG_HOME, 0, ui.focus));
    } else {
        text(&F_S, 4, FOCUS_Y + 2, "PATTERN", 1, 1);
        char a[8] = "A0";
        s_int(a + 2, cur_pat + 1);
        text_grad(FOCUS_Y + 16, F_XL.h - 6, 10, 64);
        text(&F_XL, 2, FOCUS_Y + 16, a, TX_GRAD, 0);
        char o[32], *e = s_int(o, (int)seq_bpm());
        e = s_cpy(e, " BPM  ");
        e = s_int(e, seq_len());
        s_cpy(e, " STEPS");
        text(&F_S, 4, 106, o, 1, 1);
    }
    dotted_v(GX - 2, FOCUS_Y, 104);
    picture();
    /* where the pattern is: the four pages, the playhead */
    for (int pg = 0; pg < 4; pg++) {
        int x = pg * 60, w = 58;
        int used = pg * 16 < seq_len();
        if (!used) { rect_pat(x, STRIP_Y + 4, w, 14, PT_SPARSE, 0); continue; }
        frame(x, STRIP_Y + 4, w, 14, 1);
        for (int i = 1; i < 16; i++) px(x + i * w / 16, STRIP_Y + 16, 1);
        if (pg == ui.spage) rect_pat(x + 1, STRIP_Y + 5, w - 2, 12, PT_DOTS, 0);
        if (sq.playing && play_step() / 16 == pg) rect(x + (play_step() % 16) * w / 16, STRIP_Y + 4, 4, 14, 1);
    }
    /* the six lanes */
    rect(0, FOOT_Y, 240, 2, 1);
    for (int t = 0; t < M2K_TRACKS; t++) {
        int y = FOOT_Y + 3 + t * 15;
        int flash = ui.now - ui.flash_t[t] < 90;
        char n[2] = {(char)('1' + t), 0};
        if (t == ui.trk || flash) { rect(0, y, 13, 13, 1); text(&F_S, 3, y + 1, n, 0, 0); }
        else text(&F_S, 3, y + 1, n, 1, 0);
        int muted = (p->mute >> t) & 1;
        for (int i = 0; i < 16; i++) {
            int s = ui.spage * 16 + i;
            int x = 16 + i * 14;
            if (muted) { if (p->step[t][s].flags & ST_TRIG) rect_pat(x, y + 1, 12, 11, PT_SPARSE, 0); else px(x + 6, y + 6, 1); frame(x, y + 1, 12, 11, 0); continue; }
            step_cell(x, y + 1, 12, 11, t, s);
        }
        if (muted) for (int i = 16; i < 240; i += 3) px(i, y + 6, 1);
    }
}

/* a held step: its note in big type, its condition, ratchet, accent and slide */
static void step_card(void)
{
    m2k_step_t st = pat[cur_pat].step[ui.trk][ui.held[0]];
    char o[24], *e = s_cpy(o, "STEP ");
    e = s_int(e, ui.held[0] + 1);
    if (ui.nheld > 1) { e = s_cpy(e, " +"); s_int(e, ui.nheld - 1); }
    text(&F_S, 4, FOCUS_Y + 2, o, 1, 1);
    s_note(o, st.note);
    big_value(2, FOCUS_Y + 16, GX - 6, o, 64, 1000);
    int x = 4;
    x += tag(x, 102, COND_N[st.cond], st.cond != C_ALWAYS) + 3;
    char r[4] = {'X', (char)('0' + (st.rtrg ? st.rtrg : 1)), 0, 0};
    x += tag(x, 102, r, st.rtrg > 1) + 3;
    if (st.flags & ST_ACC) x += tag(x, 102, "ACC", 1) + 3;
    if (st.flags & ST_SLD) tag(x, 102, "SLD", 1);
}

static void param_page(void)
{
    int fresh = ui.focus_t && ui.now - ui.focus_t < 2500;
    if (ui.grid && ui.nheld && !fresh) step_card();
    else if (ui.page == PG_SYN && (!fresh || (ui.card_t && ui.now - ui.card_t < 1500))) machine_card();
    else if (fresh) focus_left(knob_param(ui.page, ui.sub, ui.focus));
    else {
        /* resting: the page in big type, its four values listed */
        char o[24];
        page_label(ui.page, ui.sub, o);
        for (char *c = o; *c; c++) if (*c == ' ') { *c = 0; break; }
        text(&F_S, 4, FOCUS_Y + 2, "PAGE", 1, 1);
        big_value(2, FOCUS_Y + 16, GX - 6, o, 26, 1000);
    }
    dotted_v(GX - 2, FOCUS_Y, 104);
    picture();
    strip();
    footer();
}

static void boot(uint32_t t)
{
    cls();
    int y0 = 92;
    /* the logo, swept in left to right behind a dithered edge */
    int p = (int)CLAMP((int)t * 64 / 650, 0, 64);
    for (int j = 0; j < LOGO_H; j++)
        for (int i = 0; i < LOGO_W; i++) {
            int th = (4 + i) * 48 / 240 + (T_BAYER[(y0 + j) & 7][(4 + i) & 7] >> 2);
            if (th < p && ((LOGO_BITS[j * ((LOGO_W + 7) >> 3) + (i >> 3)] >> (7 - (i & 7))) & 1)) px(4 + i, y0 + j, 1);
        }
    /* two gradient bands, the screen's grid drawn in */
    int g = (int)CLAMP(((int)t - 250) * 240 / 500, 0, 240);
    rect_grad(0, y0 - 22, g, 12, 0, 64, 0);
    rect_grad(240 - g, y0 + LOGO_H + 10, g, 12, 64, 0, 0);
    dotted_v(GX, 0, g * (y0 - 24) / 240);
    static const char sub[] = "SIX MONO MACHINES";
    int n = (int)CLAMP(((int)t - 650) / 28, 0, (int)sizeof sub - 1);
    char o[24];
    m_cpy(o, sub, (uint32_t)n);
    o[n] = 0;
    text(&F_S, 4, y0 + LOGO_H + 28, o, 1, 2);
    if (t > 1100) {
        text(&F_S, 4, 6, "FM-1", 1, 1);
        text_r(&F_S, 236, 6, "V0.2", 1, 1);
        text(&F_S, 4, 222, "LOFI 2000", 1, 1);
        text_r(&F_S, 236, 222, "6 X 64", 1, 1);
    }
    if (t < 360) glitch(fb, t % 120, 0x2000u + t / 120, 3);
}

/* a message takes the header for a moment: inverted, its text, a gradient tail; it never covers the page */
static void overlays(void)
{
    if (!ui.toast_t) return;
    uint32_t a = ui.now - ui.toast_t;
    if (a >= 1200) return;
    int w = a < 120 ? (int)a * 240 / 120 : 240;      /* it sweeps in */
    rect(0, 0, w, 15, 1);
    clip(0, 0, w, 15);
    int x = 4 + text(&F_S, 4, 2, ui.toast, 0, 1) + 6;
    if (ui.toast2[0]) x += text(&F_S, x, 2, ui.toast2, 0, 1) + 6;
    noclip();
    if (w == 240 && x < 236) {
        rect(x, 0, 240 - x, 15, 0);
        rect_grad(x, 0, 240 - x, 15, 64, 0, 0);
    }
    hline(0, 15, 240, 1);
}

void m2k_render_ui_tick(void);

const uint8_t *m2k_frame(uint32_t ms)
{
    uint32_t dt = ui.now ? ms - ui.now : 16;
    if (ui.boot_t0 == 0xFFFFFFFFu) ui.boot_t0 = ms;
    ui.now = ms;
    /* what the audio did */
    int boost = 0;
    for (int t = 0; t < M2K_TRACKS; t++) {
        if (mon.trig[t] != ui.seen_trig[t]) {
            ui.seen_trig[t] = mon.trig[t];
            ui.flash_t[t] = ms;
            if (t == ui.trk) boost = 1;
        }
        int32_t pk = mon.peak[t] * 2;
        mon.peak[t] = 0;
        ui.meter[t] = pk > ui.meter[t] ? CLAMP(pk, 0, 32767) : (ui.meter[t] * 7) >> 3;
    }
    if (boost) ui.icon_spd = 34 << 8;
    ui.icon_spd += ((6 << 8) - ui.icon_spd) >> 3;
    ui.icon_ph += (int32_t)((ui.icon_spd * (int32_t)dt) / 1000);
    ui.lfo_hist[ui.lfo_w] = (int16_t)mon.lfo[ui.trk][ui.page == PG_LFO ? ui.sub >> 1 : 0];
    ui.lfo_w = (uint8_t)((ui.lfo_w + 1) % 88);
    if (sq.playing && ui.follow && !ui.nheld) ui.spage = (uint8_t)(play_step() / 16);
    if (ui.rec_t0 && ui.btn[B_REC] && ms - ui.rec_t0 >= 300) sq.fill = 1;

    uint32_t bt = ms - ui.boot_t0;
    if (bt < 1800) {
        boot(bt);
        m_cpy(fb_out, fb, sizeof fb_out);
        if (bt > 1650 && !ui.in_trans) { /* hand over */ }
        return fb_out;
    }
    if (bt < 1800 + 40 && !ui.in_trans && ui.trans_t < ui.boot_t0 + 1800) start_trans(360);

    cls();
    if (ui.page == PG_HOME) { header(); home(); }
    else { header(); param_page(); }
    overlays();

    if (ui.in_trans) {
        uint32_t a = ms - ui.trans_t;
        if (a >= ui.trans_len) { ui.in_trans = 0; m_cpy(fb_out, fb, sizeof fb_out); }
        else wipe((int)(a * 64 / ui.trans_len), ui.trans_down);
    } else {
        m_cpy(fb_out, fb, sizeof fb_out);
    }
    if (ui.glitch_t && ms - ui.glitch_t < 120) glitch(fb_out, ms - ui.glitch_t, ui.glitch_seed, ui.glitch_amt);
    return fb_out;
}

const uint8_t *m2k_leds(void)
{
    m2k_pattern_t *p = &pat[cur_pat];
    uint8_t *L = ui.leds;
    m_set(L, 0, sizeof ui.leds);
    int ps = sq.playing ? play_step() : -1;
    for (int k = 0; k < M2K_KEYS; k++) {
        int w = ui.kw[k], b = ui.kb[k], v = 0;
        if (ui.grid) {
            if (w >= 0) {
                int s = ui.spage * 16 + w;
                if (s < seq_len()) {
                    if (p->step[ui.trk][s].flags & ST_TRIG) v = 110;
                    if (s == ps) v = 255;
                    for (int i = 0; i < ui.nheld; i++) if (ui.held[i] == s) v = ((ui.now >> 7) & 1) ? 255 : 40;
                }
            } else if (b < M2K_TRACKS) {
                v = b == ui.trk ? 255 : (ui.now - ui.flash_t[b] < 80) ? 160 : 24;
            } else if (b < 10) {
                v = b - 6 == ui.spage ? 255 : (ps >= 0 && ps / 16 == b - 6) ? 90 : 16;
            } else {
                v = (p->mute >> ui.trk) & 1 ? 255 : 16;
            }
        } else {
            int note = 53 + k + 12 * ui.oct;
            if (ui.keys[k]) v = 255;
            else if (trk[ui.trk].sounding && trk[ui.trk].note == note) v = 120;
        }
        L[k] = (uint8_t)v;
    }
    uint8_t *B = L + M2K_KEYS;
    B[B_FX] = ui.page == PG_FX ? 255 : 0;
    B[B_SEL] = ui.page == PG_FILT ? 255 : 0;
    B[B_ENV] = ui.page == PG_AMP ? 255 : 0;
    B[B_LFO] = ui.page == PG_LFO ? 255 : 0;
    B[B_EDIT] = ui.page == PG_SYN ? 255 : 0;
    B[B_GLO] = ui.page == PG_GLO ? 255 : 0;
    B[B_HOME] = ui.page == PG_HOME ? 255 : 0;
    B[B_ARP] = ui.page == PG_ARP ? 255 : 0;
    B[B_SAVE] = ui.save_t && ui.now - ui.save_t < 300 ? 255 : 0;
    B[B_SEQ] = ui.grid ? 255 : 0;
    B[B_PLAY] = sq.playing ? ((play_step() & 3) == 0 ? 255 : 70) : 0;
    B[B_REC] = sq.rec ? 255 : 0;
    B[B_OCTD] = ui.oct < 0 ? 200 : 0;
    B[B_OCTU] = ui.oct > 0 ? 200 : 0;
    return L;
}
