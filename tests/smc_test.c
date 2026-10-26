/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE (Felucca's tests/smc_test.c, as OberVave's, on MONO2K) */
/* The SMC-Mixer DAW layout on MONO2K's six tracks (the core's m2k_bt.c, through the glue's bt_midi.c) on the host:
 * faders 1-6 the tracks' LEVEL with pickup (7, 8 nothing), knobs 1-6 PAN, 7 the tempo, 8 the swing, the buttons (REC
 * SOLO MUTE SELECT for strips 1-6, the transport through FM-Link's held PLAY, track < >), the LEDs and fader positions
 * sent (only changes, all on a new link), the layout left (its LEDs off). */
#include "bt_test.h"

static void smc_pkt(uint8_t st, uint8_t d1, uint8_t d2)
{
    bt_midi_in((uint32_t)(st >> 4) | (uint32_t)st << 8 | (uint32_t)d1 << 16 | (uint32_t)d2 << 24);
}
static void fader(uint32_t n, uint32_t v) { smc_pkt((uint8_t)(0xE0u + n), (uint8_t)((v << 7) & 0x7Fu), (uint8_t)v); }
static void knob(uint32_t k, int32_t clicks) { smc_pkt(0xB0, (uint8_t)(16u + k), (uint8_t)(clicks < 0 ? 0x40 | -clicks : clicks)); }
static void button(uint32_t note) { smc_pkt(0x90, (uint8_t)note, 127); smc_pkt(0x80, (uint8_t)note, 0); }
static uint32_t led(uint32_t note)                   /* the last value sent for an LED, 0xFF none */
{
    uint32_t i, v = 0xFFu;
    for (i = 0; i < host_ble_out_n && i < 256u; i++)
        if (host_ble_out[i][0] == 0x90 && host_ble_out[i][1] == note)
            v = host_ble_out[i][2];
    return v;
}
static void block(void) { seq_block(); }             /* a control block: the held PLAY / STOP acted on */

