/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE */
/* The core's side of the FM-1's Bluetooth build on the host (firmware/src/m2k_bt.c and the M2K_BT parts of the core):
 *   the machine marks run-length coded (tools/fm1/gen_icons_rle.py), every frame decoded back to ICON_BITS, byte for
 *   byte, and drawn the same;
 *   the glitch ring at half (GL_LEN 8192): every repeat's slice within half the ring and its run within what the ring
 *   keeps (bus_step at every size and the slowest tempo, PERFORM's repeats), the bus rendered with repeats on;
 *   the frame shown as its negative by the platform (m2k_frame_negative; the core no longer inverts a copy);
 *   GLO > MIDI's SYNC IN in four, as the core shows it;
 *   the page's keys only while it shows and GLO is not held. */
#include "bt_test.h"

int main(void)
{
    bt_fresh(0);

    /* the marks */
    {
        int bad = 0, m, f;
        for (m = 0; m < ICON_COUNT; m++)
            for (f = 0; f < ICON_FRAMES; f++)
                bad += memcmp(icon_frame(m, f), ICON_BITS[m][f], sizeof ICON_BITS[m][f]) != 0;
        ok("icons: every frame of every mark decodes to ICON_BITS, byte for byte", bad == 0);
        printf("      %u frames, %u B raw, %u B coded\n", (unsigned)(ICON_COUNT * ICON_FRAMES),
               (unsigned)sizeof ICON_BITS, (unsigned)ICON_RLE_BYTES);
        ok("icons: the coded table is the smaller (the flash's room: at least 40 KB less)",
           ICON_RLE_BYTES + sizeof ICON_RLE_AT + 40000u < sizeof ICON_BITS);
        ok("icons: the table's ends", ICON_RLE_AT[0] == 0u && ICON_RLE_AT[ICON_COUNT * ICON_FRAMES] == ICON_RLE_BYTES);
    }

    /* the glitch ring */
    ok("glitch: the Bluetooth build's ring is half (8192), a slice at most half of it", GL_LEN == 8192u && GL_SLICE == 4096u);
    {
        int bad = 0, n = 0;
        for (int tempo = 0; tempo <= 255; tempo += 15)
            for (int size = 0; size <= 127; size += 9) {
                PAT(cur_pat)->glob[G_TMPO] = (uint8_t)tempo;
                PAT(cur_pat)->glob[G_GLCH] = 127;
                PAT(cur_pat)->glob[G_GSIZ] = (uint8_t)size;
                seq_tempo();
                for (int k = 0; k < 64; k++) {
                    bus.g_on = 0;
                    bus_step();
                    if (!bus.g_on) continue;
                    n++;
                    if (bus.g_len < 64u || bus.g_len > GL_SLICE || bus.g_left > GL_LEN - bus.g_len - 256u) bad++;
                }
            }
        printf("      %d repeats started, every tempo and size\n", n);
        ok("glitch: every repeat's slice within half the ring, its run within what the ring keeps", n > 100 && bad == 0);
    }
    {
        int bad = 0;
        PAT(cur_pat)->glob[G_TMPO] = 0;              /* 30 BPM: a quarter is 44100 half-rate samples */
        seq_tempo();
        for (int k = PF_R4; k <= PF_STUT; k++) {
            bus_perform(k, 1);
            bad += bus.g_len > GL_SLICE;
            bus_perform(k, 0);
        }
        ok("glitch: PERFORM's repeats at 30 BPM within half the ring", bad == 0);
    }
    {
        static int16_t b[256];
        int32_t pk = 0;
        PAT(cur_pat)->glob[G_TMPO] = 90;
        PAT(cur_pat)->glob[G_GLCH] = 127;
        PAT(cur_pat)->glob[G_GSIZ] = 127;
        m2k_button(B_PLAY, 1);
        m2k_button(B_PLAY, 0);
        bus_perform(PF_R4, 1);
        for (int f = 0; f < M2K_SR * 4; f += 128) {
            m2k_render(b, 128);
            for (int i = 0; i < 256; i++) pk = b[i] > pk ? b[i] : pk;
        }
        bus_perform(PF_R4, 0);
        for (int f = 0; f < M2K_SR * 4; f += 128) m2k_render(b, 128);
        ok("glitch: bank A's first pattern rendered with repeats on (ASan, UBSan: clean), sound comes", pk > 1000);
        m2k_button(B_PLAY, 1);
        m2k_button(B_PLAY, 0);
        m2k_render(b, 128);
    }

    /* the negative */
    {
        const uint8_t *f;
        m2k_setting(S_SCREEN, 127);
        f = m2k_frame(10000);
        ok("negative: S_SCREEN dark -> the platform's (m2k_frame_negative), the frame as drawn", m2k_frame_negative() &&
           f == fb_out);
        m2k_setting(S_SCREEN, 0);
        ok("negative: light -> not", !m2k_frame_negative());
    }

    /* SYNC IN in four */
    {
        char o[16];
        const char *u;
        const pi_t *pi = pinfo(0, SETTING(24));
        ok("SYNC IN: GLO > MIDI's second knob in this build, four choices", knob_param(PG_GLO, 4, 1) == SETTING(24) &&
           kind_choices(pi->kind) == 4);
        fmt(pi, 0, enc(2, 4), o, &u);
        ok("SYNC IN: BLE reads BLE", str_eq(o, "BLE"));
        fmt(pi, 0, enc(3, 4), o, &u);
        ok("SYNC IN: LINK reads LINK", str_eq(o, "LINK"));
        ok("GLO > BT: no knob of the core's (the page's own)", knob_param(PG_GLO, 5, 0) == NONE &&
           knob_param(PG_GLO, 5, 3) == NONE);
    }

    /* the page's keys */
    {
        int oct;
        go(PG_HOME);
        while (!(ui.page == PG_GLO && ui.sub == 5)) go(PG_GLO);
        m2k_frame(12000);
        ok("the page shown: the platform told", btp.open);
        oct = ui.oct;
        m2k_button(B_GLO, 1);                        /* GLO held: OCT- / OCT+ are its undo and redo, not the page's */
        btp.sel = 0;
        m2k_button(B_OCTU, 1);
        m2k_button(B_OCTU, 0);
        ok("GLO held: OCT+ is not the page's", !btp.on && bs.starts == 0u);
        m2k_button(B_GLO, 0);
        m2k_button(B_OCTU, 1);
        m2k_button(B_OCTU, 0);
        ok("GLO let go: OCT+ is the page's (BLUETOOTH ON), the octave kept", btp.on && bs.starts == 1u && ui.oct == oct);
    }
    printf(fails ? "m2k_bt: %d FAILED\n" : "m2k_bt: all checks passed\n", fails);
    return fails != 0;
}
