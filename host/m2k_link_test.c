/* SPDX-License-Identifier: GPL-3.0-only */
/* The FM-1's store and the Patch Editor's link (firmware/fm1/m2k_link.c) on a flash in memory, with the core as the
 * FM-1 builds it (16 patterns, one project), and the host's side of the link as web/editor.js speaks it: a store
 * read out and written back, and what a wrong, cut, stalled or busy write leaves. The flash is NOR: a write only
 * clears bits, and one over a byte not erased is counted as a fault.
 *
 *   cc -O2 -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/host/m2k_link_test host/m2k_link_test.c
 *   build/host/m2k_link_test [WEB_STORE] [FM1_STORE_OUT]
 *
 * WEB_STORE: a store the browser wrote for the FM-1 (build/host/m2k_host --web-store PATH), written through the link;
 * FM1_STORE_OUT: the store read out of the FM-1, for the browser to read (m2k_host --fm1-store PATH). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define M2K_FW 1
#define M2K_PATTERNS 64
#define M2K_PAT_SLOTS 2
#define M2K_PROJECTS 1
#define M2K_IMG_DATA 47104
#define POOL
#include "../firmware/src/m2k_core.c"

/* ---- the platform, as m2k_link.c uses it */
#define APP_NAME "MONO2K"
#define APP_VER "0.0"
#define BANK_AT 0x000C0000u
#define BANK_LEN 0x00020000u
#define FELUCCA_ID "FM-1_980"
static uint8_t flash[0x100000];
static int flash_faults, flash_fail_at = -1, flash_writes;
static uint8_t flash_ok = 1;
static volatile uint32_t fm1_ms;
static void fm1_wdt_feed(void) {}
static int fl_erase4k(uint32_t off, uint32_t *took)
{
    *took = 0;
    if (off & 0xFFFu || off + 4096u > sizeof flash) return -1;
    memset(flash + off, 0xFF, 4096);
    return 0;
}
static int fl_write(uint32_t off, const uint8_t *p, uint32_t n)
{
    if (off + n > sizeof flash || flash_writes++ == flash_fail_at) return -1;
    for (uint32_t i = 0; i < n; i++) {
        if (flash[off + i] != 0xFF) flash_faults++;
        flash[off + i] &= p[i];
    }
    return 0;
}
static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (off + n > sizeof flash) return -1;
    memcpy(dst, flash + off, n);
    return 0;
}

