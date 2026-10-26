/* SPDX-License-Identifier: GPL-3.0-only
 * FM-Link: 2026 DEADACTIVE (Felucca's tests/bt_fml_test.c, as OberVave's, on MONO2K) */
/* FM-Link's glue (firmware/fm1/bt_fml.c) on the host: this FM-1's core (its sequencer through m2k_bt_block, each
 * control block as m2k_render runs it, heard 256 frames on; the main loop's bt_fml_frame, core_fml_rx) against another
 * FM-1 that is the engine alone (fmlink.c), its own crystal and tick count, joined by a link that delivers at 15 ms
 * connection events. The engine's accuracy over a bad link is tests/fmlink_test.c's; here: the roles from SYNC IN
 * (OFF and ON without a clock lead, LINK follows, BLE none), PLAY / STOP / tempo through the panel's PLAY (the core's
 * held request), the follower's tempo from the leader, its own PLAY / STOP held, the sequencer itself in time with
 * FM-Link's count (MONO2K's sequencer advances by the sample: within a frame or two), LINK without a link as OFF, a
 * message kept while the BLE unit has no room. */
#include "bt_test.h"

#define EV (15u * 24000u)                            /* the connection interval, ticks */
typedef struct { uint32_t at; int to_peer; uint8_t len, b[20]; } pk_t;
static pk_t Q[512];
static uint32_t qn, link_down;
static void link_put(int to_peer, const uint8_t *b, uint32_t len)
{
    if (link_down || qn >= 512u)
        return;
    Q[qn].at = (host_ticks / EV + 1u) * EV + 12000u;   /* the next connection event, then half a millisecond */
    Q[qn].to_peer = to_peer;
    Q[qn].len = (uint8_t)len;
    memcpy(Q[qn].b, b, len);
    qn++;
}
/* the other FM-1: the engine, its ticks = this one's + an offset, 40 ppm faster */
static struct { fml_t f; uint32_t off, bpm, playing, next, req, spb; } P;
static uint32_t peer_clk(void) { return host_ticks + P.off + (uint32_t)((uint64_t)host_ticks * 40u / 1000000u); }
static int said_lead, said_follow;
static uint32_t fml_room = 1000000u, fml_sent, blocks;
static int bt_send_hook(const uint8_t *p, uint32_t len)
{
    if (!fml_room)
        return 0;
    fml_room--;
    fml_sent++;
    link_put(1, p, len);
    return 1;
}
static uint64_t seq_pos, seq_acc;                    /* this FM-1's sequencer: beats (Q16) since its start */
static uint32_t spb(void) { return (sps_q8 * 4u) >> 8; }
static void step(void)                               /* one control block on this FM-1, the peer's blocks, the loops */
{
    uint32_t i, k = 0, len, seq_n;
    uint8_t b[20];
    host_ticks += M2K_SUB * 544u;
    fm1_ms = 0;                                      /* (fm1_ticks = host_ticks) */
    for (i = 0; i < qn; i++) {
        if ((int32_t)(host_ticks - Q[i].at) >= 0) {
            if (Q[i].to_peer)
                fml_rx(&P.f, Q[i].b, Q[i].len, peer_clk());
            else
                bt_fml_rx(Q[i].b, Q[i].len, host_ticks);   /* (core_fml_rx) */
        } else {
            Q[k++] = Q[i];
        }
    }
    qn = k;
    bt_blk_t = host_ticks + 256u * 544u;             /* (m2k_fm1.c bt_heard: rendered ahead of the speaker) */
    seq_block();                                     /* (m2k_render's, each control block) */
    if (m2k_playing()) {
        seq_acc += (uint64_t)bt_adv * 65536u;
        seq_pos += seq_acc / spb();
        seq_acc %= spb();
    } else {
        seq_pos = seq_acc = 0;
    }
    while ((int32_t)(peer_clk() + 256u * 544u - P.next) >= 0) {
        uint32_t bpm = P.f.role == FML_FOLLOWER && P.f.l_valid ? P.f.l_bpm : P.bpm;
        P.spb = 60u * M2K_SR / bpm;
        i = fml_block(&P.f, P.next, 32, bpm, P.spb, (int)P.playing, P.req, &seq_n);
        P.req = 0;
        if (i == 1u || i == 3u)
            P.playing = 1;
        else if (i == 2u)
            P.playing = 0;
        P.next += 32u * 544u;
    }
    if (++blocks % 16u == 0u) {                      /* (the main loop: every ~6 ms) */
        bs_fml_ready = !link_down;
        bt_fml_frame();                              /* (bt_service) */
        bs_fml_ready = 1;
    }
    while ((len = fml_poll(&P.f, peer_clk(), b)) != 0u)
        link_put(0, b, len);
    said_lead |= str_eq(ui.toast, "FM-LINK") && str_eq(ui.toast2, "LEADING");
    said_follow |= str_eq(ui.toast, "FM-LINK") && str_eq(ui.toast2, "FOLLOWING");
}
static void run_ms(uint32_t ms)
{
    uint32_t end = host_ticks + ms * 24000u;
    while ((int32_t)(host_ticks - end) < 0)
        step();
}
static double apart_ms(void)                         /* |this FM-1's beats - the peer's| now, in ms */
{
    double q = (double)spb();
    double a = (double)bt_fml.last_beats / 65536.0 + (double)(int32_t)(host_ticks - bt_fml.last_t) / (544.0 * q);
    double b = (double)P.f.last_beats / 65536.0 + (double)(int32_t)(peer_clk() - P.f.last_t) / (544.0 * P.spb);
    double d = (a - b) * 60000.0 / (double)seq_bpm();
    return d < 0 ? -d : d;
}
static int32_t seq_off(void) { return (int32_t)(((int64_t)seq_pos - (int64_t)bt_fml.beats) * (int64_t)spb() / 65536); }
static double worst_ms(uint32_t ms, int32_t *seq_worst)
{
    double w = 0, d;
    uint32_t i;
    for (i = 0; i < ms / 10u; i++) {
        int32_t s;
        run_ms(10);
        d = apart_ms();
        w = d > w ? d : w;
        s = seq_off();
        s = s < 0 ? -s : s;
        if (s > *seq_worst)
            *seq_worst = s;
    }
    return w;
}
static void fresh(int sync, uint32_t peer_role)     /* power-on, MIDI mode, linked to the other FM-1 */
{
    bt_set_t s;
    bt_fresh(0);
    memset(&s, 0, sizeof s);
    s.mode = BT_MODE_MIDI;
    bt_set_put(&s);
    bt_sync_put(sync);
    seq_pos = seq_acc = 0;
    qn = 0;
    link_down = 0;
    fml_room = 1000000u;
    host_ticks = 123456789u;
    PAT(cur_pat)->glob[G_TMPO] = 97 - 30;            /* (97 BPM) */
    memset(&P, 0, sizeof P);
    fml_init(&P.f, peer_role);
    P.off = 0x9E3779B9u;
    P.bpm = 120;
    P.next = peer_clk() + 256u * 544u;
    bt_cpy(conn_name, "Felucca FM-1", sizeof conn_name);
    conn_up = 1;
    bs_fml_ready = 1;
    bt_link(1);
    said_lead = said_follow = 0;
    ui.toast[0] = ui.toast2[0] = 0;
}
static void press_play(void) { m2k_button(B_PLAY, 1); m2k_button(B_PLAY, 0); }   /* the panel's PLAY / STOP */

