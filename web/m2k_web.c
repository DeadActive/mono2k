/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K in the browser: the firmware's core, unchanged, with a few exports for the AudioWorklet and the page.
 * web/build.sh compiles it to web/m2k.wasm. */
#include "../firmware/src/m2k_core.c"

#define EXPORT __attribute__((used, visibility("default")))

static int16_t w_buf[2 * 128];
static uint8_t w_inf[32];

EXPORT int16_t *w_audio(void) { return w_buf; }
EXPORT void w_render(int n) { m2k_render(w_buf, n); }
EXPORT const uint8_t *w_frame(uint32_t ms) { return m2k_frame(ms); }
EXPORT const uint8_t *w_leds(void) { return m2k_leds(); }
EXPORT void w_init(uint32_t seed) { m2k_init(seed); }
EXPORT void w_key(int k, int d) { m2k_key(k, d); }
EXPORT void w_button(int b, int d) { m2k_button(b, d); }
EXPORT void w_encoder(int e, int d) { m2k_encoder(e, d); }
EXPORT void w_midi(int s, int a, int b) { m2k_midi(s, a, b); }
EXPORT uint8_t *w_store(void) { int n; return m2k_store(&n); }
EXPORT int w_store_len(void) { int n; m2k_store(&n); return n; }
EXPORT void w_restore(void) { m2k_restore(); }
EXPORT int w_dirty(void) { return m2k_dirty(); }

/* what the page shows next to the panel */
EXPORT const uint8_t *w_info(void)
{
    w_inf[0] = ui.trk;
    w_inf[1] = ui.page;
    w_inf[2] = ui.sub;
    w_inf[3] = (uint8_t)cur_pat;
    w_inf[4] = sq.playing;
    w_inf[5] = (uint8_t)(seq_bpm() - 30);
    w_inf[6] = pat[cur_pat].machine[ui.trk];
    w_inf[7] = ui.grid;
    w_inf[8] = sq.rec;
    w_inf[9] = sq.fill;
    w_inf[10] = (uint8_t)play_step();
    w_inf[11] = (uint8_t)seq_len();
    w_inf[12] = sq.queued;
    w_inf[13] = pat[cur_pat].mute;
    w_inf[14] = (uint8_t)ui.oct;
    w_inf[15] = master_vol;
    w_inf[16] = ui.nheld;
    for (int t = 0; t < M2K_TRACKS; t++) w_inf[17 + t] = pat[cur_pat].machine[t];
    return w_inf;
}

/* the machines, for the page's machine sheet */
EXPORT const uint8_t *w_mark(int m) { return MARK_BITS[m % ICON_COUNT]; }
EXPORT const uint8_t *w_icon(int m, int f) { return ICON_BITS[m % ICON_COUNT][f % ICON_FRAMES]; }
EXPORT int w_machines(void) { return M_COUNT; }
EXPORT const char *w_mi(int m, int field)
{
    const mi_t *mi = &MI[m % M_COUNT];
    switch (field) {
    case 0: return mi->name;
    case 1: return mi->badge;
    case 2: return mi->style;
    case 3: return mi->cls;
    default: return "";
    }
}
EXPORT int w_stat(int m, int i) { return MI[m % M_COUNT].stat[i % 3]; }
EXPORT const char *w_syn(int m, int i, int longname) { const pi_t *p = &MI[m % M_COUNT].syn[i & 7]; return longname ? p->name : p->s4; }
EXPORT const char *w_pat_name(int i) { return pat[i % M2K_PATTERNS].name; }
