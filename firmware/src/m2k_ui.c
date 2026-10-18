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
    K_NUM, K_BI, K_TIME, K_HOLD, K_DECT, K_ATIM, K_AHLD, K_ADEC, K_AREL, K_MODT, K_MHZ, K_HZ, K_PAN, K_DTIM, K_BITS, K_SEMI, K_RATIO, K_SEMI24, K_INT25, K_SIDW, K_SIDM,
    K_ALGO, K_VOIC, K_WAVE, K_VOWL, K_TRK, K_DEST, K_LWAV, K_LTRG, K_MULT, K_AMOD, K_ASPD, K_ARNG, K_GATE,
    K_BPM, K_SWNG, K_LEN, K_GLCH, K_GSIZ, K_FINE, K_TUN2, K_SCRN, K_PROJ, K_LOAD,
    K_FOCT, K_FWID, K_FATK, K_FDEC, K_EQHZ, K_DBHZ, K_SRR, K_PCH, K_WBNK, K_FR24, K_FR64, K_FRSQ, K_ONOFF, K_MSRC, K_MFRQ
};
typedef struct { const char *s4, *name; uint8_t kind; } pi_t;

static const pi_t PI_COMMON[P_LFO - 8] = {
    {"ATCK", "ATTACK", K_ATIM}, {"HOLD", "HOLD", K_AHLD}, {"DEC", "DECAY", K_ADEC}, {"REL", "RELEASE", K_AREL},
    {"DIST", "DISTORTION", K_BI}, {"VOL", "VOLUME", K_NUM}, {"PAN", "PAN", K_PAN}, {"PORT", "PORTAMENTO", K_NUM},
    {"BASE", "FILTER BASE", K_FOCT}, {"WDTH", "FILTER WIDTH", K_FWID}, {"HPQ", "HP RESONANCE", K_NUM}, {"LPQ", "LP RESONANCE", K_NUM},
    {"ATCK", "FILTER ATTACK", K_FATK}, {"DEC", "FILTER DECAY", K_FDEC}, {"B-OF", "BASE BY ENV", K_BI}, {"W-OF", "WIDTH BY ENV", K_BI},
    {"EQF", "EQ FREQUENCY", K_EQHZ}, {"EQG", "EQ GAIN", K_BI}, {"SRR", "RATE REDUCTION", K_SRR}, {"DTIM", "DELAY TIME", K_DTIM},
    {"DSND", "DELAY SEND", K_BI}, {"DFB", "DELAY FEEDBACK", K_NUM}, {"DBAS", "DELAY BASE", K_DBHZ}, {"DWID", "DELAY WIDTH", K_NUM},
    {"BITS", "BIT DEPTH", K_BITS}, {"FOLD", "WAVEFOLD", K_NUM}, {"XOR", "XOR MANGLE", K_NUM}, {"VERB", "REVERB SEND", K_NUM},
    {"PTCH", "TRACK PITCH", K_SEMI}, {"LEVL", "TRACK LEVEL", K_NUM}, {"MODE", "ARP MODE", K_AMOD}, {"SPD", "ARP SPEED", K_ASPD},
    {"RNG", "ARP RANGE", K_ARNG}, {"GATE", "GATE LENGTH", K_GATE},
};
static const pi_t PI_LFO[8] = {
    {"TRCK", "LFO TRACK", K_TRK}, {"DEST", "LFO DESTINATION", K_DEST}, {"WAVE", "LFO WAVE", K_LWAV}, {"TRIG", "LFO TRIG MODE", K_LTRG},
    {"MULT", "LFO MULTIPLIER", K_MULT}, {"SPD", "LFO SPEED", K_NUM}, {"PHAS", "START PHASE", K_NUM}, {"DPTH", "LFO DEPTH", K_BI},
};
static const pi_t PI_WBNK = {"BANK", "WAVE BANK", K_WBNK};
static const pi_t PI_MOD[4] = {
    {"TYPE", "MOD TYPE", K_MODT}, {"RATE", "MOD RATE", K_MHZ}, {"DPTH", "MOD DEPTH", K_NUM}, {"MIX", "MOD MIX", K_NUM},
};

static const pi_t PI_GLOB[G_COUNT] = {
    {"BPM", "TEMPO", K_BPM}, {"SWNG", "SWING", K_SWNG}, {"LEN", "PATTERN LENGTH", K_LEN}, {"ACNT", "ACCENT", K_NUM},
    {"TAPE", "TAPE WOW", K_NUM}, {"ADPC", "ADPCM CRUNCH", K_NUM}, {"GLCH", "GLITCH CHANCE", K_GLCH}, {"SIZE", "REPEAT SIZE", K_GSIZ},
    {"DCAY", "REVERB DECAY", K_NUM}, {"DAMP", "REVERB DAMPING", K_NUM}, {"HISS", "HISS AND DUST", K_NUM}, {"DRIV", "BUS DRIVE", K_NUM},
};

static const pi_t PI_SET[3] = {
    {"SCRN", "SCREEN", K_SCRN},
    {"PROJ", "PROJECT", K_PROJ},
    {"LOAD", "LOAD PROJECT", K_LOAD},
};

typedef struct {
    const char *name, *badge, *style, *cls;
    uint8_t stat[3];                   /* punch, metal, plastic: 0..5 */
    pi_t syn[8];
} mi_t;

/* the machines as the selector steps through them: the Monomachine's order (its FM+ STAT, PAR, DYN together) */
static const uint8_t M_ORDER[M_COUNT] = {M_GSIN, M_GNOI, M_SID, M_FMS, M_FMP, M_FMD, M_SAW, M_ENS, M_WAV, M_VOX};
static int m_place(int m) { for (int i = 0; i < M_COUNT; i++) if (M_ORDER[i] == m) return i; return 0; }

