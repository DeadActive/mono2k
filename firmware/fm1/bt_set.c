/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE
 * FM-Link: 2026 DEADACTIVE */
/* FELUCCA_BLE (m2k_fm1.c; from OberVave's bt_set.c): what the Bluetooth page keeps over power-off, in the Bluetooth record (bt_store.c bts.set, 8 bytes in
 * Felucca's layout):
 *   [0..5] retired (Felucca's one remembered device, before the saved devices: bt_devs.c; written 0), [6] bits 0-1
 *   MODE, bit 2 retired, bit 3 AUTOSTART (Bluetooth on by itself after a clean boot, bt_page.c bt_page_auto), bit 4
 *   ROLE (0 HOST, 1 DEVICE), bit 5 retired (written 0), bits 6-7 RECONNECT (0 AUTO, 1 FAST, 2 SLOW; 3 a damaged
 *   record), [7] the XOR of the seven bytes before it, XOR 0xB7.
 * A record without the bytes (none yet: all zero) or with a wrong check byte is MODE AUTO, AUTOSTART OFF, ROLE HOST,
 * RECONNECT AUTO. And SYNC IN's two Bluetooth values (bts.sync). Main loop only. */
#define BT_SET_C 1
enum { BT_MODE_AUTO, BT_MODE_SMC, BT_MODE_MIDI, BT_MODE_LINK };   /* LINK: FM-Link only, no MIDI either way */
enum { BT_RATE_AUTO, BT_RATE_FAST, BT_RATE_SLOW };   /* RECONNECT: a host link's rest after a drop (bt_set_slow) */
typedef struct { uint8_t addr[6], type, mode, has, autostart, role, link, rate; } bt_set_t;   /* has: a device is
                                                                  * remembered; role: 1 DEVICE; link: HOST + LINK */
#define bt_set_bytes (bts_get()->set)

static uint8_t bt_set_sum(const uint8_t *b)
{
    uint32_t i, x = 0xB7u;
    for (i = 0; i < 7u; i++)
        x ^= b[i];
    return (uint8_t)x;
}

static void bt_set_get(bt_set_t *s)
{
    const uint8_t *b = bt_set_bytes;
    uint32_t i;
    memset(s, 0, sizeof *s);
    if (b[7] != bt_set_sum(b) || b[6] >= 0xC0u)
        return;
    for (i = 0; i < 6u; i++) {
        s->addr[i] = b[i];
        s->has |= b[i] != 0u;
    }
    s->type = (b[6] >> 2) & 1u;
    s->mode = b[6] & 3u;
    s->autostart = (b[6] >> 3) & 1u;
    s->role = (b[6] >> 4) & 1u;
    s->link = (b[6] >> 5) & 1u;
    s->rate = b[6] >> 6;
}

/* the bytes written and saved (bt_store.c bts_changed: once stopped and quiet) */
static void bt_set_put(const bt_set_t *s)
{
    uint8_t *b = bt_set_bytes;
    uint32_t i;
    for (i = 0; i < 6u; i++)
        b[i] = s->addr[i];
    b[6] = (uint8_t)((s->mode <= BT_MODE_LINK ? s->mode : BT_MODE_AUTO) | (s->type ? 4u : 0u) | (s->autostart ? 8u : 0u) |
                     (s->role ? 16u : 0u) | (s->link ? 32u : 0u) | (uint32_t)s->rate << 6);
    b[7] = bt_set_sum(b);
    bts_changed();
}

/* RECONNECT: a host link rests longer after a drop or a failed connect (ble_fail_rest: 8 s, then 16 s): SLOW, or AUTO
 * while the transport plays (the user: a mixer that dropped refuses for ~8 s, each refused try holds the audio off
 * ~12 ms); FAST, or AUTO stopped: 2 s doubling to 16 s */
static int bt_set_slow(int playing)
{
    bt_set_t s;
    bt_set_get(&s);
    return s.rate == BT_RATE_SLOW || (s.rate == BT_RATE_AUTO && playing);
}

/* GLO > MIDI's SYNC IN (the core's m2k_bt_sync): OFF and ON are the core's own (S_SYNI: a clock in followed or
 * not), BLE (the BLE device's clock, start and stop only) and LINK (FM-Link's follower) are kept here; the core then
 * says ON for BLE and OFF for LINK (so the plain build, which reads S_SYNI, follows any clock or none) */
enum { BT_SYNC_AS_CORE = 0, BT_SYNC_BLE = 2, BT_SYNC_LINK = 3 };
static uint32_t bt_sync(void)
{
    uint32_t v = bts_get()->sync;
    return v == BT_SYNC_BLE || v == BT_SYNC_LINK ? v : BT_SYNC_AS_CORE;
}
int m2k_bt_sync(void) { return (int)bt_sync(); }   /* (the core: below 2, its own ON or OFF) */
void m2k_bt_sync_set(int v)
{
    bts_t *r = bts_get();
    uint8_t keep = (uint8_t)(v == BT_SYNC_BLE || v == BT_SYNC_LINK ? v : 0);
    if (r->sync != keep) {
        r->sync = keep;
        bts_changed();
    }
}
/* the MIDI in (take_panel): a realtime byte (clock, start, continue, stop) from USB / TRS (ble 0) or the BLE device
 * (ble 1) handed to the core? SYNC IN BLE: the BLE device's only; LINK: none (FM-Link leads); else every source's, as
 * the plain build (the core follows them as its own SYNC IN says). Any other message: yes */
static int bt_rt_ok(uint32_t st, int ble)
{
    uint32_t v;
    if (st != 0xF8u && st != 0xFAu && st != 0xFBu && st != 0xFCu)
        return 1;
    v = bt_sync();
    return v == BT_SYNC_AS_CORE || (v == BT_SYNC_BLE && ble);
}