/* the update entry's hooks (ota.c: only its 7-bit packing is used here); a reply is kept for the test */
static uint8_t reply[1024];
static uint32_t reply_n;
static int replies;
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    if (n <= sizeof reply) { memcpy(reply, p, n); reply_n = n; }
    replies++;
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n) { (void)p; (void)n; return 0; }
static void ota_frame_done(void) {}
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void) {}
static int ota_erase(uint32_t off) { (void)off; return -1; }
static int ota_prog(uint32_t off, const void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int ota_fread(uint32_t off, void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static void ota_show(uint32_t step, int32_t code) { (void)step; (void)code; }
static void ota_commit(const uint8_t *parm) { (void)parm; }
#include "../firmware/fm1/src/ota.c"

#include "../firmware/fm1/m2k_link.c"

/* ---- the editor's side */
static uint8_t ans[512];
static uint32_t ans_n;
static int wire_max;

/* a request through the platform's hook; the status of the one answer, decoded into ans (-1 not taken, -2 no answer
 * or a malformed one) */
static int ask(int cmd, const uint8_t *body, uint32_t n)
{
    uint8_t f[640];
    uint32_t w = 0;
    f[w++] = 0x7D; f[w++] = 'M'; f[w++] = '2'; f[w++] = 'K'; f[w++] = (uint8_t)cmd;
    w += ota_pack7(body, n, f + w);
    if (w + 2u > 640u) return -3;                 /* (F0 and F7 around it: the platform's frame) */
    replies = 0;
    if (!app_sysex(f, w)) return -1;
    lk_service();
    if (replies != 1 || reply_n < 8u || reply[0] != 0xF0 || reply[reply_n - 1] != 0xF7 || reply[1] != 0x7D ||
        reply[2] != 'M' || reply[3] != '2' || reply[4] != 'K' || reply[5] != (cmd | 0x40)) return -2;
    for (uint32_t i = 1; i + 1 < reply_n; i++) if (reply[i] & 0x80) return -2;
    if ((int)reply_n > wire_max) wire_max = (int)reply_n;
    ans_n = ota_unpack7(reply + 6, reply_n - 7, ans, sizeof ans);
    return ans[0];
}

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static uint32_t crc32(const uint8_t *p, uint32_t n) { return ~crc_more(0xFFFFFFFFu, p, n); }

/* the store read out: INFO, then READ 128 bytes at a time */
static uint8_t rx[0x10000];
static int read_store(uint32_t *len)
{
    if (ask(LK_INFO, 0, 0) != LK_OK || ans_n < 9u) return 0;
    uint32_t n = rd32(ans + 1), c = rd32(ans + 5);
    for (uint32_t at = 0; at < n;) {
        uint8_t q[5];
        wr32(q, at);
        q[4] = (uint8_t)(n - at > 128u ? 128u : n - at);
        if (ask(LK_READ, q, 5) != LK_OK || rd32(ans + 1) != at || ans[5] != q[4]) return 0;
        memcpy(rx + at, ans + 6, ans[5]);
        at += ans[5];
    }
    *len = n;
    return crc32(rx, n) == c;
}

/* a store written: BEGIN, WRITE 256 bytes at a time, END; the END's status (or -10 - the step that failed) */
static int write_store(const uint8_t *b, uint32_t n, uint32_t crc, uint32_t stop_after)
{
    uint8_t q[4 + 256];
    wr32(q, n);
    wr32(q + 4, crc);
    if (ask(LK_BEGIN, q, 8) != LK_OK) return -10 - ans[0];
    for (uint32_t at = 0, k = 0; at < n; k++) {
        if (k == stop_after) return -100;
        uint32_t c = n - at > 256u ? 256u : n - at;
        wr32(q, at);
        memcpy(q + 4, b + at, c);
        if (ask(LK_WRITE, q, 4 + c) != LK_OK || rd32(ans + 1) != at + c) return -20 - ans[0];
        at += c;
    }
    return ask(LK_END, 0, 0);
}

static int fails;
static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

static void mark(const char *pn, const char *sn)   /* a state to tell apart: pattern A01's name, sound slot 6's */
{
    memset(PAT(0)->name, 0, 8);
    memcpy(PAT(0)->name, pn, strlen(pn));
    memset(snd[5].name, 0, 8);
    memcpy(snd[5].name, sn, strlen(sn));
}
static int marked(const char *pn, const char *sn) { return !strcmp(PAT(0)->name, pn) && !strcmp(snd[5].name, sn); }
static int stored(void) { uint32_t h[4]; return !st_read(BANK_AT, h, 16) && h[0] == STORE_MAGIC && store_load(); }

/* the pattern cache (the FM-1 keeps two of its 64 patterns unpacked, the rest packed in the working image) */
static int trigs_of(int i)
{
    int n = 0;
    for (int t = 0; t < M2K_TRACKS; t++) for (int st = 0; st < M2K_STEPS; st++) n += PAT(i)->step[t][st].flags & ST_TRIG ? 1 : 0;
    return n;
}

static void fill(int i, int seed)                 /* every step of every track a trig, its note by seed */
{
    for (int t = 0; t < M2K_TRACKS; t++)
        for (int st = 0; st < M2K_STEPS; st++) {
            m2k_step_t *x = &PAT(i)->step[t][st];
            x->flags = ST_TRIG;
            x->note = (uint8_t)(24 + (seed * 7 + t * 5 + st) % 72);
            x->cond = 0;
            x->rtrg = 0;
        }
}

static uint32_t bank_hash(void)                   /* every pattern packed, and the sound library (m2k_host --bank-hash) */
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < M2K_PATTERNS; i++) { int n = pat_pack(PAT(i), blob); for (int k = 0; k < n; k++) h = (h ^ blob[k]) * 16777619u; }
    const uint8_t *sb = (const uint8_t *)snd;
    for (unsigned k = 0; k < sizeof snd; k++) h = (h ^ sb[k]) * 16777619u;
    return h;
}