static const mi_t MI[M_COUNT] = {
    {"GND-SIN", "GND", "SINE", "PERC", {5, 1, 3},
     {{"TUNE", "TUNE", K_TUN2}, {"FOLD", "SINE FOLD", K_NUM}, {"SUB", "SUB LEVEL", K_NUM}, {"NOIS", "NOISE", K_NUM},
      {"PDEP", "PITCH DROP", K_NUM}, {"PDEC", "DROP TIME", K_TIME}, {"CLIK", "CLICK", K_NUM}, {"HARM", "2ND HARMONIC", K_NUM}}},
    {"GND-NOIS", "GND", "NOISE", "PERC", {3, 4, 1},
     {{"COLR", "COLOUR", K_BI}, {"S&H", "PITCHED HOLD", K_NUM}, {"META", "METAL MIX", K_NUM}, {"MSPR", "METAL SPREAD", K_NUM},
      {"PDEP", "PITCH DROP", K_NUM}, {"PDEC", "DROP TIME", K_TIME}, {"GRIT", "8-BIT GRIT", K_NUM}, {"-", "-", K_NUM}}},
    {"SID-6581", "SID", "CHIP", "SYNTH", {3, 2, 4},
     {{"PW", "PULSE WIDTH", K_NUM}, {"PWAD", "PW SWEEP", K_NUM}, {"PWRS", "SWEEP RESET", K_ONOFF}, {"WAVE", "WAVE", K_SIDW},
      {"MOD", "RING / SYNC", K_SIDM}, {"MSRC", "MOD SOURCE", K_MSRC}, {"MFRQ", "MOD FREQUENCY", K_MFRQ}, {"TUNE", "TUNE", K_TUN2}}},
    {"FM+STAT", "FM+", "STATIC", "SYNTH", {4, 5, 3},
     {{"1FRQ", "RATIO 1", K_FR24}, {"1FIN", "FINE 1", K_BI}, {"1ENV", "DEPTH / DECAY 1", K_BI}, {"1FB", "FEEDBACK 1", K_NUM},
      {"2FRQ", "RATIO 2", K_FR24}, {"2VOL", "DEPTH 2", K_NUM}, {"TONE", "TONE", K_NUM}, {"TUNE", "TUNE", K_TUN2}}},
    {"FM+DYN", "FM+", "DYNAMIC", "SYNTH", {4, 4, 5},
     {{"1FRQ", "RATIO 1", K_FR64}, {"1FEN", "RATIO 1 BY ENV", K_BI}, {"1VOL", "DEPTH 1", K_NUM}, {"1VEN", "DEPTH 1 BY ENV", K_BI},
      {"2FRQ", "RATIO 2", K_FRSQ}, {"2ENV", "DEPTH / DECAY 2", K_BI}, {"2FB", "FEEDBACK 2", K_NUM}, {"TUNE", "TUNE", K_TUN2}}},
    {"SWAV-SAW", "SWV", "SUPER", "SYNTH", {2, 1, 2},
     {{"UNIL", "UNISON LEVEL", K_NUM}, {"UNIW", "UNISON WIDTH", K_NUM}, {"UNIX", "EXTENDED UNISON", K_NUM}, {"DRFT", "DRIFT", K_NUM},
      {"SUBX", "SQUARE SUB", K_NUM}, {"SUB1", "SINE SUB -1", K_NUM}, {"SUB2", "SINE SUB -2", K_NUM}, {"TUNE", "TUNE", K_TUN2}}},
    {"SWAV-ENS", "SWV", "CHORD", "SYNTH", {2, 1, 3},
     {{"PCH2", "PITCH 2", K_PCH}, {"PCH3", "PITCH 3", K_PCH}, {"PCH4", "PITCH 4", K_PCH}, {"WAVE", "SAW TO PULSE", K_NUM},
      {"PW", "PULSE WIDTH", K_NUM}, {"CHRL", "CHORUS LEVEL", K_NUM}, {"CHRW", "CHORUS WIDTH", K_NUM}, {"TUNE", "TUNE", K_TUN2}}},
    {"DIGI-WAV", "DGW", "DIGITAL", "SYNTH", {3, 3, 4},
     {{"WAVE", "WAVE", K_WAVE}, {"SCAN", "SCAN BY ENV", K_BI}, {"STIM", "SCAN TIME", K_DECT}, {"SYNC", "HARD SYNC", K_NUM},
      {"BITS", "WAVE BITS", K_NUM}, {"DTUN", "OSC 2 DETUNE", K_BI}, {"MIX2", "OSC 2 LEVEL", K_NUM}, {"STEP", "PHASE STEPS", K_NUM}}},
    {"VOX-6", "VOX", "VOICE", "VOICE", {1, 2, 5},
     {{"VOWL", "VOWEL", K_VOWL}, {"VSWP", "VOWEL SWEEP", K_BI}, {"VTIM", "SWEEP TIME", K_TIME}, {"SHFT", "FORMANT SHIFT", K_BI},
      {"BRTH", "BREATH", K_NUM}, {"RESO", "RESONANCE", K_NUM}, {"BUZZ", "BUZZ", K_NUM}, {"VIBR", "VIBRATO", K_NUM}}},
    {"FM+PAR", "FM+", "TRIPLE", "SYNTH", {4, 5, 3},
     {{"1FRQ", "RATIO 1", K_FR24}, {"1ENV", "DEPTH / DECAY 1", K_BI}, {"2FRQ", "RATIO 2", K_FR24}, {"2ENV", "DEPTH / DECAY 2", K_BI},
      {"3FRQ", "RATIO 3", K_FR24}, {"3ENV", "DEPTH / DECAY 3", K_BI}, {"TONE", "TONE", K_NUM}, {"TUNE", "TUNE", K_TUN2}}},
};

static const char *const SIDW_N[5] = {"TRI", "SAW", "PULS", "MIX", "NOIS"};
static const char *const SIDM_N[4] = {"OFF", "RING", "SYNC", "R+S"};
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
    if (id >= 0xC0) return &PI_SET[(id & 0x3F) == 16 ? 1 : (id & 0x3F) == 17 ? 2 : 0];
    if (id >= 0x80) return &PI_GLOB[id & 0x7F];
    if (id < 8) return &MI[pat[cur_pat].machine[t]].syn[id];
    if (id < P_LFO) return &PI_COMMON[id - 8];
    if (id == P_WBNK) return &PI_WBNK;
    if (id >= P_MODT) return &PI_MOD[(id - P_MODT) & 3];
    return &PI_LFO[(id - P_LFO) & 7];
}

