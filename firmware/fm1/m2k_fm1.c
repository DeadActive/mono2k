/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the M-VAVE FM-1: the core (firmware/src) on the open FM-1 platform (src/platform.c: CS-1's, after
 * Dinghy, SLOOP and Felucca; README.md says whose is what): this unit, the platform and the glue; the core is
 * its own unit (both carry statics of the same names).
 *
 * The core runs in the main loop alone, as it runs in the browser's worklet: the loop renders the sound ahead into a
 * ring (RING blocks of M2K_SUB * 8 frames) and the audio interrupt only copies out of it, so no interrupt ever lands
 * in the middle of an edit. Between two blocks the loop takes the panel (keys, buttons, knobs), MIDI, draws a frame
 * when the last has gone out, and sends the screen one changed strip at a time (a whole screen is 77 ms of SPI: the
 * sound would starve if the loop waited for it). */
#define APP_NAME "MONO2K"
#ifndef APP_VER
#define APP_VER "0.0"
#endif
/* the store's region: the last 64 KiB of the family's data area (Felucca's projects and samples live in it too) */
#define BANK_AT 0x000D0000u
#define BANK_LEN 0x00010000u
#define CV_MAX (240u * 24u)                     /* the platform's text: a line of FONT_S at a time (crash, update, rescue) */
#include "platform.c"
#include "m2k.h"                                 /* the core: its own unit (core.o), its API alone */

/* ---- the sound: the ring the main loop fills and the audio interrupt empties */
#define BLK 128u                                /* frames a render */
#define RING 6u                                 /* blocks: 17 ms ahead of the interrupt's own 5.8 ms */
static int16_t ring[RING][BLK * 2u];           /* (RAM, not the pool: the core's buffers fill that) */
static volatile uint32_t ring_w, ring_r;        /* blocks written / read, free-running */
static volatile uint32_t ring_short;            /* halves the interrupt found the ring short for */

static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t i, f = 0;
    while (f < n) {
        if (ring_w == ring_r) {                 /* the loop fell behind: silence, counted */
            ring_short++;
            return;
        }
        const int16_t *b = ring[ring_r % RING];
        for (i = 0; i < BLK * 2u; i++)
            out[f * 2u + i] = b[i];
        ring_r++;
        f += BLK;
    }
}

/* at most half the ring a pass: were the core slower than the sound, the loop still reaches the panel and the
 * watchdog (the sound would stutter, the FM-1 would not reset) */
static uint32_t load_ticks, load_frames;        /* render time against the frames rendered, the meter's */

static void render_ahead(void)
{
    uint32_t k;
    for (k = 0; k < RING / 2u && ring_w - ring_r < RING; k++) {
        uint32_t t0 = fm1_ticks();
        m2k_render(ring[ring_w % RING], (int)BLK);
        load_ticks += fm1_ticks() - t0;
        load_frames += BLK;
        RING_PUBLISH();
        ring_w++;
    }
}

/* ---- the panel: the platform's ids (platform.c) to the core's (m2k.h) */
static int button_of(uint32_t id) { return id == BTN_OCT_DOWN ? B_OCTD : id == BTN_OCT_UP ? B_OCTU : (int)id - 2; }
static const uint8_t ENC_OF[7] = {E_SELECT, E_ALGO, E_K1, E_K2, E_K3, E_K4, E_PRESET};
static uint32_t notes_prev;

static void take_panel(void)
{
    uint32_t rel, pr = fm1_input_edges(&rel), notes = fm1_in.notes, ch = notes ^ notes_prev, i;
    for (i = 0; i < 14u; i++) {
        if (pr >> i & 1u) m2k_button(button_of(i), 1);
        if (rel >> i & 1u) m2k_button(button_of(i), 0);
    }
    for (i = 0; i < M2K_KEYS; i++)
        if (ch >> i & 1u) m2k_key((int)i, (int)(notes >> i & 1u));
    notes_prev = notes;
    for (i = 0; i < 7u; i++) {
        int32_t d = fm1_enc_take(i);
        if (d) m2k_encoder(ENC_OF[i], d);
    }
    uint8_t st, d1, d2;
    while (midi_get(&st, &d1, &d2)) m2k_midi(st, d1, d2);
}

