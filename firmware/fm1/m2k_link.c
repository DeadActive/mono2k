/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the FM-1: the store in the flash and the Patch Editor's link to it. Included by m2k_fm1.c after the
 * platform; host/m2k_link_test.c runs it on a flash in memory. From the platform it takes fl_erase4k, fl_write,
 * st_read, flash_ok, fm1_wdt_feed, fm1_ms, ota_pack7 / ota_unpack7 and ota_wire_send; BANK_AT / BANK_LEN and
 * APP_NAME / APP_VER are the glue's. */

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

/* ---- the link: the website's Patch Editor reads the store and writes one back over USB-MIDI SysEx,
 * F0 7D 'M' '2' 'K' cmd pack7(body) F7, each request answered (cmd | 0x40, a status first) before the host sends the
 * next (the platform keeps one frame). The store goes out as it lies in the flash, saved first, 128 bytes a reply;
 * one comes in the same way, 256 bytes a request, into the store's place, and its header is written once its length
 * and CRC-32 agree; the core then reads it as at boot. Anything that fails or stalls (4 s) writes the FM-1's own state
 * back. Only while the sequencer stands: a sector's erase stops the sound. */
enum { LK_HELLO = 1, LK_INFO, LK_READ, LK_BEGIN, LK_WRITE, LK_END };
enum { LK_OK, LK_PLAYING, LK_BIG, LK_IDLE, LK_ORDER, LK_FLASH, LK_CRC, LK_REFUSED, LK_BAD };
#define LK_IN 640u                               /* the platform's frame */
static uint8_t lk_in[LK_IN], lk_dec[320], lk_out[140], lk_wire[8u + 162u];
static uint32_t lk_n, lk_len, lk_crc, lk_at, lk_ms;
static uint8_t lk_pending, lk_open;

static int app_sysex(const uint8_t *p, uint32_t n)
{
    if (n < 5u || p[0] != 0x7Du || p[1] != 'M' || p[2] != '2' || p[3] != 'K') return 0;
    if (!lk_pending && n <= LK_IN) {
        memcpy(lk_in, p, n);
        lk_n = n;
        lk_pending = 1;
    }
    return 1;
}

static uint32_t lk_rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void lk_wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

static void lk_reply(uint32_t cmd, uint32_t n)   /* lk_out[0..n) */
{
    uint32_t w = 0;
    lk_wire[w++] = 0xF0; lk_wire[w++] = 0x7D; lk_wire[w++] = 'M'; lk_wire[w++] = '2'; lk_wire[w++] = 'K';
    lk_wire[w++] = (uint8_t)(cmd | 0x40u);
    w += ota_pack7(lk_out, n, lk_wire + w);
    lk_wire[w++] = 0xF7;
    ota_wire_send(lk_wire, w);
}

static void lk_status(uint32_t cmd, uint32_t st) { lk_out[0] = (uint8_t)st; lk_reply(cmd, 1); }

static void lk_abort(void)                       /* a write cut short: the FM-1's own state back into the flash */
{
    lk_open = 0;
    store_save();
}