static void cache_tests(void)
{
    static int16_t buf[256];
    memset(flash, 0xFF, sizeof flash);
    m2k_init(3);
    printf("     bank hash %08x\n", bank_hash());
    int names = !strcmp(PAT(0)->name, "MAMMOTH") && !strcmp(PAT(15)->name, "SPLICE");
    for (int i = 16; i < 64; i++) names &= !strcmp(PAT(i)->name, "INIT") && trigs_of(i) == 0;
    check(names && work_img.used < 20000u, "64 patterns: bank A's demos, banks B-D INIT, the blanks taking no room");

    m2k_pattern(16);                              /* B01 */
    PAT(16)->step[2][5].flags = ST_TRIG; PAT(16)->step[2][5].note = 49;
    m2k_pattern(36);                              /* C05 */
    PAT(36)->step[0][0].flags = ST_TRIG; PAT(36)->step[0][0].note = 40;
    m2k_pattern(0);
    m2k_pattern(63);
    int kept = cur_pat == 63 && PAT(16)->step[2][5].note == 49 && trigs_of(16) == 1 && PAT(36)->step[0][0].note == 40;
    store_save();
    m2k_init(4);
    int back = store_load() && PAT(16)->step[2][5].note == 49 && trigs_of(36) == 1 && !strcmp(PAT(0)->name, "MAMMOTH");
    check(kept && back, "patterns edited, left and come back to keep their steps, through a save and a boot");

    m2k_pattern(16);
    seq_play(1);
    m2k_pattern(36);                              /* queued: at the end of the bar */
    int queued = cur_pat == 16 && sq.queued == 36;
    for (int f = 0; f < 44100 * 12 && cur_pat == 16; f += 128) m2k_render(buf, 128);
    int switched = cur_pat == 36;
    for (int f = 0; f < 44100 * 2; f += 128) m2k_render(buf, 128);
    seq_play(0);
    check(queued && switched && trigs_of(16) == 1, "a pattern queued while playing opens at the end of the bar");

    /* the working image fills: a pattern that would not leave the playing one room is refused, and nothing is lost */
    int filled[64] = {0}, last = -1, refused = 0;
    for (int i = 16; i < 64 && !refused; i++) {
        m2k_pattern(i);
        if (cur_pat != i) { refused = 1; break; }
        fill(i, i);
        filled[i] = 1;
        last = i;
    }
    int intact = refused && last > 20;
    for (int i = 16; i <= last; i++) intact &= !filled[i] || (trigs_of(i) == 6 * 64 && PAT(i)->step[3][9].note == (uint8_t)(24 + (i * 7 + 15 + 9) % 72));
    printf("     the working image: %u of %u B, patterns full after %d full ones\n", work_img.used, IMG_DATA, last - 15);
    store_save();
    m2k_init(5);
    int stored = store_load();
    for (int i = 16; i <= last; i++) stored &= trigs_of(i) == 6 * 64;
    check(intact && stored, "patterns full: the next refused, every full one intact, through a save and a boot");
}