/* ---- a test build's meter (M2K_FM1_DIAG): along the top edge, the render's share of real time (the whole width
 * is 100 %); at the top right, a block while the sound ran short in the last second */
#ifndef M2K_FM1_DIAG
#define M2K_FM1_DIAG 0
#endif
static uint32_t diag_pct, diag_short_ms, diag_last_short;

static void diag_tick(void)
{
    if (load_frames >= FS / 4u) {               /* a quarter second's worth: ticks per frame against 1 / FS */
        uint32_t per = load_ticks / load_frames, real = 1000000u * FM1_TICKS_PER_US / FS;
        diag_pct = per * 100u / (real ? real : 1u);
        load_ticks = load_frames = 0;
    }
    if (ring_short != diag_last_short) { diag_last_short = ring_short; diag_short_ms = fm1_ms; }
}

/* ---- the screen: the core's 1-bit frame, ink and paper as RGB565 (the panel takes it big-endian), sent in strips
 * of ROWS lines where they changed since the last sent */
#define ROWS 8u
#define NSTRIP (240u / ROWS)
static uint8_t shown[M2K_FB_BYTES];             /* what the panel shows */
static uint16_t strip[2][240u * ROWS];          /* two: one fills while the other goes out */
static uint8_t todo[NSTRIP], pending, sbuf;
static const uint8_t *frame;

static uint16_t be16(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }

static void screen_frame(void)
{
    uint32_t s, i;
    frame = m2k_frame(fm1_ms);
    for (s = 0; s < NSTRIP; s++) {
        const uint8_t *a = frame + s * ROWS * 30u, *b = shown + s * ROWS * 30u;
        for (i = 0; i < ROWS * 30u && a[i] == b[i]; i++)
            ;
        todo[s] = i < ROWS * 30u;
        pending += todo[s];
    }
}

static void screen_send(void)                   /* one strip, if the last has gone out */
{
    uint32_t s, x, y;
    uint16_t ink = be16(RGB(11, 11, 11)), paper = be16(RGB(242, 241, 236));   /* the site's ink and paper */
    if (!pending || !lcd_idle())
        return;
    for (s = 0; s < NSTRIP && !todo[s]; s++)
        ;
    if (s == NSTRIP) { pending = 0; return; }
    lcd_sync();
    uint16_t *px = strip[sbuf];
    const uint8_t *src = frame + s * ROWS * 30u;
    for (y = 0; y < ROWS; y++)
        for (x = 0; x < 240u; x++)
            px[y * 240u + x] = src[y * 30u + (x >> 3)] >> (7u - (x & 7u)) & 1u ? ink : paper;
    memcpy(shown + s * ROWS * 30u, src, ROWS * 30u);
    if (M2K_FM1_DIAG && s == 0) {               /* the meter over the first strip (it is resent each frame) */
        uint32_t w = diag_pct * 240u / 100u, red = be16(RGB(220, 40, 30));
        for (x = 0; x < 240u; x++) px[x] = px[240u + x] = x < (w > 240u ? 240u : w) ? red : px[x];
        if (fm1_ms - diag_short_ms < 1000u && diag_last_short)
            for (y = 2; y < ROWS; y++) for (x = 232; x < 240u; x++) px[y * 240u + x] = red;
        shown[0] ^= 0x01u;                      /* so the next frame sends this strip again */
    }
    lcd_blit(0, s * ROWS, 240, ROWS, px);
    sbuf ^= 1u;
    todo[s] = 0;
    pending--;
}

/* ---- the lights: the core's 27 keys and 14 buttons, 0..255, on from a quarter up (the panel's LEDs are on / off) */
static void lights(void)
{
    const uint8_t *l = m2k_leds();
    uint32_t i;
    for (i = 0; i < M2K_KEYS; i++) fm1_led_key(KEY_LED(i), l[i] >= 64u);
    for (i = 0; i < 14u; i++) fm1_led_key(i, l[M2K_KEYS + button_of(i)] >= 64u);
}

