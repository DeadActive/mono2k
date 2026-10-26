/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE */
/* FELUCCA_BLE (m2k_fm1.c includes it with the other Bluetooth files): what the Bluetooth build keeps over power-off,
 * a 300-byte record of its own in two flash sectors, BTS_AT and the one after (0xBE000, 0xBF000: the family's data
 * area under MONO2K's store, which MONO2K never uses; platform.c's FL_RANGE_OK allows them in this build only), each
 * a sector of its own: [magic][length][crc32] then the record, the header written last, so a save cut off leaves that
 * copy invalid and the other one stands. MONO2K's own store (the core's settings, its patterns) is not touched: the
 * plain build never reads these sectors. Nothing of the stock config store (0xE5000..0xFBFFF, the radio calibration).
 *   The record (OberVave's layout): a magic, its length and version, a sequence number, the page's settings (bt_set.c,
 * Felucca's 8 bytes), SYNC IN's BLE / LINK (bts.sync: 0 as the core's SYNC IN says), this FM-1's address (bt_devs.c
 * bt_mac), the 8 saved devices (24 bytes each), the radio calibration's copy (72 bytes: bt_devs.c bt_rf_io), then a
 * CRC-32 of the rest.
 *   Read once, at the start (bts_load: app_init, before the first render, so a read of the flash never meets the
 * second core's job: tools/check_core1.py): the valid copy with the higher sequence number. A change (bts_changed) is
 * saved by the main loop once the sequencer stands (an erase stops the sound ~50 ms, as every MONO2K save), into the
 * other copy. Main loop only. */
#define BTS_MAGIC 0x31544253u                       /* "SBT1": the record */
#define BTS_SECT_MAGIC 0x54424D32u                  /* "2MBT": a sector's header */
#define BTS_VER 1u
typedef struct {
    uint32_t magic;
    uint16_t len, ver;                              /* sizeof, BTS_VER */
    uint32_t seq;
    uint8_t set[8];                                 /* bt_set.c */
    uint8_t sync, rsv[3];                           /* SYNC IN: 0 as the core's, BT_SYNC_BLE, BT_SYNC_LINK */
    uint8_t mac[8];                                 /* bt_devs.c bt_mac */
    uint8_t dev[8][24];                             /* bt_devs.c, the saved devices */
    uint8_t rf[72];                                 /* bt_devs.c bt_rf_io */
    uint32_t crc;                                   /* crc32 of the bytes before it */
} bts_t;
_Static_assert(sizeof(bts_t) == 300u, "the Bluetooth record");
static bts_t bts;
static uint8_t bts_dirty, bts_copy;                /* changed since the last save; the copy it came from */
static uint32_t bts_t_chg;                          /* when it last changed */

#ifndef BT_HOST                                     /* (the host's tests: tests/bt_host.h keeps the two sectors) */
static int bts_io_load(uint32_t c, void *p, uint32_t n)
{
    uint32_t h[3];
    if (!flash_ok || st_read(BTS_AT + c * 0x1000u, h, sizeof h) || h[0] != BTS_SECT_MAGIC || h[1] != n)
        return -1;
    if (st_read(BTS_AT + c * 0x1000u + sizeof h, p, n) || crc32(p, n) != h[2])
        return -1;
    return 0;
}
static int bts_io_save(uint32_t c, const void *p, uint32_t n)   /* p in RAM; erases the sector: ~50 ms */
{
    uint32_t h[3] = {BTS_SECT_MAGIC, n, 0}, at = BTS_AT + c * 0x1000u, took;
    int rc;
    if (!flash_ok)
        return -1;
    h[2] = crc32(p, n);
    fm1_wdt_feed();
    if ((rc = fl_erase4k(at, &took)) != 0 || (rc = fl_write(at + sizeof h, p, n)) != 0)
        return rc;
    return fl_write(at, (const uint8_t *)h, sizeof h);   /* the header last: a cut save stays invalid */
}
#endif

static int bts_valid(const bts_t *r)
{
    return r->magic == BTS_MAGIC && r->len == sizeof *r && r->ver == BTS_VER &&
           r->crc == crc32((const uint8_t *)r, sizeof *r - 4u);
}
static void bts_load(void)                          /* the newer of the two copies' records, else none (all zero):
                                                     * once, at the start (app_init). One record's RAM: copy A's
                                                     * number noted, copy B read over it, A again if B is not newer */
{
    uint32_t seq_a;
    int va, vb;
    va = !bts_io_load(0, &bts, sizeof bts) && bts_valid(&bts);
    seq_a = bts.seq;
    vb = !bts_io_load(1, &bts, sizeof bts) && bts_valid(&bts);
    if (vb && (!va || bts.seq > seq_a)) {
        bts_copy = 1;
        return;
    }
    bts_copy = 0;
    if (!va || bts_io_load(0, &bts, sizeof bts) || !bts_valid(&bts))
        memset(&bts, 0, sizeof bts);
}
static bts_t *bts_get(void) { return &bts; }        /* (RAM only: the render's FM-Link reads SYNC IN through it) */
static void bts_changed(void)                       /* a Bluetooth setting changed: saved once the sequencer stands */
{
    bts_dirty = 1;
    bts_t_chg = fm1_ms;
}
/* the main loop: a change saved into the other copy, 1 s after the last one, when the store could be (quiet) */
static void bt_store_frame(int quiet)
{
    if (!bts_dirty || !quiet || fm1_ms - bts_t_chg < 1000u)
        return;
    bts_dirty = 0;
    bts.magic = BTS_MAGIC;
    bts.len = (uint16_t)sizeof bts;
    bts.ver = BTS_VER;
    bts.seq++;
    bts.crc = crc32((const uint8_t *)&bts, sizeof bts - 4u);
    if (!bts_io_save(bts_copy ^ 1u, &bts, sizeof bts))
        bts_copy ^= 1u;                             /* (failed: the next change tries the same copy again) */
}
