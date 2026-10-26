/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE (Felucca's tests/bt_midi_test.c, as OberVave's, on MONO2K) */
/* Bluetooth MIDI on the host (firmware/fm1/bt_midi.c, bt_set.c, with MONO2K's core): the settings bytes, MIDI mode
 * in through its own ring into the core (m2k_midi, as take_panel hands it: the tracks' notes by channel, after USB's),
 * SMC DAW mode to the core's layout, FM-LINK mode nothing; the realtime bytes by SYNC IN (OFF / ON: every source's,
 * BLE: the device's only, LINK: none); the notes the device held let go on a lost link; the core's MIDI out (its
 * clock, start and stop, the tracks' notes) mirrored to the device, but its clock while it plays to the device's. */
#include "bt_test.h"

static int16_t abuf_[2 * 128];
static void take_ble(void)                           /* take_panel's BLE part */
{
    uint8_t st, d1, d2;
    while (bt_rx_get(&st, &d1, &d2))
        if (bt_rt_ok(st, 1))
            m2k_midi(st, d1, d2);
}
static void take_usb(uint8_t st, uint8_t d1, uint8_t d2)   /* a message from USB / TRS, as take_panel hands it */
{
    if (bt_rt_ok(st, 0))
        m2k_midi(st, d1, d2);
}
static void render(uint32_t blocks)                  /* the main loop's render, and midi_drain's mirror */
{
    uint8_t m[3];
    int n;
    while (blocks--) {
        m2k_render(abuf_, 128);
        while ((n = m2k_midi_out(m, m2k_samples())) > 0)
            bt_out_mirror(m, n);
    }
}
static void ble_in(uint8_t st, uint8_t d1, uint8_t d2)   /* a message from the BLE device (core_ble_midi_in) */
{
    bt_midi_in((uint32_t)(st >> 4) | (uint32_t)st << 8 | (uint32_t)d1 << 16 | (uint32_t)d2 << 24);
}
static void ble_rt(uint8_t st) { bt_midi_in(0xFu | (uint32_t)st << 8); }
static void link_up(const char *name, uint32_t mode)
{
    bt_set_t s;
    bt_set_get(&s);
    s.mode = (uint8_t)mode;
    bt_set_put(&s);
    bt_cpy(conn_name, name, sizeof conn_name);
    conn_up = 1;
    bt_link(1);
}
static int holds(int t, int note) { return trk[t].gate && trk[t].note == note; }