/* ---- the store, in the flash: [magic][length][crc32][0] then the stream m2k_store_pack writes, front to back,
 * through a sector at a time (erased, then programmed: ~50 ms each with the interrupts off, so only when the
 * sequencer stands); the header last, so a save cut short reads as none */
#define STORE_MAGIC 0x464B324Du                 /* "M2KF" */
#define STORE_HDR 16u
static uint8_t sec[4096];
static int32_t sec_at = -1;                     /* the sector the buffer holds, -1 none */
static uint32_t st_crc, saves_failed;
static uint8_t save_due;

static uint32_t crc_more(uint32_t c, const uint8_t *p, uint32_t n)
{
    uint32_t k;
    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8u; k++) c = c & 1u ? (c >> 1) ^ 0xEDB88320u : c >> 1;
    }
    return c;
}

static int sec_flush(void)
{
    uint32_t took;
    int rc;
    if (sec_at < 0) return 0;
    fm1_wdt_feed();
    rc = fl_erase4k(BANK_AT + (uint32_t)sec_at * 4096u, &took);
    if (!rc) rc = fl_write(BANK_AT + (uint32_t)sec_at * 4096u, sec, 4096u);
    sec_at = -1;
    return rc;
}

int m2k_fw_store_put(uint32_t off, const uint8_t *p, uint32_t n)
{
    off += STORE_HDR;
    if (off + n > BANK_LEN) return -1;
    st_crc = crc_more(st_crc, p, n);
    while (n) {
        uint32_t s = off >> 12, o = off & 4095u, k = 4096u - o;
        if (k > n) k = n;
        if ((int32_t)s != sec_at) {
            if (sec_flush()) return -1;
            sec_at = (int32_t)s;
            memset(sec, 0xFF, sizeof sec);
        }
        memcpy(sec + o, p, k);
        off += k;
        p += k;
        n -= k;
    }
    return 0;
}

int m2k_fw_store_get(uint32_t off, uint8_t *p, uint32_t n)
{
    return off + STORE_HDR + n > BANK_LEN ? -1 : st_read(BANK_AT + STORE_HDR + off, p, n);
}

static void store_save(void)
{
    uint32_t h[4];
    int len;
    if (!flash_ok) return;
    st_crc = 0xFFFFFFFFu;
    sec_at = -1;
    len = m2k_store_pack();
    if (len <= 0 || sec_flush()) { saves_failed++; return; }
    h[0] = STORE_MAGIC; h[1] = (uint32_t)len; h[2] = ~st_crc; h[3] = 0;
    if (fl_write(BANK_AT, (const uint8_t *)h, sizeof h)) saves_failed++;
}

static int store_load(void)
{
    uint32_t h[4], at, c = 0xFFFFFFFFu;
    uint8_t b[256];
    if (!flash_ok || st_read(BANK_AT, h, sizeof h) || h[0] != STORE_MAGIC || !h[1] || h[1] > BANK_LEN - STORE_HDR)
        return 0;
    for (at = 0; at < h[1]; at += sizeof b) {
        uint32_t k = h[1] - at > sizeof b ? sizeof b : h[1] - at;
        if (st_read(BANK_AT + STORE_HDR + at, b, k)) return 0;
        c = crc_more(c, b, k);
    }
    return ~c == h[2] && m2k_store_unpack((int)h[1]);
}

static void app_init(void)
{
    m2k_init(fm1_ticks());
    store_load();                               /* the factory's bank stays when there is none */
    lcd_fill(0, 0, 240, 240, RGB(242, 241, 236));
    render_ahead();
    screen_frame();
}

static void app_loop(void)
{
    if (M2K_FM1_DIAG) diag_tick();
    render_ahead();
    take_panel();
    render_ahead();
    if (!pending) {
        lights();
        screen_frame();
    }
    screen_send();
    if (m2k_dirty()) save_due = 1;
    if (save_due && !m2k_playing()) {           /* SAVE kept it: into the flash once the sequencer stands */
        store_save();
        save_due = 0;
    }
}