int main(int argc, char **argv)
{
    memset(flash, 0xFF, sizeof flash);
    m2k_init(1);
    check(!store_load(), "an erased flash holds no store: the factory's bank stays");

    uint8_t f0[4] = {0x00, 0x59, 0x11, 0x00};
    check(!app_sysex(f0, 4), "the update entry's frames are left to it");
    check(ask(LK_HELLO, 0, 0) == LK_OK && ans[1] == 1 && ans[2] == 64 && ans[3] == 1 && ans[4] == M2K_SOUNDS &&
          !memcmp(ans + 8, "MONO2K", 6) && rd32(ans + 24) == M2K_IMG_DATA,
          "HELLO: the link's version, 64 patterns, 1 project, 128 sounds, the name, the image's room");

    mark("LINKA", "SNDA");
    uint32_t na = 0;
    check(read_store(&na) && na > 1000u, "the store read out: saved first, 128 bytes a reply, its CRC-32 agrees");
    printf("     store %u B, the longest reply %d wire bytes\n", na, wire_max);
    check(wire_max <= 3 * 64, "a reply fits the platform's SysEx queue (64 USB-MIDI packets) without waiting");
    static uint8_t a[0x10000];
    memcpy(a, rx, na);
    check(!memcmp(a, flash + BANK_AT + STORE_HDR, na), "what was read is what the flash holds");
    if (argc > 2) {
        FILE *f = fopen(argv[2], "wb");
        if (f) { fwrite(a, 1, na, f); fclose(f); }
    }

    mark("LINKB", "SNDB");
    store_save();
    check(stored() && marked("LINKB", "SNDB"), "(a second state saved)");
    flash_faults = 0;
    int st = write_store(a, na, crc32(a, na), ~0u);
    check(st == LK_OK && marked("LINKA", "SNDA"), "a store written: the core reads it, the patterns and the sounds");
    check(stored() && marked("LINKA", "SNDA"), "and it is the FM-1's store from then on");
    check(flash_faults == 0, "no byte programmed without its erase");

    mark("LINKC", "SNDC");
    check(write_store(a, na, crc32(a, na) ^ 1u, ~0u) == LK_CRC && marked("LINKC", "SNDC"),
          "a wrong CRC is refused: the FM-1 keeps what it plays");
    check(stored() && marked("LINKC", "SNDC"), "and its own state is back in the flash");

    uint8_t q[4 + 8];
    wr32(q, na); wr32(q + 4, crc32(a, na));
    check(ask(LK_BEGIN, q, 8) == LK_OK, "(a write begun)");
    wr32(q, 256); memset(q + 4, 0, 8);
    check(ask(LK_WRITE, q, 12) == LK_ORDER && rd32(ans + 1) == 0, "a chunk out of order is refused, the next offset said");
    fm1_ms += 5000;
    lk_service();
    check(!lk_open && stored() && marked("LINKC", "SNDC"), "a write that stalls 4 s ends: the FM-1's state back in the flash");

    check(write_store(a, na, crc32(a, na), 5) == -100, "(a write cut after five chunks)");
    fm1_ms += 5000;
    lk_service();
    check(stored() && marked("LINKC", "SNDC"), "a write cut short leaves the FM-1 as it was");

    sq.playing = 1;
    check(ask(LK_INFO, 0, 0) == LK_PLAYING && ask(LK_BEGIN, q, 8) == LK_PLAYING, "while it plays: neither read nor write");
    sq.playing = 0;

    check(ask(LK_END, 0, 0) == LK_IDLE && ask(LK_WRITE, q, 12) == LK_IDLE, "an END or a chunk with no write begun: refused");
    wr32(q, BANK_LEN); wr32(q + 4, 0);
    check(ask(LK_BEGIN, q, 8) == LK_BIG, "a store larger than the region: refused");
    check(ask(0x30, 0, 0) == LK_BAD, "an unknown request: answered, refused");

    flash_fail_at = flash_writes + 3;
    st = write_store(a, na, crc32(a, na), ~0u);
    flash_fail_at = -1;
    check(st != LK_OK && stored() && marked("LINKC", "SNDC"), "a flash that fails during a write: refused, the FM-1's state kept");

    if (argc > 1) {                                /* the browser's store for the FM-1 */
        FILE *f = fopen(argv[1], "rb");
        static uint8_t w[0x10000];
        size_t n = f ? fread(w, 1, sizeof w, f) : 0;
        if (f) fclose(f);
        check(n > 0 && write_store(w, (uint32_t)n, crc32(w, (uint32_t)n), ~0u) == LK_OK && marked("WEB16", "WEBSND"),
              "a store the browser wrote for the FM-1 (64 patterns, 1 project): written and read");
    }

    cache_tests();

    uint8_t pk[10] = {0, 1, 2, 0x7F, 0x80, 0xFF, 0x55, 0xAA, 0x01, 0xFE}, pw[16];
    uint32_t pn = ota_pack7(pk, 10, pw);
    printf("     pack7 of 00 01 02 7F 80 FF 55 AA 01 FE:");
    for (uint32_t i = 0; i < pn; i++) printf(" %02X", pw[i]);
    printf("\n");
    printf("%s (%d failed)\n", fails ? "link test FAILED" : "link test passed", fails);
    return fails != 0;
}