static int kind_choices(int k)
{
    switch (k) {
    case K_RATIO: case K_WAVE: return 32;
    case K_FR24: return 24;
    case K_SIDW: return 5;
    case K_ONOFF: case K_MSRC: return 2;
    case K_SIDM: case K_ALGO: case K_ASPD: case K_ARNG: return 4;
    case K_SCRN: return 2;
    case K_MODT: return 3;
    case K_PROJ: return M2K_PROJECTS;
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
/* signed, one decimal: -2.0, +0.5 */
static char *s_sdec1(char *d, int v10) { *d++ = v10 < 0 ? '-' : '+'; return s_dec1(d, v10 < 0 ? -v10 : v10); }
static void s_hz(char *out, const char **unit, int hz)
{
    if (hz < 1000) { s_int(out, hz); *unit = "HZ"; }
    else { s_dec1(out, hz / 100); *unit = "KHZ"; }
}
/* an FM ratio in 4096ths as the Monomachine's screen writes it: its own fractions, else 3 decimals under 1, 2 over, cut */
static void s_ratio(char *d, int r, int sq)
{
    static const uint16_t FR[17] = {64, 128, 256, 384, 512, 576, 640, 768, 1024, 1280, 1536, 1792, 2048, 2304, 2560, 3072, 3584};
    static const char *const FN[17] = {"1/64", "1/32", "1/16", "3/32", "1/8", "9/64", "5/32", "3/16", "1/4", "5/16", "3/8",
                                       "7/16", "1/2", "9/16", "5/8", "3/4", "7/8"};
    for (int i = 0; i < 17; i++)
        if (r == FR[i] && (sq ? r <= 1024 || r == 2304 : r != 576)) { s_cpy(d, FN[i]); return; }
    if (r == 0) { s_cpy(d, "0.0"); return; }
    if (r < 4096) {
        int m = r * 1000 / 4096;
        d[0] = '.'; d[1] = (char)('0' + m / 100); d[2] = (char)('0' + m / 10 % 10); d[3] = (char)('0' + m % 10); d[4] = 0;
        return;
    }
    int c = r * 100 / 4096;
    d = s_int(d, c / 100);
    *d++ = '.';
    if (c % 100) { *d++ = (char)('0' + c / 10 % 10); *d++ = (char)('0' + c % 10); } else *d++ = '0';
    *d = 0;
}
/* a pattern's name on the panel: A01 .. A16 */
/* a pattern's name on the panel: the bank's letter and its place, A01 .. D16 */
static char *s_pat(char *d, int n)
{
    int k = n % 16 + 1;
    d[0] = (char)('A' + n / 16); d[1] = (char)('0' + k / 10); d[2] = (char)('0' + k % 10); d[3] = 0;
    return d + 3;
}
static char *s_note(char *d, int n) { d = s_cpy(d, NOTE_N[n % 12]); return s_int(d, n / 12 - 1); }

/* where a filter step sits on the screen's 20 Hz .. 20 kHz axis (0..1016): the filter follows the track's last note */
static int filt_q3(int t, int32_t steps_q8)
{
    int note = trk[t].note ? trk[t].note : 60;
    int32_t pos = (note * 8 * 256 + 12 * (steps_q8 - 16 * 256)) >> 8;
    return CLAMP((pos * 1087 >> 10) - 132, 0, 1016);
}

/* the value as the screen shows it, and its unit */
static void fmt(const pi_t *pi, int t, int v, char *out, const char **unit)
{
    *unit = "";
    int n = kind_choices(pi->kind), c = n ? (v * n) >> 7 : 0;
    switch (pi->kind) {
    case K_BI: s_int(out, v - 64); break;
    case K_FINE: s_int(out, ((v - 64) * 100) / 64); *unit = "CENT"; break;
    case K_TUN2: s_int(out, ((v - 64) * 25) / 8); *unit = "CENT"; break;     /* +-2 semitones, 3.125 c a step */
    case K_PCH: s_cpy(out, T_ENS_PNAME[T_ENS_PIDX[v & 127]]); break;            /* SWAV-ENS's intervals, the just as 5:4 */
    case K_WBNK: s_cpy(out, (v * 2) >> 7 ? "M64" : "M2K"); break;             /* MONO2K's 32, the owner's 64 */
    case K_TIME: case K_HOLD: case K_DECT: case K_ATIM: case K_AHLD: case K_ADEC: case K_AREL: {
        int k = pi->kind;
        if ((k == K_HOLD || k == K_DECT || k == K_AHLD || k == K_ADEC) && v == 127) { s_cpy(out, "INF"); break; }
        uint32_t ms10 = k == K_ATIM ? T_AATK_MS10[v] : k == K_AHLD ? T_AHLD_MS10[v] : (k == K_ADEC || k == K_AREL) ? T_ADEC_MS10[v] : T_ENV_MS10[v];
        if (ms10 < 100) { s_dec1(out, (int)ms10); *unit = "MS"; }
        else if (ms10 < 10000) { s_int(out, (int)(ms10 / 10)); *unit = "MS"; }
        else if (ms10 < 1000000) { s_dec1(out, (int)(ms10 / 1000)); *unit = "S"; }
        else { s_int(out, (int)(ms10 / 10000)); *unit = "S"; }
        break;
    }
    case K_MODT: { static const char *const N[3] = {"CHOR", "FLNG", "PHAS"}; s_cpy(out, N[c < 3 ? c : 2]); break; }
    case K_MHZ: s_dec1(out, T_MOD_HZ10[v]); *unit = "HZ"; break;
    case K_HZ: s_hz(out, unit, T_HZ[v]); break;
    case K_EQHZ: s_hz(out, unit, T_EQHZ[v]); break;
    case K_DBHZ: s_hz(out, unit, T_DBHZ[v]); break;
    case K_SRR: if (!v) { s_cpy(out, "OFF"); break; } s_hz(out, unit, T_SRRHZ[v]); break;
    case K_FOCT: s_sdec1(out, ((v - 16) * 10) / 8); *unit = "OCT"; break;     /* the filter follows the note: 16 is on it */
    case K_FWID: s_sdec1(out, (v * 10) / 8); *unit = "OCT"; break;
    case K_FATK: case K_FDEC: {
        if (pi->kind == K_FDEC && v == 127) { s_cpy(out, "INF"); break; }
        uint32_t ms10 = pi->kind == K_FATK ? T_FATK_MS10[v] : T_FDEC_MS10[v];
        if (ms10 < 100) { s_dec1(out, (int)ms10); *unit = "MS"; }
        else if (ms10 < 10000) { s_int(out, (int)(ms10 / 10)); *unit = "MS"; }
        else { s_dec1(out, (int)(ms10 / 1000)); *unit = "S"; }
        break;
    }
    case K_PAN:
        if (v == 64) s_cpy(out, "C");
        else { char *e = s_cpy(out, v < 64 ? "L" : "R"); s_int(e, v < 64 ? 64 - v : v - 64); }
        break;
    case K_DTIM: {
        if (v == 0) { s_cpy(out, "COMB"); break; }
        /* in 256ths of a note, as the Monomachine counts: simplify */
        int num = v, den = 256;
        while (!(num & 1) && den > 1) { num >>= 1; den >>= 1; }
        char *e = s_int(out, num);
        if (den > 1) { *e++ = '/'; s_int(e, den); }
        *unit = "NOTE";
        break;
    }
    case K_BITS: s_int(out, 16 - (v * 14 >> 7)); *unit = "BIT"; break;
    case K_SEMI: s_int(out, v - 64); *unit = "SEMI"; break;
    case K_RATIO: s_cpy(out, T_RATIO_NAME[c]); *unit = "RATIO"; break;
    case K_FR24: s_cpy(out, T_FMR24_NAME[c]); *unit = "RATIO"; break;   /* FM+'s 24 steps */
    case K_FR64: s_ratio(out, v == 127 ? 8192 : v * 64, 0); *unit = "RATIO"; break;          /* FM+DYN's: v / 64, 2 at 127 */
    case K_FRSQ: s_ratio(out, v == 127 ? 16384 : v * v, 1); *unit = "RATIO"; break;          /* (v / 64)^2, 4 at 127 */
    case K_SEMI24: s_int(out, ((v - 64) * 24) / 64); *unit = "SEMI"; break;
    case K_INT25: { char *e = s_cpy(out, "+"); s_int(e, c); *unit = "SEMI"; break; }
    case K_SIDW: s_cpy(out, SIDW_N[c]); break;
    case K_SIDM: s_cpy(out, SIDM_N[c]); break;
    case K_ONOFF: s_cpy(out, c ? "ON" : "OFF"); break;
    case K_MSRC: s_cpy(out, c ? "PRCH" : "MFRQ"); *unit = c ? "PREV TRACK" : ""; break;
    case K_MFRQ: s_note(out, 48 + (v * 3 + 4) / 8); *unit = "NOTE"; break;     /* C3 at 0, an octave every 32 */
    case K_ALGO: s_cpy(out, ALGO_N[c]); break;
    case K_VOIC: s_int(out, c + 1); *unit = "VOICES"; break;
    case K_WAVE: s_cpy(out, (pat[cur_pat].kit[t][P_WBNK] * 2) >> 7 ? T_WAVE64_NAME[v >> 1 > 63 ? 63 : v >> 1] : T_WAVE_NAME[c]); break;
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
    case K_GLCH: {                     /* a sixteenth's chance of a repeat, its law's: 0.3 % .. 50 % */
        int c10 = (int)((T_GLCH_P[v & 127] * 1000u + 32768u) >> 16);
        if (c10 < 100) s_dec1(out, c10); else s_int(out, (c10 + 5) / 10);
        *unit = "%";
        break;
    }
    case K_GSIZ: s_cpy(out, GSIZ_N[(v * 5) >> 7]); *unit = "NOTE"; break;
    case K_SCRN: s_cpy(out, c ? "DARK" : "LIGHT"); *unit = "DISPLAY"; break;
    case K_PROJ: { char *e = s_cpy(out, "P"); s_int(e, c + 1); *unit = m2k_project_name(c); break; }
    case K_LOAD: s_cpy(out, "TURN"); *unit = "TWICE TO LOAD"; break;
    default: s_int(out, v); break;
    }
}

/* -------------------------------------------------------------------------------------------- the pages */

enum { PG_HOME, PG_SYN, PG_AMP, PG_FILT, PG_FX, PG_LFO, PG_ARP, PG_GLO, PG_COUNT };
static const uint8_t NSUB[PG_COUNT] = {1, 2, 2, 2, 4, 6, 1, 4};
static int nsub(int page);                     /* SYN has a third page, the bank, where the machine plays waves */
#define SETTING(i) (0xC0 | (i))
#define GL(g) (0x80 | (g))
#define NONE 0xFF

static int knob_param(int page, int sub, int k)
{
    switch (page) {
    case PG_HOME: { static const uint8_t m[4] = {P_LEVL, P_PTCH, GL(G_TMPO), GL(G_SWNG)}; return m[k]; }
    case PG_SYN: return sub < 2 ? P_SYN + sub * 4 + k : k == 0 ? P_WBNK : NONE;
    case PG_AMP: return P_ATK + sub * 4 + k;
    case PG_FILT: return P_BASE + sub * 4 + k;
    case PG_FX: return sub < 3 ? P_EQF + sub * 4 + k : P_MODT + k;
    case PG_LFO: return P_LFO + (sub >> 1) * 8 + (sub & 1) * 4 + k;
    case PG_ARP: return P_AMOD + k;
    case PG_GLO: return sub < 3 ? GL(sub * 4 + k) : k == 0 ? SETTING(S_SCREEN) : k == 1 ? SETTING(16) : k == 2 ? SETTING(17) : NONE;
    default: return NONE;
    }
}

static void page_label(int page, int sub, char *o)
{
    static const char *const N[PG_COUNT] = {"HOME", "SYN", "AMP", "FILT", "FX", "LFO", "ARP", "GLO"};
    switch (page) {
    case PG_FX: s_cpy(o, sub == 0 ? "FX" : sub == 1 ? "DELAY" : sub == 2 ? "DSTR" : "MOD"); break;
    case PG_LFO: { char *e = s_cpy(o, "LFO"); e = s_int(e, (sub >> 1) + 1); s_cpy(e, (sub & 1) ? "B" : "A"); break; }
    case PG_GLO: s_cpy(o, sub == 0 ? "TIME" : sub == 1 ? "GLITCH" : sub == 2 ? "VERB" : "SET"); break;
    case PG_SYN: s_cpy(o, sub == 2 ? "BANK" : "SYN"); break;
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
    uint32_t rec_t0, save_t0;
    uint8_t save_fired, proj_sel;
    uint8_t pkey, pfired, cursor;      /* a page key held in GRID (+1), its hold done; step entry's step */
    uint32_t pkey_t0;
    uint32_t load_t;
    /* the focus */
    uint8_t focus;
    uint32_t focus_t;
    char roll_old[12];
    uint32_t roll_t;
    int32_t ease[4];                   /* Q8 */
    /* animations */
    uint32_t trans_t, glitch_t, card_t, save_t, toast_t;
    uint16_t trans_len;                /* ms; the boot's is longer than a byte */
    uint8_t glitch_amt, in_trans, trans_down;
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
    m_cpy(fb_prev, fb_out, sizeof fb_prev);
    ui.trans_t = ui.now;
    ui.trans_len = (uint16_t)len;
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

static int nsub(int page) { return page == PG_SYN && pat[cur_pat].machine[ui.trk] == M_WAV ? 3 : NSUB[page]; }

static void toast(const char *a, const char *b)
{
    s_cpy(ui.toast, a);
    s_cpy(ui.toast2, b ? b : "");
    ui.toast_t = ui.now | 1u;
}

static void go(int page)
{
    if (page == ui.page) ui.sub = (uint8_t)((ui.sub + 1) % nsub(page));
    else { ui.page = (uint8_t)page; ui.sub = 0; }
    ui.focus_t = 0;
    start_trans(250);
}

/* the value a knob shows: a held step's lock, or the kit */
static int knob_value(int code)
{
    const m2k_pattern_t *p = &pat[cur_pat];
    if (code == SETTING(16)) return enc(ui.proj_sel, M2K_PROJECTS);
    if (code == SETTING(17)) return 0;
    if (code >= 0xC0) return store_.set[code & 0x3F];
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
    if (code == SETTING(16)) {
        ui.proj_sel = (uint8_t)((nv * M2K_PROJECTS) >> 7);
        ui.load_t = 0;
    } else if (code == SETTING(17)) {
        if (ui.load_t && ui.now - ui.load_t < 2000) {          /* the second turn loads */
            ui.load_t = 0;
            project_load(ui.proj_sel);
            char o[16] = {'P', (char)('1' + ui.proj_sel), ' ', 0};
            s_cpy(o + 3, m2k_project_name(ui.proj_sel));
            toast("LOADED", o);
            start_glitch(4);
            dirty = 1;
        } else {
            ui.load_t = ui.now | 1u;
            toast("TURN AGAIN", "TO LOAD");
        }
    } else if (code >= 0xC0) {
        store_.set[code & 0x3F] = (uint8_t)nv;
    } else if (code >= 0x80) {
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

/* a page key in GRID: the page, or why it is empty; tapped again, the view follows the playhead */
static void page_key(int pg)
{
    char a[16], o[24], *e;
    e = s_cpy(a, "PAGE "); s_int(e, pg + 1);
    if (pg * 16 >= seq_len()) {
        e = s_cpy(o, "PAST THE END, HOLD: ");
        s_int(e, (pg + 1) * 16);
        toast(a, o);
        return;
    }
    if (pg == ui.spage && !ui.follow) { ui.follow = 1; toast(a, "FOLLOWS THE PLAY"); return; }
    ui.spage = (uint8_t)pg;
    ui.follow = 0;
    e = s_cpy(o, "STEPS "); e = s_int(e, pg * 16 + 1); e = s_cpy(e, "-"); s_int(e, pg * 16 + 16);
    toast(a, o);
}

/* a page key held: the pattern's length ends with that page; new steps take the pattern so far, so it goes on */
static void page_length(int pg)
{
    m2k_pattern_t *p = &pat[cur_pat];
    int old = seq_len(), len = (pg + 1) * 16;
    if (len > old) {
        for (int t = 0; t < M2K_TRACKS; t++)
            for (int s = old; s < len; s++) { p->step[t][s] = p->step[t][s % old]; lock_clear_step(t, s); }
        for (int i = 0; i < M2K_LOCKS; i++) {
            m2k_lock_t l = p->lock[i];
            if (l.trk >= M2K_TRACKS || l.step >= old) continue;
            for (int s = l.step + old; s < len; s += old) lock_set(l.trk, s, l.param, l.val);
        }
    }
    p->glob[G_LEN] = (uint8_t)len;
    ui.spage = (uint8_t)pg;
    ui.follow = 0;
    char o[16], *e = s_cpy(o, "LENGTH ");
    s_int(e, len);
    toast(o, len > old ? "THE STEPS GO ON" : "STEPS PAST IT KEPT");
}

/* step entry: REC armed while stopped, the keys playing: each note is the next step */
static int step_entry(void) { return sq.rec && !sq.playing && !ui.grid; }

static void entry_show(const char *what)
{
    char o[16], *e = s_cpy(o, "STEP ");
    s_int(e, ui.cursor + 1);
    toast(o, what);
    ui.spage = (uint8_t)(ui.cursor / 16);
    ui.follow = 0;
}

static void entry_write(int note)
{
    m2k_step_t *st = &pat[cur_pat].step[ui.trk][ui.cursor];
    st->flags = ST_TRIG;
    st->note = (uint8_t)note;
    st->cond = C_ALWAYS;
    st->rtrg = 0;
    lock_clear_step(ui.trk, ui.cursor);
    char n[8];
    s_note(n, note);
    entry_show(n);
    ui.cursor = (uint8_t)((ui.cursor + 1) % seq_len());
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
        if (down) {
            live_on(note, 100);
            ui.note[ui.trk] = (uint8_t)note;
            if (step_entry()) entry_write(note);
        } else live_off(note);
        return;
    }
    if (b >= 0) {
        if (b >= 6 && b < 10) {        /* P1..P4: a tap shows the page, a hold makes the pattern end there */
            if (down) { page_key(b - 6); ui.pkey = (uint8_t)(b - 5); ui.pkey_t0 = ui.now | 1u; ui.pfired = 0; }
            else if (ui.pkey == b - 5) ui.pkey = 0;
            return;
        }
        if (!down) return;
        if (b < M2K_TRACKS) { if (b != ui.trk) { ui.trk = (uint8_t)b; sq.ktrk = ui.trk; start_trans_dir(190, 1); } }
        else {
            p->mute ^= (uint8_t)(1u << ui.trk);
            char o[4] = {'T', (char)('1' + ui.trk), 0, 0};
            toast((p->mute >> ui.trk) & 1 ? "MUTED" : "UNMUTED", o);
        }
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

static void glo_combo(int b);

/* the clipboard: a whole pattern (from HOME) or one track's sound (from a track's pages) */
static m2k_pattern_t clip_pat POOL;
static struct { uint8_t kind, machine, kit[P_COUNT]; } clipb;

static void glo_combo(int b)
{
    m2k_pattern_t *p = &pat[cur_pat];
    int whole = ui.page == PG_HOME || ui.page == PG_GLO;
    char a[24];
    s_pat(a, cur_pat);
    switch (b) {
    case B_REC:                                        /* copy */
        if (whole) { m_cpy(&clip_pat, p, sizeof clip_pat); clipb.kind = 1; toast("COPY", a); }
        else {
            clipb.kind = 2;
            clipb.machine = p->machine[ui.trk];
            m_cpy(clipb.kit, p->kit[ui.trk], P_COUNT);
            char o[12] = {'T', (char)('1' + ui.trk), ' ', 0};       /* "T6 SOUND": 9 bytes */
            s_cpy(o + 3, "SOUND");
            toast("COPY", o);
        }
        break;
    case B_PLAY:                                       /* paste */
        if (clipb.kind == 1 && whole) {
            m_cpy(p, &clip_pat, sizeof *p);
            work_reset_current();
            toast("PASTE", a);
        } else if (clipb.kind == 2 && !whole) {
            p->machine[ui.trk] = clipb.machine;
            m_cpy(p->kit[ui.trk], clipb.kit, P_COUNT);
            m_cpy(trk[ui.trk].eff, clipb.kit, P_COUNT);
            char o[12] = {'T', (char)('1' + ui.trk), ' ', 0};       /* "T6 SOUND": 9 bytes */
            s_cpy(o + 3, "SOUND");
            toast("PASTE", o);
            ui.card_t = ui.now | 1u;
        } else {
            toast("NOTHING", whole ? "COPY A PATTERN FIRST" : "COPY A SOUND FIRST");
        }
        start_glitch(2);
        break;
    case B_SEQ:                                        /* clear: the pattern's trigs, or the track's */
        if (whole) {
            for (int t = 0; t < M2K_TRACKS; t++)
                for (int s = 0; s < M2K_STEPS; s++) { p->step[t][s].flags = 0; p->step[t][s].cond = 0; p->step[t][s].rtrg = 0; }
            for (int i = 0; i < M2K_LOCKS; i++) p->lock[i].trk = 0xFF;
            toast("CLEAR", a);
        } else {
            for (int s = 0; s < M2K_STEPS; s++) { p->step[ui.trk][s].flags = 0; p->step[ui.trk][s].cond = 0; p->step[ui.trk][s].rtrg = 0; }
            for (int i = 0; i < M2K_LOCKS; i++) if (p->lock[i].trk == ui.trk) p->lock[i].trk = 0xFF;
            char o[8] = {'T', (char)('1' + ui.trk), 0};
            toast("CLEAR", o);
        }
        lockmap_build();
        break;
    case B_SAVE:                                       /* reload: back to the saved pattern */
        if (pattern_reload()) toast("RELOAD", a);
        start_glitch(3);
        break;
    default:
        break;
    }
}

void m2k_button(int b, int down)
{
    if (b < 0 || b >= B_COUNT) return;
    int was = ui.btn[b];
    ui.btn[b] = (uint8_t)down;
    /* GLO held: the editing functions (Elektron's FUNC): REC copy, PLAY paste, SEQ clear, SAVE reload */
    if (b != B_GLO && ui.btn[B_GLO] && (b == B_REC || b == B_PLAY || b == B_SEQ || b == B_SAVE)) {
        if (down) { glo_combo(b); ui.glo_used = 1; }
        return;
    }
    if (b != B_GLO && !down && !was) return;
    if (b == B_SAVE) {
        if (down) { ui.save_t0 = ui.now | 1u; ui.save_fired = 0; return; }
        if (!ui.save_t0) return;
        ui.save_t0 = 0;
        if (ui.save_fired) return;            /* the hold already saved the project */
        char a[24], *e = s_pat(a, cur_pat);
        if (pattern_save()) { *e = 0; toast("SAVED", a); dirty = 1; ui.save_t = ui.now | 1u; }
        else toast("PROJECT FULL", "PATTERN NOT SAVED");
        return;
    }
    if (b == B_REC) {                  /* a tap arms recording; held, it is FILL */
        if (down) { ui.rec_t0 = ui.now | 1u; return; }
        if (ui.now - ui.rec_t0 < 300) {
            sq.rec ^= 1;
            if (sq.rec && !sq.playing) {   /* stopped: step entry from the page in view */
                ui.cursor = (uint8_t)(ui.spage * 16 < seq_len() ? ui.spage * 16 : 0);
                if (ui.grid) toast("REC", "SEQ: KEYS WRITE STEPS");
                else toast("STEP ENTRY", "KEYS WRITE STEPS");
            }
        }
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

    case B_SEQ:
        ui.grid ^= 1;
        ui.nheld = 0;
        if (ui.spage * 16 >= seq_len()) ui.spage = 0;
        toast(ui.grid ? "GRID" : "KEYS", ui.grid ? "STEPS ON THE KEYS" : "PLAY THE TRACK");
        start_trans(250);
        break;
    case B_PLAY: seq_play(!sq.playing); break;
    case B_OCTD: case B_OCTU: {
        int d = b == B_OCTU ? 1 : -1;
        if (ui.btn[B_GLO] && step_entry()) {   /* GLO + OCT+ leaves a rest, GLO + OCT- goes back a step */
            ui.glo_used = 1;
            if (d > 0) {
                pat[cur_pat].step[ui.trk][ui.cursor].flags = 0;
                lock_clear_step(ui.trk, ui.cursor);
                ui.cursor = (uint8_t)((ui.cursor + 1) % seq_len());
                entry_show("REST");
            } else {
                ui.cursor = (uint8_t)((ui.cursor + seq_len() - 1) % seq_len());
                entry_show("BACK");
            }
            break;
        }
        if (ui.grid && ui.nheld) held_step_edit(0, d);
        else if (ui.grid) { ui.spage = (uint8_t)CLAMP(ui.spage + d, 0, 3); ui.follow = 0; }
        else { ui.oct = (int8_t)CLAMP(ui.oct + d, -3, 3); char o[8]; char *e = s_cpy(o, "OCT "); if (ui.oct > 0) *e++ = '+'; s_int(e, ui.oct); toast(o, 0); }
        break;
    }
    default: break;
    }
}

/* a pattern: at once when stopped, at the end of the bar when playing */
static void select_pattern(int n)
{
    n = (n % M2K_PATTERNS + M2K_PATTERNS) % M2K_PATTERNS;
    char a[24], *e = s_pat(a, n);
    if (sq.playing) {
        sq.queued = (uint8_t)n;
        *e++ = ' ';
        s_cpy(e, pat[n].name);
        toast(a, n == cur_pat ? "" : "QUEUED");
        return;
    }
    cur_pat = n;
    sq.queued = (uint8_t)n;
    if (ui.spage * 16 >= seq_len()) ui.spage = 0;     /* a page this pattern does not reach shows nothing: its first */
    ui.follow = 1;
    lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[n].kit[t], P_COUNT);
    toast(a, pat[n].name);
    start_glitch(4);
}

void m2k_pattern(int n) { select_pattern(n); }

/* the web page's own controls: what the panel does, without the panel's ways */
void m2k_track(int t)
{
    if (t < 0 || t >= M2K_TRACKS || t == ui.trk) return;
    for (int i = 0; i < 8 && sq.nheld; i++) live_off(sq.held[0]);
    ui.trk = (uint8_t)t;
    sq.ktrk = ui.trk;
    start_trans_dir(190, 1);
}

void m2k_mute(int t)
{
    if (t < 0 || t >= M2K_TRACKS) return;
    pat[cur_pat].mute ^= (uint8_t)(1u << t);
    toast((pat[cur_pat].mute >> t) & 1 ? "MUTED" : "UNMUTED", (const char[]){'T', (char)('1' + t), 0});
}

void m2k_tempo(int d)
{
    const pi_t *pi = pinfo(ui.trk, GL(G_TMPO));
    uint8_t *g = &pat[cur_pat].glob[G_TMPO];
    *g = (uint8_t)CLAMP(*g + d, kind_min(pi->kind), kind_max(pi->kind));
    char o[12];
    s_cpy(s_int(o, (int)seq_bpm()), " BPM");
    toast("TEMPO", o);
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
        start_trans_dir(190, 1);
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
        set_machine(M_ORDER[(m_place(p->machine[ui.trk]) + M_COUNT + (d > 0 ? 1 : -1)) % M_COUNT]);
        break;
    case E_PRESET: {
        if (ui.grid && ui.nheld) { held_step_edit(d > 0 ? 1 : -1, 0); break; }
        select_pattern(((sq.playing ? sq.queued : cur_pat) + M2K_PATTERNS + (d > 0 ? 1 : -1)) % M2K_PATTERNS);
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
    char pa[8];
    s_pat(pa, cur_pat);
    xr -= text_r(&F_S, xr, 2, pa, 1, 1) + 5;
    if (ui.grid) { int w = text_w(&F_S, "GRID", 1) + 6; tag(xr - w, 0, "GRID", 0); xr -= w + 4; }
    if (sq.fill) { int w = text_w(&F_S, "FILL", 1) + 6; tag(xr - w, 0, "FILL", 1); xr -= w + 4; }
    /* left: the page, the track, its machine; what does not fit is left out */
    char o[24];
    page_label(ui.page, ui.sub, o);
    int x = tag(0, 0, o, 1) + 4;
    /* where in the section: a square a page, this one solid, on the type's middle line */
    if (nsub(ui.page) > 1) {
        for (int i = 0; i < nsub(ui.page); i++) {
            if (i == ui.sub) rect(x + i * 6, 4, 4, 6, 1);
            else frame(x + i * 6, 4, 4, 6, 1);
        }
        x += nsub(ui.page) * 6 + 4;
    }
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
    /* one size for every big value, centred between the label (above) and the unit (below); the odd word
     * too wide for the column drops to the large size, on the same centre */
    const m2k_font_t *f = &F_V;
    if (text_w(f, s, 0) > w) f = &F_L;
    int yy = y + 33 - f->h / 2;
    (void)roll;
    text_grad(yy, f->h, CLAMP(density, 4, 60) - 4, 64);
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
        if (M_ORDER[i] == m) disc(x, y, 3, 1);
        else { frame(x - 3, y - 3, 7, 7, 1); px(x - 3, y - 3, 0); px(x + 3, y - 3, 0); px(x - 3, y + 3, 0); px(x + 3, y + 3, 0); }
    }
}

/* the filter's response at column i of w: high and low pass as q3 cutoffs (0..1016), their resonances 0..127 */
static int filt_y(int i, int w, int hp, int lp, int hq, int lq, int top, int yb)
{
    int q = i * 1016 / (w - 1), g = 256;
    if (q < hp) g = CLAMP(256 - (hp - q) * 3, 0, 256);
    if (q > lp) g = CLAMP(256 - (q - lp) * 3, 0, g);
    int dh = q - hp, dl = q - lp;
    if (dh > -40 && dh < 40) g += (hq * (40 - (dh < 0 ? -dh : dh))) >> 4;
    if (dl > -40 && dl < 40) g += (lq * (40 - (dl < 0 ? -dl : dl))) >> 4;
    int y = yb - ((yb - top) * g >> 8);
    return y < top - 12 ? top - 12 : y;
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
            /* the track in the corner, the machine's name under its mark: tracked if it fits the column, tight if not */
            char tn[3] = {'T', (char)('1' + ui.trk), 0};
            tag(X - 2, Y - 1, tn, 1);
            const char *nm = MI[m].name;
            text_c(&F_S, X + W / 2, Y + 68, nm, 1, text_w(&F_S, nm, 1) <= W - 2 ? 1 : 0);
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
        int fk = ui.focus_t && ui.now - ui.focus_t < 2500 ? ui.focus : -1;   /* the knob being turned */
        if (ui.sub == 1) {
            /* distortion as a transfer curve, the volume as its height and a bar, the pan on a line, the glide
             * as a ramp between two notes; the part being turned is drawn heavy and its tag inverted */
            int64_t dh = T_DIST_H[k[P_DIST]];
            int32_t dc = T_DIST_C[k[P_DIST]], vol = T_VOL[k[P_VOL]];
            int bx = X + 2, by = Y + 2, bs = 46, mid = by + bs / 2;
            frame(bx, by, bs, bs, 1);
            for (int j = 4; j < bs; j += 4) px(bx + j, by + bs - 1 - j, 1);           /* the clean line */
            int prev = mid;
            for (int i = 0; i < bs - 2; i++) {
                int32_t in = (i - (bs - 2) / 2) * 65535 / (bs - 2);
                int32_t c = (int32_t)CLAMP((in * dh) >> 24, -dc, dc);                  /* the headroom and the clip */
                int32_t o = ((c << 7) / (T_DIST_C[64] >> 8)) * (vol >> 3) >> 12;    /* Q15, 32-bit division only */
                int y = mid - (int)((o * (bs / 2 - 2)) >> 15);
                if (i) { line(bx + i, prev, bx + 1 + i, y, 1); if (fk == 0) line(bx + i, prev + 1, bx + 1 + i, y + 1, 1); }
                prev = y;
            }
            int vx = X + 54, vw = W - 56, vh = (bs - 2) * vol >> 15;
            frame(vx, by, vw, bs, 1);
            rect_grad(vx + 1, by + bs - 1 - vh, vw - 2, vh, fk == 1 ? 40 : 14, 64, 1);
            hline(vx - 2, by + bs - 1 - vh, vw + 4, 1);
            tag(bx, by + bs + 3, "DS", fk == 0);
            tag(vx, by + bs + 3, "VL", fk == 1);
            int lx = X + 26, lw = W - 26, pan = k[P_PAN];
            int py = Y + 75;
            tag(X, py - 7, "PN", fk == 2);
            hline(lx, py, lw, 1);
            vline(lx + lw / 2, py - 3, 7, 1);
            vline(lx, py - 2, 5, 1); vline(lx + lw - 1, py - 2, 5, 1);
            int mx = lx + 2 + pan * (lw - 5) / 127;
            if (fk == 2) rect(mx - 2, py - 5, 5, 11, 1); else { rect(mx - 2, py - 5, 5, 11, 0); frame(mx - 2, py - 5, 5, 11, 1); }
            int gy = Y + 98, gl = 2 + k[P_PORT] * (lw - 10) / 127;
            tag(X, gy - 12, "GL", fk == 3);
            hline(lx, gy, 4, 1);
            line(lx + 4, gy, lx + 4 + gl, gy - 9, 1);
            if (fk == 3) line(lx + 5, gy, lx + 5 + gl, gy - 9, 1);
            hline(lx + 4 + gl, gy - 9, lw - 4 - gl, 1);
            break;
        }
        /* attack, hold, decay, release as lengths on a log scale. The segment being turned fills dense, the others
         * light; handles mark where each ends; the letters sit under their segments, and the live voice runs on it */
        int wa = 6 + k[P_ATK] * 24 / 127, wh = k[P_HOLD] == 127 ? 18 : 2 + k[P_HOLD] * 18 / 127;
        int wd = k[P_DEC] == 127 ? 18 : 12 + k[P_DEC] * 30 / 127, wr = 8 + k[P_REL] * 24 / 127;
        int tot = wa + wh + wd + wr;
        wa = wa * (W - 6) / tot; wh = wh * (W - 6) / tot; wd = wd * (W - 6) / tot; wr = (W - 6) - wa - wh - wd;
        int x0 = X + 3, yb = Y + 84, top = Y + 10, hgt = yb - top;
        int xs[5] = {x0, x0 + wa, x0 + wa + wh, x0 + wa + wh + wd, x0 + wa + wh + wd + wr};
        int ys[5], py = yb;
        for (int x = xs[0]; x <= xs[4] && x < X + W; x++) {
            int y, seg = x <= xs[1] ? 0 : x <= xs[2] ? 1 : x <= xs[3] ? 2 : 3;
            if (seg == 0) y = yb - hgt * (x - xs[0]) / (wa ? wa : 1);
            else if (seg == 1) y = top;
            else if (seg == 2) { int q = (x - xs[2]) * 256 / (wd ? wd : 1); int l = 256 - q; l = k[P_DEC] == 127 ? 256 : (l * l) >> 8; y = yb - (hgt * l >> 8); }
            else { int q = (x - xs[3]) * 256 / (wr ? wr : 1); int l0 = k[P_DEC] == 127 ? 256 : 40; int l = (l0 * (256 - q)) >> 8; y = yb - (hgt * l >> 8); }
            int dense = seg == fk ? 64 : fk < 0 ? 44 : 22;
            for (int j = y; j <= yb; j++) if (T_BAYER[j & 7][x & 7] < dense - (j - y) * (dense - 4) / (hgt + 1)) px(x, j, 1);
            line(x - 1, py, x, y, 1);
            if (seg == fk) line(x - 1, py - 1, x, y - 1, 1);
            for (int b = 1; b < 5; b++) if (x == xs[b]) ys[b] = y;
            py = y;
        }
        ys[4] = yb;
        for (int b = 1; b < 4; b++) dotted_v(xs[b], top - 6, ys[b] - top + 6);
        for (int b = 1; b < 5; b++) {
            if (b - 1 == fk) { rect(xs[b] - 3, ys[b] - 3, 7, 7, 1); continue; }
            rect(xs[b] - 2, ys[b] - 2, 5, 5, 0);
            frame(xs[b] - 2, ys[b] - 2, 5, 5, 1);
        }
        hline(X, yb + 1, W, 1);
        static const char *const L[4] = {"A", "H", "D", "R"};
        int lx = -100;
        for (int i = 0; i < 4; i++) {          /* under its segment; a letter that would touch the last is left out */
            int c = (xs[i] + xs[i + 1]) / 2, w = text_w(&F_S, L[i], 0);
            if (i == fk) { rect(c - w / 2 - 2, yb + 3, w + 5, F_S.h + 2, 1); text(&F_S, c - w / 2, yb + 4, L[i], 0, 0); lx = c + w / 2 + 3; }
            else if (c - w / 2 > lx + 2 && (fk < 0 || fk != i + 1 || c + w / 2 + 6 < (xs[fk] + xs[fk + 1]) / 2 - 4)) { text(&F_S, c - w / 2, yb + 4, L[i], 1, 0); lx = c + w / 2; }
        }
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
        int fk = ui.focus_t && ui.now - ui.focus_t < 2500 ? ui.focus : -1;
        int yb = Y + 80, top = Y + 14;
        int db = k[P_BOF] - 64, dw = k[P_WOF] - 64, ob = 7 * db * (db < 0 ? -db : db), ow = 7 * dw * (dw < 0 ? -dw : dw);
        int hp0 = filt_q3(ui.trk, k[P_BASE] << 8), lp0 = filt_q3(ui.trk, (k[P_BASE] + k[P_WDTH]) << 8);
        if (ui.sub == 1) {
            /* the filter envelope: the band at rest in dots, where the envelope takes it solid, and the envelope's
             * attack and decay under it */
            int hp1 = filt_q3(ui.trk, (k[P_BASE] << 8) + ob);
            int lp1 = filt_q3(ui.trk, ((k[P_BASE] + k[P_WDTH]) << 8) + ob + ow);
            int yb2 = Y + 70, prev0 = yb2, prev1 = yb2;
            for (int i = 0; i < W; i++) {
                int y0 = filt_y(i, W, hp0, lp0, k[P_HPQ], k[P_LPQ], top, yb2), y1 = filt_y(i, W, hp1, lp1, k[P_HPQ], k[P_LPQ], top, yb2);
                int dense = fk >= 2 ? 52 : 34;
                for (int j = y1 + 1; j <= yb2; j++) if (T_BAYER[j & 7][(X + i) & 7] < dense - (j - y1) * (dense - 4) / (yb2 - top + 1)) px(X + i, j, 1);
                if (i) line(X + i - 1, prev1, X + i, y1, 1);
                if (i && ((i >> 1) & 1)) line(X + i - 1, prev0, X + i, y0, 2);        /* the band at rest, dashed */
                prev0 = y0; prev1 = y1;
            }
            hline(X, yb2 + 1, W, 1);
            /* how far the envelope takes the base: an arrow over the band */
            int xh0 = X + hp0 * (W - 1) / 1016, xh1 = X + hp1 * (W - 1) / 1016, ay = top - 8;
            if (xh1 != xh0) {
                hline(xh0 < xh1 ? xh0 : xh1, ay, (xh1 > xh0 ? xh1 - xh0 : xh0 - xh1) + 1, 1);
                if (fk == 2) hline(xh0 < xh1 ? xh0 : xh1, ay + 1, (xh1 > xh0 ? xh1 - xh0 : xh0 - xh1) + 1, 1);
                vline(xh0, ay - 2, 5, 1);
                int dir = xh1 > xh0 ? -1 : 1;
                for (int j = 1; j <= 3; j++) vline(xh1 + dir * j, ay - j, 2 * j + 1, 1);
            }
            tag(X, yb2 + 4, "BO", fk == 2);
            tag(X, yb2 + 19, "WO", fk == 3);
            /* the envelope: attack up, decay down */
            int ex = X + 30, ew = W - 30, eb = yb2 + 31, et = yb2 + 7;
            int a = 2 + k[P_FATK] * (ew / 3 - 2) / 127, d = 3 + k[P_FDEC] * (ew - a - 4) / 127;
            for (int x = 0; x <= a + d; x++) {
                int y = x <= a ? eb - (eb - et) * x / (a ? a : 1) : eb - ((eb - et) * (((256 - (x - a) * 256 / d) * (256 - (x - a) * 256 / d)) >> 8) >> 8);
                int seg = x <= a ? 0 : 1;
                for (int j = y; j <= eb; j++) if (T_BAYER[j & 7][(ex + x) & 7] < (seg == fk ? 56 : 20)) px(ex + x, j, 1);
                px(ex + x, y, 1);
                if (seg == fk) px(ex + x, y - 1, 1);
            }
            hline(ex, eb + 1, ew, 1);
            break;
        }
        /* the band: high and low pass as a base and a width, their resonances as peaks. Handles stand on each
         * cutoff, the width is a bracket under the line; what is being turned is heavy */
        int hp = hp0, lp = lp0;
        if (t->sounding) { hp = mon.cut_hp[ui.trk]; lp = mon.cut_lp[ui.trk]; }
        int prev = yb;
        for (int i = 0; i < W; i++) {
            int y = filt_y(i, W, hp, lp, k[P_HPQ], k[P_LPQ], top, yb);
            for (int j = y + 1; j <= yb; j++) if (T_BAYER[j & 7][(X + i) & 7] < 48 - (j - y) * 44 / (yb - top + 1)) px(X + i, j, 1);
            line(X + i - 1, prev, X + i, y, 1);
            prev = y;
        }
        hline(X, yb + 1, W, 1);
        int xh = X + hp * (W - 1) / 1016, xl = X + lp * (W - 1) / 1016;
        int yh = filt_y(xh - X, W, hp, lp, k[P_HPQ], k[P_LPQ], top, yb), yl = filt_y(xl - X, W, hp, lp, k[P_HPQ], k[P_LPQ], top, yb);
        dotted_v(xh, top - 8, yb - top + 8);
        dotted_v(xl, top - 8, yb - top + 8);
        int hot_h = fk == 0 || fk == 2, hot_l = fk == 3;
        if (hot_h) rect(xh - 3, yh - 3, 7, 7, 1); else { rect(xh - 2, yh - 2, 5, 5, 0); frame(xh - 2, yh - 2, 5, 5, 1); }
        if (hot_l) rect(xl - 3, yl - 3, 7, 7, 1); else { rect(xl - 2, yl - 2, 5, 5, 0); frame(xl - 2, yl - 2, 5, 5, 1); }
        /* the width */
        int wy = yb + 4;
        vline(xh, wy, 5, 1); vline(xl, wy, 5, 1);
        if (fk == 1) rect(xh, wy + 1, xl - xh + 1, 3, 1); else dotted_h(xh, wy + 2, xl - xh + 1);
        /* the names, under their cutoffs and inside the column, apart */
        int wH = text_w(&F_S, "HP", 1) + 6, wL = text_w(&F_S, "LP", 1) + 6;
        int th = CLAMP(xh - wH / 2, X, X + W - wH - wL - 2), tl = CLAMP(xl - wL / 2, th + wH + 2, X + W - wL);
        tag(th, wy + 7, "HP", hot_h);
        tag(tl, wy + 7, "LP", hot_l);
        break;
    }
    case PG_FX: {
        if (ui.sub == 1) {             /* echoes: the width is half a bar; each lights when it sounds */
            int dt = k[P_DTIM] ? k[P_DTIM] : 1;
            int fb = k[P_DFB];                           /* 64 is unity */
            int lvl = 256;
            uint32_t ms_step = 15000u / seq_bpm();       /* a sixteenth in ms */
            uint32_t since = ui.now - ui.flash_t[ui.trk];
            for (int e = 0; e < 12; e++) {
                int x = X + 6 + (dt * (e + 1) * (W - 8)) / 128;
                if (x >= X + W) break;
                int h = (80 * lvl) >> 8;
                uint32_t at = (uint32_t)dt * (uint32_t)(e + 1) * ms_step / 16u;
                int lit = since >= at && since < at + 90;
                rect_pat(x, Y + 88 - h, 4, h, lit ? PT_SOLID : PT_CHECK, 0);
                lvl = CLAMP((lvl * fb) >> 6, 0, 256);
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
                if (bits < 16) s = (s >> (16 - bits)) * (1 << (16 - bits));   /* a multiply: no left shift of a negative */
                acc += step;
                if (acc >= 65536 || i == 0) { acc -= 65536; hold = s; }
                if (srr) s = hold;
                int y = Y + 46 - (s * 40 >> 15);
                if (i) line(X + i - 1, prev, X + i, y, 1);
                prev = y;
            }
            dotted_h(X, Y + 46, W);
        } else if (ui.sub == 3) {      /* the modulation: its sweep at its rate, as deep as DPTH, and how much of it */
            int type = (k[P_MODT] * 3) >> 7, dep = k[P_MODD], mix = k[P_MODM];
            uint32_t ph = (uint32_t)ui.now * (T_MOD_INC[k[P_MODR]] * 44u);
            int mid = Y + 34, amp = 3 + dep * 28 / 127, prev = mid;
            for (int i = 0; i < W; i++) {
                int32_t tri = (int32_t)((ph + (uint32_t)i * (0xFFFFFFFFu / W)) >> 16);
                tri = tri < 32768 ? tri : 65535 - tri;
                int y = mid + amp - (tri * amp * 2 >> 15);
                for (int j = y; j <= mid + amp; j++) if (pat_ink(type == 2 ? PT_DIAG : type == 1 ? PT_LINES : PT_DOTS, X + i, j)) px(X + i, j, 1);
                if (i) line(X + i - 1, prev, X + i, y, 1);
                prev = y;
            }
            hline(X, mid + amp + 1, W, 1);
            static const char *const N[3] = {"CHOR", "FLNG", "PHAS"};
            int bx = X + tag(X, Y + 74, N[type], 1) + 4, bw = X + W - bx;
            frame(bx, Y + 76, bw, 9, 1);
            rect_grad(bx + 1, Y + 77, (bw - 2) * mix / 127, 7, 20, 64, 1);
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
        if (ui.sub == 3) {             /* the project picked, the open one; the display disc */
            char o[16] = {'P', (char)('1' + ui.proj_sel), 0};
            text(&F_L, X + 2, Y + 2, o, 1, 0);
            text(&F_S, X + 2, Y + 34, m2k_project_name(ui.proj_sel), 1, 1);
            if (ui.proj_sel == cur_proj) tag(X + 2, Y + 48, "OPEN", 1);
            int cx = X + W - 18, cy = Y + 84, r = 12;
            for (int j = -r; j <= r; j++)
                for (int i = -r; i <= r; i++) {
                    int d = i * i + j * j;
                    if (d > r * r) continue;
                    if (d > (r - 2) * (r - 2) || i >= 0) px(cx + i, cy + j, 1);
                    else if (T_BAYER[(cy + j) & 7][(cx + i) & 7] < 8) px(cx + i, cy + j, 1);
                }
            (void)cx;
        } else if (ui.sub == 1) {      /* the master, and the repeat when it bites */
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
        /* an empty step: its cell in dots (on the screen's checker, so neighbours line up): the lanes read as a grid */
        for (int i = 0; i < w; i++)
            for (int j = 0; j < h; j++)
                if ((i == 0 || j == 0 || i == w - 1 || j == h - 1) && !((x + i + y + j) & 1)) px(x + i, y + j, 1);
    }
    if (playing) frame(x - 1, y - 1, w + 2, h + 2, 2);
    if (step_entry() && t == ui.trk && s == ui.cursor && ((ui.now >> 8) & 1)) frame(x - 1, y - 1, w + 2, h + 2, 1);
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
        int bip = pi->kind == K_BI || pi->kind == K_PAN || pi->kind == K_SEMI || pi->kind == K_FINE || pi->kind == K_TUN2 || pi->kind == K_SEMI24;
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
        char a[8];
        s_pat(a, cur_pat);
        big_value(2, FOCUS_Y + 16, GX - 6, a, 10, 1000);
        char o[40], *e = s_cpy(o, "P");
        e = s_int(e, cur_proj + 1);
        e = s_cpy(e, "  ");
        e = s_int(e, (int)seq_bpm());
        s_cpy(e, " BPM");
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
    text(&F_S, 4, 90, "TRACK: NOTE", 1, 1);       /* the TRACK knob turns a held step's note */
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

static uint8_t fb_show[M2K_FB_BYTES];

static const uint8_t *present(void)
{
    if (store_.set[S_SCREEN] < 64) return fb_out;
    for (int i = 0; i < M2K_FB_BYTES; i++) fb_show[i] = (uint8_t)~fb_out[i];
    return fb_show;
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
    if (ui.pkey && !ui.pfired && ms - ui.pkey_t0 >= 600) { ui.pfired = 1; page_length(ui.pkey - 1); }
    if (ui.save_t0 && ui.btn[B_SAVE] && !ui.save_fired && ms - ui.save_t0 >= 700) {
        int slot = ui.page == PG_GLO && ui.sub == 3 ? ui.proj_sel : cur_proj;
        ui.save_fired = 1;
        char o[16] = {'P', (char)('1' + slot), ' ', 0};
        s_cpy(o + 3, "SAVED");
        if (project_save(slot)) { toast("PROJECT", o); dirty = 1; ui.save_t = ms | 1u; }
        else toast("PROJECT FULL", "NOT SAVED");
    }

    uint32_t bt = ms - ui.boot_t0;
    if (bt < 1800) {
        boot(bt);
        m_cpy(fb_out, fb, sizeof fb_out);
        return present();
    }
    if (bt < 1800 + 40 && !ui.in_trans && ui.trans_t < ui.boot_t0 + 1800) start_trans(420);   /* out of the splash */

    cls();
    if (ui.page == PG_HOME) { header(); home(); }
    else { header(); param_page(); }
    overlays();

    if (ui.in_trans) {
        uint32_t a = ms - ui.trans_t;
        if (a >= ui.trans_len) { ui.in_trans = 0; m_cpy(fb_out, fb, sizeof fb_out); }
        else cyber((int)(a * 256 / ui.trans_len), ui.glitch_seed ^ ui.trans_t, ui.trans_down);
    } else {
        m_cpy(fb_out, fb, sizeof fb_out);
    }
    if (ui.glitch_t && ms - ui.glitch_t < 120) glitch(fb_out, ms - ui.glitch_t, ui.glitch_seed, ui.glitch_amt);
    return present();
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
