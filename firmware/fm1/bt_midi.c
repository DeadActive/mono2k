/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE (from Felucca's ble-kit bt_midi.c, as OberVave's)
 * FM-Link: 2026 DEADACTIVE */
/* FELUCCA_BLE (m2k_fm1.c includes it after bt_set.c): Bluetooth MIDI on MONO2K.
 * In: each USB-MIDI packet the BLE unit received goes to the SMC-Mixer layout (SMC DAW mode: the core's m2k_smc_in)
 *   or, in MIDI mode, into a ring of its own, which take_panel reads after USB's and TRS's queue and hands to the core
 *   (m2k_midi) as theirs: the tracks' notes by channel, the Monomachine's CC map, and the realtime bytes as SYNC IN
 *   says (bt_set.c bt_rt_ok: BLE follows this device's only, LINK none).
 * Out, MIDI mode: what the core sends to USB MIDI out (GLO > MIDI: the clock, start and stop, the tracks' notes, each
 *   once the sound it belongs to plays: midi_drain) goes to the device too (bt_out_mirror), the clock but while the
 *   core plays to the device's own clock (no loop).
 * FM-LINK mode: no MIDI either way: the link carries FM-Link only (bt_fml.c runs in every mode).
 * The link down: the notes the device still held are let go (a note off each, into the ring), BT_REL_MS after the
 *   drop, so a note on it sent just before has reached the core first. Main loop only. */
#define BT_MIDI_C 1
#define BT_OUT_Q 32u
#define BT_RX_Q 32u
#define BT_HELD 16u                                  /* notes held by the device, for the release after a drop */
#define BT_REL_MS 15u
static struct {
    uint32_t drops;                                  /* MIDI mode: packets refused, the ring full */
    uint32_t out_drops;                              /* MIDI out: messages lost to a full queue here */
    uint8_t held[BT_HELD][2];                        /* (channel | 0x80, note): held; 0 a free entry */
    uint8_t release;                                 /* the link dropped: its held notes still to let go */
    uint32_t rel_ms;
    uint8_t smc;                                     /* the linked device's name starts with "SMC" (bt_link) */
    uint8_t up;                                      /* linked (subscribed to the device's BLE-MIDI) */
    uint8_t out_on;                                  /* MIDI out wanted: linked, MIDI mode (bt_midi_frame) */
    uint8_t smc_up;                                  /* the core's SMC layout told it is linked (m2k_smc_link) */
} bt_in;
static uint32_t bt_out_q[BT_OUT_Q], bt_out_w, bt_out_r;
static uint32_t bt_rx_q[BT_RX_Q], bt_rx_w, bt_rx_r;  /* the device's MIDI for take_panel */

/* 1 = the SMC-Mixer DAW layout, 0 = MIDI: MODE, or AUTO by the connected device's name */
static uint32_t bt_mode_now(void)
{
    bt_set_t s;
    bt_set_get(&s);
    if (s.mode != BT_MODE_AUTO)
        return s.mode == BT_MODE_SMC;
    return bt_in.smc;
}
static int bt_link_only(void)                        /* MODE FM-LINK: no MIDI either way */
{
    bt_set_t s;
    bt_set_get(&s);
    return s.mode == BT_MODE_LINK;
}

/* core_ble_link: the link up (the connected device's name read once) / down */
static void bt_link(int up)
{
    char n[24];
    bt_in.up = (uint8_t)(up != 0);
    bt_in.smc = (uint8_t)(up && ble_ui_conn_name(n) && n[0] == 'S' && n[1] == 'M' && n[2] == 'C');
    bt_in.out_on = (uint8_t)(up && !bt_mode_now() && !bt_link_only());
    if (!up) {
        bt_in.release = 1;                           /* (bt_midi_frame: into the ring when it has room) */
        bt_in.rel_ms = fm1_ms;
        bt_out_r = bt_out_w;                         /* nothing queued for a device that is gone */
    }
}

/* the realtime F8 FA FB FC (as their status), channel voice with CIN = the status' high nibble and data bytes < 0x80
 * (Cx Dx: one); 0 = dropped (as usb.c's filter) */
static uint32_t bt_midi_filter(uint32_t pkt)
{
    uint32_t cin = pkt & 15u, st = (pkt >> 8) & 0xFFu;
    if (cin == 0xFu)
        return st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu ? 0xFu | st << 8 : 0u;
    if (cin >= 8u && cin <= 0xEu && (st >> 4) == cin && !((pkt >> 16) & 0x80u) &&
        ((cin == 0xCu || cin == 0xDu) || !(pkt & 0x80000000u)))
        return pkt;
    return 0;
}