int main(void)
{
    bt_set_t s;
    uint32_t i, n0, f8;
    int ch0;

    bt_fresh(0);
    bt_set_get(&s);
    ok("settings: none saved -> MODE AUTO, AUTOSTART OFF, ROLE HOST, RECONNECT AUTO",
       s.mode == BT_MODE_AUTO && !s.autostart && !s.role && s.rate == BT_RATE_AUTO);
    s.mode = BT_MODE_MIDI;
    s.autostart = 1;
    s.role = 1;
    s.rate = BT_RATE_SLOW;
    bt_set_put(&s);
    memset(&s, 0, sizeof s);
    bt_set_get(&s);
    ok("settings: MODE, AUTOSTART, ROLE, RECONNECT read back", s.mode == BT_MODE_MIDI && s.autostart && s.role &&
       s.rate == BT_RATE_SLOW);
    ok("settings: RECONNECT SLOW rests long, also stopped", bt_set_slow(0));
    s.rate = BT_RATE_AUTO;
    bt_set_put(&s);
    ok("settings: RECONNECT AUTO: long while playing, short stopped", bt_set_slow(1) && !bt_set_slow(0));
    bts.set[7] ^= 1u;
    bt_set_get(&s);
    ok("settings: a damaged check byte -> defaults", s.mode == BT_MODE_AUTO && !s.autostart);

    /* MIDI mode in: the device plays the tracks by channel, as USB does */
    bt_fresh(0);
    ch0 = midi_ch();
    link_up("Xkey Air 25", BT_MODE_AUTO);
    ok("AUTO: a device not named SMC -> MIDI mode, MIDI out on", !bt_mode_now() && bt_in.out_on);
    ble_in((uint8_t)(0x90 | ((ch0 + 2) & 15)), 60, 100);
    ok("MIDI in: queued in the BLE ring", bt_rx_w - bt_rx_r == 1u);
    take_ble();
    ok("MIDI in: the core takes it, track 3's note held", holds(2, 60) && bt_rx_r == bt_rx_w);
    ble_in((uint8_t)(0x80 | ((ch0 + 2) & 15)), 60, 0);
    take_ble();
    ok("MIDI in: its note-off ends it", !trk[2].gate);
    ble_in((uint8_t)(0xB0 | ((ch0 + 1) & 15)), 7, 33);   /* CC 7: track 2's LEVEL (the Monomachine's map) */
    take_ble();
    ok("MIDI in: a CC is the core's (CC 7 the track's LEVEL)", PAT(cur_pat)->kit[1][P_LEVL] == 33);
    ble_in((uint8_t)(0x90 | ((ch0 + 3) & 15)), 62, 100);
    take_usb((uint8_t)(0x90 | ((ch0 + 4) & 15)), 64, 90);
    take_ble();
    ok("MIDI in: USB's and the device's both", holds(3, 62) && holds(4, 64));
    for (i = 0; i < 40u; i++)
        ble_in((uint8_t)(0x90 | ch0), 70, 1);
    ok("MIDI in: a full ring drops (counted), never blocks", bt_in.drops == 40u - BT_RX_Q);
    take_ble();
    bt_in.drops = 0;
    ble_in(0xF0, 1, 2);                             /* SysEx and the like: dropped by the filter */
    ok("MIDI in: anything but channel voice and F8 FA FB FC is dropped", bt_rx_w == bt_rx_r);

    /* the link lost: the notes the device held are let go */
    bt_fresh(0);
    link_up("Xkey Air 25", BT_MODE_MIDI);
    ble_in((uint8_t)(0x90 | ((ch0 + 5) & 15)), 65, 100);
    take_ble();
    ok("link lost: (track 6 holds its note)", holds(5, 65));
    conn_up = 0;
    bt_link(0);
    bt_midi_frame();
    ok("link lost: nothing at once (the device's last note reaches the core first)", bt_rx_w == bt_rx_r);
    fm1_ms += BT_REL_MS;
    bt_midi_frame();
    ok("link lost: a note off for the note it held", bt_rx_w - bt_rx_r == 1u &&
       bt_rx_q[bt_rx_r % BT_RX_Q] == (0x08u | (0x80u | ((ch0 + 5) & 15)) << 8 | 65u << 16));
    take_ble();
    ok("link lost: the note ends", !holds(5, 65));

    /* the realtime bytes by SYNC IN */
    bt_fresh(0);
    link_up("Xkey Air 25", BT_MODE_MIDI);
    bt_sync_put(0);                                 /* OFF: start and stop still drive the sequencer (the core's) */
    ble_rt(0xFA);
    take_ble();
    ok("SYNC IN OFF: the device's Start runs the sequencer", m2k_playing());
    ble_rt(0xFC);
    take_ble();
    ok("SYNC IN OFF: its Stop stops it", !m2k_playing());
    bt_sync_put(3);
    ble_rt(0xFA);
    take_ble();
    take_usb(0xFA, 0, 0);
    ok("SYNC IN LINK: every source's Start ignored (FM-Link leads)", !m2k_playing());
    bt_sync_put(2);
    take_usb(0xFA, 0, 0);
    ok("SYNC IN BLE: USB's Start ignored", !m2k_playing());
    ble_rt(0xFA);
    take_ble();
    ok("SYNC IN BLE: the device's Start runs it, to its clock", m2k_playing() && m2k_ext_clock());
    {                                               /* its clock at 100 BPM: 24 a quarter, the tempo followed */
        uint32_t per = 60u * M2K_SR / 100u / 24u, k, j;
        for (k = 0; k < 4u * 24u + 2u; k++) {
            ble_rt(0xF8);
            take_ble();
            for (j = 0; j < per; j += M2K_SUB) seq_block();
        }
        printf("      the device's clock at 100 BPM -> %u BPM\n", seq_bpm());
        ok("SYNC IN BLE: the device's clock sets the tempo (100 BPM)", seq_bpm() >= 99u && seq_bpm() <= 101u);
    }
    ble_rt(0xFC);
    take_ble();
    ok("SYNC IN BLE: the device's Stop stops it", !m2k_playing());
    bt_sync_put(0);

    /* SMC DAW and FM-LINK modes */
    bt_fresh(0);
    link_up("SMC-Mixer", BT_MODE_AUTO);
    ok("AUTO: a device named SMC... -> the SMC-Mixer layout, no MIDI out", bt_mode_now() && !bt_in.out_on);
    ble_in(0x90, 0x10, 127);                        /* MUTE 1 */
    ok("SMC DAW: its buttons go to the layout (MUTE 1), not into the ring", bt_rx_w == bt_rx_r && (PAT(cur_pat)->mute & 1u));
    bt_fresh(0);
    link_up("Xkey Air 25", BT_MODE_LINK);
    ble_in(0x90, 60, 100);
    ok("FM-LINK: the device's MIDI is dropped, nothing goes out", bt_rx_w == bt_rx_r && !bt_in.out_on);

    /* out: the core's MIDI out mirrored, in MIDI mode: the clock (SYNC OUT), start, stop, notes */
    bt_fresh(0);
    link_up("Xkey Air 25", BT_MODE_MIDI);
    m2k_setting(S_SYNO, 127);
    bt_sync_put(0);
    bt_fml_frame();                                 /* (the role: leader, no link) */
    n0 = host_ble_out_n;
    m2k_button(B_PLAY, 1);
    m2k_button(B_PLAY, 0);
    render(1);
    ok("MIDI out: PLAY (FM-Link passes it on unlinked: the next block) -> playing", m2k_playing());
    for (i = 0; i < (uint32_t)(4u * 60u * M2K_SR / seq_bpm() / 128u); i++) {   /* four beats of blocks */
        render(1);
        if (i % 4u == 0u)
            bt_midi_frame();
    }
    bt_midi_frame();
    f8 = ble_out_find(0xF8, 0, 0);
    ok("MIDI out: Start to the device on PLAY", ble_out_find(0xFA, 0, 0) == 1u && host_ble_out[n0 % 256u][0] == 0xFA);
    printf("      %u clock pulses in four beats\n", f8);
    ok("MIDI out: the core's clock, 24 a beat (96 in four, a pulse either way)", f8 >= 95u && f8 <= 97u);
    m2k_button(B_PLAY, 1);
    m2k_button(B_PLAY, 0);
    render(2);
    bt_midi_frame();
    ok("MIDI out: Stop on STOP", ble_out_find(0xFC, 0, 0) == 1u && !m2k_playing());
    host_ble_room = 0;
    {
        uint8_t m[3] = {0x90, 61, 99};
        bt_out_mirror(m, 3);
    }
    bt_midi_frame();
    ok("MIDI out: no room in the BLE unit -> kept, sent when there is", ble_out_find(0x90, 61, 99) == 0u &&
       bt_out_w - bt_out_r == 1u);
    host_ble_room = 64;
    bt_midi_frame();
    ok("MIDI out: .. then it goes", ble_out_find(0x90, 61, 99) == 1u);
    bt_sync_put(2);                                 /* following the device's clock: its clock not sent back */
    ble_rt(0xFA);
    take_ble();
    n0 = ble_out_find(0xF8, 0, 0);
    for (i = 0; i < 48u; i++) {
        ble_rt(0xF8);
        take_ble();
        render(1);
    }
    bt_midi_frame();
    ok("MIDI out: no clock back while it follows the device's (SYNC IN BLE): no loop", ble_out_find(0xF8, 0, 0) == n0);
    ble_rt(0xFC);
    take_ble();
    conn_up = 0;
    bt_link(0);
    {
        uint8_t m[3] = {0x90, 62, 99};
        bt_out_mirror(m, 3);
    }
    ok("MIDI out: not linked -> nothing queued", bt_out_w == bt_out_r);
    printf(fails ? "bt_midi: %d FAILED\n" : "bt_midi: all checks passed\n", fails);
    return fails != 0;
}