int main(void)
{
    double e;
    int32_t sw = 0;
    uint32_t i;
    fml_send_hook = bt_send_hook;
    /* this FM-1 follows (SYNC IN LINK), the other leads */
    fresh(3, FML_LEADER);
    run_ms(3000);
    ok("SYNC IN LINK: the follower's role", bt_fml.role == FML_FOLLOWER);
    ok("the follower hears the leader and knows the offset", fml_linked(&bt_fml, host_ticks) && bt_fml.off_ok);
    ok("it says FM-LINK: FOLLOWING", said_follow && !said_lead);
    P.req = 1;                                       /* PLAY on the leader */
    run_ms(5);
    ok("the leader announces its start (both 80 ms on)", !P.playing && !m2k_playing());
    run_ms(300);
    ok("both play", P.playing && m2k_playing());
    ok("the follower's tempo is the leader's (from 97)", seq_bpm() == 120u);
    e = worst_ms(20000, &sw);
    printf("      FM-Link's count against the leader: %.3f ms at most over 20 s; the sequencer within %d frames of "
           "it\n", e, sw);
    ok("within 1.5 ms over 20 s, still playing", e <= 1.5 && m2k_playing());
    ok("the sequencer itself within 2 frames of FM-Link's count", sw <= 2);
    press_play();                                    /* its own STOP */
    run_ms(50);
    ok("a linked follower's own STOP does nothing and says FOLLOWING LINK", m2k_playing() &&
       str_eq(ui.toast, "FOLLOWING LINK"));
    P.bpm = 133;                                     /* the leader's tempo changes */
    run_ms(1000);
    ok("the follower takes the leader's new tempo", seq_bpm() == 133u);
    sw = 0;
    e = worst_ms(5000, &sw);
    printf("      at the new tempo: %.3f ms, the sequencer within %d frames\n", e, sw);
    ok("in time at the new tempo", e <= 1.5 && sw <= 2);
    link_down = 1;                                   /* the link lost: it plays on */
    run_ms(3000);
    ok("the link lost: the follower plays on at the leader's last tempo", m2k_playing() && seq_bpm() == 133u);
    link_down = 0;
    run_ms(3000);
    P.req = 2;                                       /* STOP on the leader */
    run_ms(200);
    ok("STOP on the leader stops the follower", !P.playing && !m2k_playing());

    /* this FM-1 leads (SYNC IN OFF), the other follows */
    fresh(0, FML_FOLLOWER);
    PAT(cur_pat)->glob[G_TMPO] = 110 - 30;
    run_ms(3000);
    ok("SYNC IN OFF: the leader's role, FM-LINK: LEADING", bt_fml.role == FML_LEADER && said_lead);
    ok("the leader keeps its own tempo", seq_bpm() == 110u);
    press_play();
    run_ms(5);
    ok("PLAY with a follower linked: not at once", !m2k_playing());
    run_ms(300);
    ok("both play, the follower at the leader's tempo", m2k_playing() && P.playing && P.f.l_bpm == 110u);
    sw = 0;
    e = worst_ms(10000, &sw);
    printf("      the follower against the core's sequencer: %.3f ms at most over 10 s\n", e);
    ok("within 1.5 ms", e <= 1.5 && sw <= 2);
    press_play();
    run_ms(200);
    ok("STOP stops both", !m2k_playing() && !P.playing);
    fml_room = 0;                                    /* the BLE unit has no room: a message is kept, not lost */
    for (i = 0; i < 600u && !bt_fo.len; i++)
        run_ms(1);
    ok("no room: one message waits", bt_fo.len != 0u);
    fml_sent = 0;
    fml_room = 1000000u;
    run_ms(8);
    ok("room again: it goes first", fml_sent != 0u && bt_fo.len == 0u);
    link_down = 1;                                   /* the follower gone: PLAY at once again */
    run_ms(2500);
    press_play();
    run_ms(2);
    ok("no follower: PLAY starts at once", m2k_playing());

    /* SYNC IN ON without a MIDI clock leads too; following one, no part */
    fresh(1, FML_FOLLOWER);
    run_ms(100);
    ok("SYNC IN ON, no MIDI clock: the leader's role", bt_fml.role == FML_LEADER);
    m2k_midi(0xFA, 0, 0);                            /* a MIDI clock's Start: the core plays to it */
    run_ms(100);
    ok("SYNC IN ON following a MIDI clock: no FM-Link role", m2k_ext_clock() && bt_fml.role == FML_NONE);
    m2k_midi(0xFC, 0, 0);

    /* SYNC IN LINK without a link: as OFF */
    fresh(3, FML_LEADER);
    link_down = 1;
    run_ms(100);
    press_play();
    run_ms(2);
    ok("SYNC IN LINK, no other FM-1: PLAY starts at once, at its own tempo", m2k_playing() && seq_bpm() == 97u);
    run_ms(2000);
    ok("... and keeps playing", m2k_playing());

    /* SYNC IN BLE: no part in FM-Link */
    fresh(2, FML_LEADER);
    run_ms(100);
    ok("SYNC IN BLE: no FM-Link role", bt_fml.role == FML_NONE);
    press_play();
    run_ms(2);
    ok("SYNC IN BLE: the panel's PLAY at once (FM-Link passes it on)", m2k_playing());
    printf(fails ? "bt_fml: %d FAILED\n" : "bt_fml: all checks passed\n", fails);
    return fails != 0;
}