static int bt_rx_put(uint32_t pkt)                   /* 0 = the ring is full */
{
    if (bt_rx_w - bt_rx_r >= BT_RX_Q)
        return 0;
    bt_rx_q[bt_rx_w % BT_RX_Q] = pkt;
    bt_rx_w++;
    return 1;
}
/* take_panel: the next message from the BLE device (after USB / TRS's); 1 = one */
static int bt_rx_get(uint8_t *st, uint8_t *d1, uint8_t *d2)
{
    uint32_t pk;
    if (bt_rx_r == bt_rx_w)
        return 0;
    pk = bt_rx_q[bt_rx_r % BT_RX_Q];
    bt_rx_r++;
    *st = (uint8_t)(pk >> 8);
    *d1 = (uint8_t)(pk >> 16);
    *d2 = (uint8_t)(pk >> 24);
    return 1;
}

static void bt_held_note(uint32_t ch, uint32_t note, int on)   /* the device's notes held, as it plays them */
{
    uint32_t i, fr = BT_HELD;
    for (i = 0; i < BT_HELD; i++) {
        if (bt_in.held[i][0] == (0x80u | ch) && bt_in.held[i][1] == note) {
            if (!on)
                bt_in.held[i][0] = 0;
            return;
        }
        if (!bt_in.held[i][0] && fr == BT_HELD)
            fr = i;
    }
    if (on && fr < BT_HELD) {
        bt_in.held[fr][0] = (uint8_t)(0x80u | ch);
        bt_in.held[fr][1] = (uint8_t)note;
    }
}

static void bt_midi_in(uint32_t pkt)
{
    uint32_t cin, st;
    if (bt_link_only())
        return;                                      /* FM-LINK: MIDI from the device ignored */
    if (bt_mode_now()) {
        m2k_smc_in(pkt);
        return;
    }
    pkt = bt_midi_filter(pkt);
    if (!pkt)
        return;
    if (!bt_rx_put(pkt)) {
        bt_in.drops++;
        return;
    }
    cin = pkt & 15u;
    st = (pkt >> 8) & 0xFFu;
    if (cin == 9u || cin == 8u)
        bt_held_note(st & 15u, (pkt >> 16) & 0x7Fu, cin == 9u && (pkt >> 24));
}

static void bt_out_push(uint32_t pkt)               /* the out queue (main loop); full: dropped and counted */
{
    if (bt_out_w - bt_out_r >= BT_OUT_Q) {
        bt_in.out_drops++;
        return;
    }
    bt_out_q[bt_out_w % BT_OUT_Q] = pkt;
    bt_out_w++;
}

/* midi_drain: the core's MIDI out, as it goes to USB, to the BLE device too in MIDI mode; the realtime bytes not while
 * the core plays to the BLE device's own clock (SYNC IN BLE: it would hear its clock back) */
static void bt_out_mirror(const uint8_t *m, int n)
{
    uint32_t st = m[0];
    if (!bt_in.out_on || n < 1)
        return;
    if (st >= 0xF8u) {
        if (bt_sync() == BT_SYNC_BLE && m2k_ext_clock())
            return;
        bt_out_push(0xFu | st << 8);
        return;
    }
    bt_out_push((st >> 4) | st << 8 | (n > 1 ? (m[1] & 0x7Fu) << 16 : 0u) | (n > 2 ? (uint32_t)(m[2] & 0x7Fu) << 24 : 0u));
}

/* the core's SMC layout: m2k_bt_send (smc frames) */
int m2k_bt_send(const uint8_t *m, int n)
{
    return ble_midi_room() && ble_midi_send(m, (uint32_t)n) == 1;
}

/* the main loop (bt_service): the releases owed after a dropped link, the SMC layout's link, then the out queue to
 * the BLE unit while it has room (ble_midi_send: main loop only) */
static void bt_midi_frame(void)
{
    uint32_t i;
    int smc;
    if (bt_in.release && fm1_ms - bt_in.rel_ms >= BT_REL_MS) {
        for (i = 0; i < BT_HELD; i++)
            if (bt_in.held[i][0]) {
                if (!bt_rx_put(0x08u | (0x80u | (bt_in.held[i][0] & 15u)) << 8 | (uint32_t)bt_in.held[i][1] << 16))
                    break;                           /* still owed: the next pass */
                bt_in.held[i][0] = 0;
            }
        if (i == BT_HELD)
            bt_in.release = 0;
    }
    bt_in.out_on = (uint8_t)(bt_in.up && !bt_mode_now() && !bt_link_only());
    smc = bt_in.up && bt_mode_now() && !bt_link_only();
    if (smc != bt_in.smc_up) {                       /* linked in SMC DAW mode, or no longer */
        bt_in.smc_up = (uint8_t)smc;
        m2k_smc_link(smc);
    }
    if (bt_in.up)
        m2k_smc_frame();                             /* (SMC: its LEDs and faders; out of it: its LEDs off once) */
    while (bt_out_r != bt_out_w && ble_midi_room()) {
        uint32_t pkt = bt_out_q[bt_out_r % BT_OUT_Q], cin = pkt & 15u;
        uint8_t m[3] = {(uint8_t)(pkt >> 8), (uint8_t)(pkt >> 16), (uint8_t)(pkt >> 24)};
        if (!ble_midi_send(m, cin == 0xFu ? 1u : cin == 0xCu || cin == 0xDu ? 2u : 3u))
            break;
        bt_out_r++;
    }
}