int main(void)
{
    uint32_t n0;
    int lv;
    bt_fresh(0);
    {
        bt_set_t s;
        bt_set_get(&s);
        s.mode = BT_MODE_AUTO;
        bt_set_put(&s);
    }
    bt_cpy(conn_name, "SMC-Mixer", sizeof conn_name);
    conn_up = 1;
    bt_link(1);
    bt_midi_frame();                                 /* (the layout told it is linked; its first frame) */
    ok("the mixer by its name: SMC DAW mode, the layout linked", bt_mode_now() && bt_in.smc_up && smc.up);
    ok("a new link: every LED and every fader position sent", host_ble_out_n >= 35u + 8u &&
       led(0x18) == 127u && led(0x19) == 0u && led(0x5D) == 127u && led(0x5E) == 0u);
    lv = PAT(cur_pat)->kit[0][P_LEVL];
    ok("fader positions: the tracks' LEVEL on 1-6, 0 on 7, 8", ble_out_find(0xE0, (uint8_t)lv, (uint8_t)lv) == 1u &&
       ble_out_find(0xE6, 0, 0) == 1u && ble_out_find(0xE7, 0, 0) == 1u);
    n0 = host_ble_out_n;
    bt_midi_frame();
    ok("nothing changed: nothing sent", host_ble_out_n == n0);

    /* faders, with pickup */
    PAT(cur_pat)->kit[0][P_LEVL] = 100;
    bt_midi_frame();
    fader(0, 40);
    ok("fader 1 far from LEVEL 100: not picked up", PAT(cur_pat)->kit[0][P_LEVL] == 100u);
    fader(0, 99);
    ok("fader 1 within 2 of it: picked up, track 1's LEVEL follows", PAT(cur_pat)->kit[0][P_LEVL] == 99u &&
       trk[0].eff[P_LEVL] == 99u);
    fader(0, 60);
    ok("then it follows freely", PAT(cur_pat)->kit[0][P_LEVL] == 60u);
    PAT(cur_pat)->kit[5][P_LEVL] = 20;
    bt_midi_frame();
    fader(5, 30);
    fader(5, 10);
    ok("fader 6: crossing its value picks it up", PAT(cur_pat)->kit[5][P_LEVL] == 10u);
    fader(6, 90);
    ok("fader 7: nothing (six tracks)", 1);
    PAT(cur_pat)->kit[0][P_LEVL] = 110;              /* the panel moved it */
    n0 = host_ble_out_n;
    bt_midi_frame();
    ok("moved on the FM-1: its position sent, unpicked", ble_out_find(0xE0, 110, 110) == 1u);
    fader(0, 60);
    ok(".. so the fader far from it does nothing", PAT(cur_pat)->kit[0][P_LEVL] == 110u);

    /* knobs */
    PAT(cur_pat)->kit[2][P_PAN] = 64;
    knob(2, -5);
    ok("knob 3 left: track 3's PAN, said", PAT(cur_pat)->kit[2][P_PAN] == 59u && trk[2].eff[P_PAN] == 59u &&
       str_eq(ui.toast, "T3 PAN") && str_eq(ui.toast2, "L5"));
    {
        uint32_t b = seq_bpm();
        knob(6, 3);
        ok("knob 7: the tempo, a BPM a click", seq_bpm() == b + 3u);
    }
    PAT(cur_pat)->glob[G_SWNG] = 10;
    knob(7, 4);
    ok("knob 8: the swing", PAT(cur_pat)->glob[G_SWNG] == 14u);

    /* buttons */
    button(0x11);
    ok("MUTE 2: track 2 muted", (PAT(cur_pat)->mute >> 1) & 1u);
    button(0x11);
    ok("MUTE 2 again: heard", !((PAT(cur_pat)->mute >> 1) & 1u));
    button(0x0C);
    ok("SOLO 5: track 5 alone", sq.solo == 1u << 4);
    button(0x0C);
    ok("SOLO 5 again: off", !sq.solo);
    button(0x1B);
    ok("SELECT 4: track 4", ui.trk == 3);
    button(0x1E);
    ok("SELECT 7: nothing (six tracks)", ui.trk == 3);
    button(0x2F);
    ok("CHANNEL >: the next track", ui.trk == 4);
    button(0x2E);
    button(0x2E);
    ok("BANK < twice: two back", ui.trk == 2);
    button(0x01);
    ok("REC 2: track 2 selected, REC on (as the panel's tap)", ui.trk == 1 && sq.rec);
    button(0x5F);
    ok("REC: off again", !sq.rec);
    button(0x5E);
    ok("PLAY: held for FM-Link, the next block", !m2k_playing());
    block();
    ok(".. playing", m2k_playing());
    n0 = host_ble_out_n;
    bt_midi_frame();
    ok("the transport LEDs: PLAY lit, STOP dark", led(0x5E) == 127u && led(0x5D) == 0u);
    button(0x5D);
    block();
    ok("STOP: stopped", !m2k_playing());
    PAT(cur_pat)->mute = 1u << 2;
    sq.solo = 1u << 3;
    bt_midi_frame();
    ok("the LEDs follow: MUTE 3, SOLO 4, SELECT 2", led(0x12) == 127u && led(0x0B) == 127u && led(0x19) == 127u &&
       led(0x18) == 0u);
    sq.solo = 0;

    /* the layout left */
    {
        bt_set_t s;
        bt_set_get(&s);
        s.mode = BT_MODE_MIDI;
        bt_set_put(&s);
    }
    n0 = host_ble_out_n;
    bt_midi_frame();
    ok("MODE MIDI while linked: the layout's LEDs off once", led(0x12) == 0u && led(0x5D) == 0u && host_ble_out_n > n0);
    n0 = host_ble_out_n;
    bt_midi_frame();
    ok(".. once only", host_ble_out_n == n0);
    smc_pkt(0x90, 0x10, 127);
    ok("MIDI mode: the mixer's buttons are MIDI now, not the layout's", !(PAT(cur_pat)->mute & 1u));
    printf(fails ? "smc: %d FAILED\n" : "smc: all checks passed\n", fails);
    return fails != 0;
}