static void lk_service(void)
{
    uint32_t cmd, d, h[4];
    if (lk_open && fm1_ms - lk_ms > 4000u) lk_abort();
    if (!lk_pending) return;
    cmd = lk_in[4];
    d = ota_unpack7(lk_in + 5, lk_n - 5u, lk_dec, sizeof lk_dec);
    lk_pending = 0;
    if (d > sizeof lk_dec) { lk_status(cmd, LK_BAD); return; }
    switch (cmd) {
    case LK_HELLO: {                             /* 1, the counts, playing, the name and version */
        static const char nm[] = APP_NAME " " APP_VER;
        uint32_t i;
        memset(lk_out, 0, 24);
        lk_out[0] = LK_OK; lk_out[1] = 1; lk_out[2] = M2K_PATTERNS; lk_out[3] = M2K_PROJECTS; lk_out[4] = M2K_SOUNDS;
        lk_out[5] = (uint8_t)m2k_playing();
        for (i = 0; i < 16u && nm[i]; i++) lk_out[8 + i] = (uint8_t)nm[i];
        lk_reply(cmd, 24);
        break;
    }
    case LK_INFO: {                              /* saved now, then its length and CRC */
        uint32_t failed = saves_failed;
        if (lk_open) lk_abort();
        if (m2k_playing()) { lk_status(cmd, LK_PLAYING); break; }
        store_save();
        save_due = 0;
        if (saves_failed != failed || st_read(BANK_AT, h, sizeof h) || h[0] != STORE_MAGIC) { lk_status(cmd, LK_FLASH); break; }
        lk_out[0] = LK_OK;
        lk_wr32(lk_out + 1, h[1]);
        lk_wr32(lk_out + 5, h[2]);
        lk_reply(cmd, 9);
        break;
    }
    case LK_READ: {                              /* off, n (up to 128) -> off, n, the bytes */
        uint32_t off, n;
        if (d < 5u) { lk_status(cmd, LK_BAD); break; }
        off = lk_rd32(lk_dec);
        n = lk_dec[4] > 128u ? 128u : lk_dec[4];
        if (off + n > BANK_LEN - STORE_HDR || st_read(BANK_AT + STORE_HDR + off, lk_out + 6, n)) { lk_status(cmd, LK_BIG); break; }
        lk_out[0] = LK_OK;
        lk_wr32(lk_out + 1, off);
        lk_out[5] = (uint8_t)n;
        lk_reply(cmd, 6u + n);
        break;
    }
    case LK_BEGIN:                               /* length, CRC */
        if (d < 8u) { lk_status(cmd, LK_BAD); break; }
        if (m2k_playing()) { lk_status(cmd, LK_PLAYING); break; }
        lk_len = lk_rd32(lk_dec);
        lk_crc = lk_rd32(lk_dec + 4);
        if (!lk_len || lk_len > BANK_LEN - STORE_HDR || !flash_ok) { lk_status(cmd, LK_BIG); break; }
        lk_open = 1;
        lk_at = 0;
        lk_ms = fm1_ms;
        st_crc = 0xFFFFFFFFu;
        sec_at = -1;
        lk_status(cmd, LK_OK);
        break;
    case LK_WRITE: {                             /* off, the bytes (up to 256) -> status, the next offset */
        uint32_t off = d >= 4u ? lk_rd32(lk_dec) : 0, n = d >= 4u ? d - 4u : 0, st = LK_OK;
        if (!lk_open) st = LK_IDLE;
        else if (d < 5u || off != lk_at || lk_at + n > lk_len) st = LK_ORDER;
        else if (m2k_fw_store_put(off, lk_dec + 4, n)) { st = LK_FLASH; lk_abort(); }
        else { lk_at += n; lk_ms = fm1_ms; }
        lk_out[0] = (uint8_t)st;
        lk_wr32(lk_out + 1, lk_at);
        lk_reply(cmd, 5);
        break;
    }
    case LK_END: {                               /* the header, then the core reads it */
        uint32_t st = LK_OK;
        if (!lk_open) { lk_status(cmd, LK_IDLE); break; }
        lk_open = 0;
        if (lk_at != lk_len || sec_flush() || ~st_crc != lk_crc) st = LK_CRC;
        else {
            h[0] = STORE_MAGIC; h[1] = lk_len; h[2] = lk_crc; h[3] = 0;
            if (fl_write(BANK_AT, (const uint8_t *)h, sizeof h)) st = LK_FLASH;
            else if (!store_load()) st = LK_REFUSED;
        }
        if (st != LK_OK) store_save();
        else save_due = 0;
        lk_status(cmd, st);
        break;
    }
    default: lk_status(cmd, LK_BAD); break;
    }
}
