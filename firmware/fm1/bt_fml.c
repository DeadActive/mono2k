/* SPDX-License-Identifier: GPL-3.0-only
 * FM-Link: 2026 DEADACTIVE (from Felucca's ble-kit bt_fml.c, as OberVave's) */
/* FELUCCA_BLE (m2k_fm1.c includes it after bt_midi.c): FM-Link's glue (Felucca's
 * docs/superpowers/specs/2026-10-21-fm-link-design.md): the engine (fmlink.c, Felucca's, unchanged: FM-1s on
 * different firmwares keep time with each other) with MONO2K's sequencer through the core's m2k_bt_block. MONO2K on
 * its own tempo leads (SYNC IN OFF, or ON with no MIDI clock coming in), SYNC IN LINK follows (the leader's tempo,
 * PLAY and STOP; its own PLAY / STOP say FOLLOWING LINK); on another clock (ON following one, BLE) it takes no part
 * (its HELLOs still say it is an FM-1).
 *   The core renders in the main loop, ahead of the speaker (m2k_fm1.c: a ring of RING blocks): each control block
 *   asks m2k_bt_block, with the TIMER4 tick the block will be heard at (bt_heard: the last audio half's tick and the
 *   ring's place then), what the transport does (a leader's PLAY with a follower linked: both start 80 ms later; a
 *   linked follower's own PLAY / STOP: the leader's) and how far the sequencer advances (a block, a sample more or
 *   less: the follower held on the leader). The main loop: bt_fml_frame (bt_service), the role from SYNC IN, the
 *   messages out; core_fml_rx: in (the follower's tempo set from the leader's at once). All in the main loop: the
 *   engine's words are shared with nothing else. */
#define BT_FML_C 1
#include "fmlink.c"
static fml_t bt_fml;
static struct {
    uint8_t b[20], len;                              /* a message ble_fml_send had no room for: first, next pass */
    uint32_t ms;                                     /* when it was made: dropped 50 ms on (a stale time or state) */
    uint8_t said;                                    /* the peer announced (a toast) while linked */
} bt_fo;

static uint32_t bt_fml_role(void)                    /* from SYNC IN, and whether a MIDI clock leads */
{
    uint32_t v = bt_sync();
    return v == BT_SYNC_LINK ? FML_FOLLOWER : v == BT_SYNC_AS_CORE && !m2k_ext_clock() ? FML_LEADER : FML_NONE;
}

#ifndef BT_HOST                                      /* (the host's tests set bt_blk_t themselves) */
/* render_ahead, before block w is rendered: the tick its first frame is heard at. The audio half that took block
 * r0 at tick t0 put it in the half the DMA plays next (HALF_FRAMES on) */
static void bt_heard(uint32_t w)
{
    uint32_t t0, r0, f = irq_save();
    t0 = bt_a_t0;
    r0 = bt_a_r0;
    irq_restore(f);
    bt_blk_t = t0 ? t0 + (HALF_FRAMES + (w - r0) * BLK) * FML_TPS : fm1_ticks();
}
#endif

/* the core, each control block (m2k_bt_block, m2k.h) */
uint32_t m2k_bt_block(uint32_t ui, int playing, uint32_t bpm, uint32_t spb, uint32_t *adv)
{
    uint32_t t = bt_blk_t, req, seq_n = M2K_SUB, role = bt_fml.role;
    bt_blk_t += M2K_SUB * FML_TPS;
    *adv = M2K_SUB;
    if (role == FML_NONE || role != bt_fml_role())   /* (SYNC IN changed: bt_fml_frame sets the engine up first) */
        return ui;
    req = fml_block(&bt_fml, t, M2K_SUB, bpm ? bpm : 120u, spb ? spb : 22050u, playing, ui, &seq_n);
    *adv = seq_n;
    return req;                                      /* (0 with ui: held, the leader's start waits, a follower's own
                                                      * PLAY / STOP is the leader's to decide) */
}

/* core_fml_rx (the BLE unit's ble_service): a message from the other FM-1 and the tick it came at */
static void bt_fml_rx(const uint8_t *p, uint32_t len, uint32_t t)
{
    fml_rx(&bt_fml, p, len, t);
    if (bt_fml.role == FML_FOLLOWER && bt_fml.l_valid && bt_fml.l_bpm >= 40u && bt_fml.l_bpm <= 240u)
        m2k_bt_tempo(bt_fml.l_bpm);                  /* (SYNC IN LINK: the leader's tempo) */
}

/* the main loop (bt_service): the role from SYNC IN, the messages out while linked */
static void bt_fml_frame(void)
{
    uint32_t role = bt_fml_role(), len;
    int linked;
    if (role != bt_fml.role) {
        fml_init(&bt_fml, role);
        bt_fo.len = 0;
        bt_fo.said = 0;
        if (role != FML_FOLLOWER)
            m2k_bt_tempo(0);                         /* (the pattern's own tempo again) */
    }
    if (bt_fml.msg_follow) {
        bt_fml.msg_follow = 0;
        m2k_toast("FOLLOWING LINK", "THE LEADER'S PLAY, STOP");
    }
    if (!ble_fml_ready()) {                          /* (FM-Link's own link, or the device's) */
        bt_fo.len = 0;
        return;
    }
    if (bt_fo.len && fm1_ms - bt_fo.ms > 50u)
        bt_fo.len = 0;
    for (;;) {
        if (!bt_fo.len) {
            len = fml_poll(&bt_fml, fm1_ticks(), bt_fo.b);
            if (!len)
                break;
            bt_fo.len = (uint8_t)len;
            bt_fo.ms = fm1_ms;
        }
        if (!ble_fml_send(bt_fo.b, bt_fo.len))
            break;                                   /* no room (or not an FM-1): this one first, next pass */
        bt_fo.len = 0;
    }
    linked = fml_linked(&bt_fml, fm1_ticks());
    if (linked && !bt_fo.said && role != FML_NONE)
        m2k_toast("FM-LINK", role == FML_LEADER ? "LEADING" : "FOLLOWING");
    bt_fo.said = (uint8_t)linked;
}
