/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE (Felucca's tests/bt_page_test.c and bt_page_render.c, as OberVave's, on MONO2K) */
/* The Bluetooth page on the host (firmware/fm1/bt_page.c, the core's m2k_bt.c draws it): GLO's sixth page (TIME,
 * GLITCH, VERB, SET, MIDI, BT; any other page leaves it), the cursor (PRESETS, wrapping), BLUETOOTH ON / OFF (the only
 * start: ble_user_start once), AUTOSTART (after a clean boot only, 3 s on; never with it OFF), ROLE (DEVICE: no device
 * lines), MODE and RECONNECT (stop at the ends, OCT+ wraps), + ADD A DEVICE (the 8 s scan, BLE-MIDI first, the saved
 * left out, a pick saved first; NOTHING FOUND: OCT+ again), a device line (OCT+ uses it, a knob left asks FORGET,
 * OCT- keeps, OCT+ forgets), the state and dim lines (FAILED NO RF TRIM IN FLASH; AUDIO GAP / LATE / SHORT), the keys
 * the page takes (PRESETS, KNOB 1..4, OCT-, OCT+: no octave change, no pattern change) and those it leaves, the
 * header's BT (none, struck, lit); and the page drawn in every state as screens (build/host/bt/bt-*.pbm) with the
 * core's text audit (no two texts touching, none cut). */
#define M2K_TEXT_AUDIT 1
#include "bt_test.h"

static uint32_t now_ms = 1000;
static int audit_bad;
static const uint8_t *frame_now(void)                /* a frame of the page now, its text audited */
{
    const uint8_t *f;
    now_ms += 600;                                   /* (past a page's transition and a toast's sweep) */
    ui.toast_t = 0;
    f = m2k_frame(now_ms);
    for (int i = 0; i < ta_n; i++) {
        const ta_t *a = &ta[i];
        if (a->cut) { printf("      TEXT cut: \"%s\"\n", a->s); audit_bad++; }
        for (int j = i + 1; j < ta_n; j++) {
            const ta_t *b = &ta[j];
            if (a->x1 < 0 || b->x1 < 0) continue;
            if (a->x0 <= b->x1 + 1 && b->x0 <= a->x1 + 1 && a->y0 <= b->y1 + 1 && b->y0 <= a->y1 + 1) {
                printf("      TEXT touch: \"%s\" (%d,%d..%d,%d) and \"%s\" (%d,%d..%d,%d)\n", a->s, a->x0, a->y0, a->x1,
                       a->y1, b->s, b->x0, b->y0, b->x1, b->y1);
                audit_bad++;
            }
        }
    }
    return f;
}
static void shot(const char *name)                   /* the screen now -> build/host/bt/bt-<name>.pbm */
{
    char path[256];
    const uint8_t *f = frame_now();
    FILE *o;
    snprintf(path, sizeof path, "build/host/bt/bt-%s.pbm", name);
    if ((o = fopen(path, "wb"))) {
        fprintf(o, "P4\n240 240\n");
        fwrite(f, 1, M2K_FB_BYTES, o);
        fclose(o);
    }
}
static void turn(int enc, int n) { m2k_encoder(enc, n); }
static void press(int b) { m2k_button(b, 1); m2k_button(b, 0); }
static void scan_dev(uint32_t i, uint8_t a0, const char *name, int midi, int rssi)
{
    memset(&bs.dev[i], 0, sizeof bs.dev[i]);
    bs.dev[i].addr[0] = a0;
    bs.dev[i].addr[1] = 0x00;
    bs.dev[i].addr[5] = 0x55;
    bs.dev[i].midi = (uint8_t)midi;
    bs.dev[i].rssi = (int8_t)rssi;
    bs.dev[i].seen_ms = fm1_ms;
    bt_cpy(bs.dev[i].name, name, sizeof bs.dev[i].name);
    if (bs.ndev <= i)
        bs.ndev = i + 1u;
}
static void state_line(char *st) { bt_state_text(st, 32); }
static void to_bt(void)                              /* GLO tapped to its sixth page */
{
    while (!(ui.page == PG_GLO && ui.sub == 5)) go(PG_GLO);
    frame_now();
}

int main(void)
{
    char st[40], lb[24];
    int oct, pat0;
    bt_fresh(0);
    frame_now();                                     /* (the boot's splash: its time starts) */
    now_ms += 2000;

    /* GLO's pages */
    {
        char o[24], seq[64] = "";
        go(PG_GLO);
        for (int i = 0; i < 7; i++) {
            page_label(ui.page, ui.sub, o);
            strcat(seq, o);
            strcat(seq, " ");
            go(PG_GLO);
        }
        ok("GLO tapped: TIME GLITCH VERB SET MIDI BT, then TIME", str_eq(seq, "TIME GLITCH VERB SET MIDI BT TIME "));
    }
    to_bt();
    ok("GLO > BT: the platform hears the page open (HOST: a scan while it is open)", btp.open && bt_dv.page == 1u);
    ok("BLUETOOTH is off at power-on, nothing started", !btp.on && bs.starts == 0u && bt_head_icon() == 0u);
    state_line(st);
    ok("state line: OFF", str_eq(st, "OFF"));
    shot("off");

    /* the cursor and the rows */
    ok("rows: five settings, + ADD A DEVICE (no device saved)", bt_page_rows() == 6u && m2k_bt_view()->n == 6u);
    turn(E_PRESET, -1);
    ok("PRESETS back from the first row wraps to the last", btp.sel == 5u);
    turn(E_PRESET, 1);
    pat0 = cur_pat;
    ok(".. and on wraps to the first; PRESETS is the page's (no pattern change)", btp.sel == 0u && cur_pat == pat0);
    oct = ui.oct;
    press(B_OCTU);
    ok("OCT+ on BLUETOOTH: ON, the stack started once, links wanted", btp.on && bs.starts == 1u && bs.want == 1);
    ok("OCT taken by the page (no octave change)", ui.oct == oct);
    ok("the header's BT: struck (on, nothing linked)", bt_head_icon() == 1u);
    bs.state = BLE_ST_IDLE;
    turn(E_K2, 1);
    ok("a knob right on BLUETOOTH ON: no second start", bs.starts == 1u);
    turn(E_K1, -1);
    ok("a knob left: OFF, links dropped (the stack stays up)", !btp.on && bs.want == 0 && bt_head_icon() == 0u);
    turn(E_K4, 1);
    ok("a knob right: ON again, still one start", btp.on && bs.starts == 1u && bs.want == 1);
    state_line(st);
    ok("state line: ON, NO DEVICE SAVED", str_eq(st, "ON, NO DEVICE SAVED"));
    shot("on");

    turn(E_PRESET, 1);                               /* AUTOSTART */
    turn(E_K1, 1);
    {
        bt_set_t s;
        bt_set_get(&s);
        ok("AUTOSTART: a knob right -> ON, saved (the record)", s.autostart && bts_dirty);
    }
    turn(E_PRESET, 1);                               /* ROLE */
    press(B_OCTU);
    ok("ROLE: OCT+ -> DEVICE, handed to the BLE unit, no device lines", bt_role() == 1u && bs_role == 1 &&
       bt_page_rows() == 5u);
    bs.state = BLE_ST_ADVERTISING;
    state_line(st);
    ok("ROLE DEVICE: WAITING FOR A HOST", str_eq(st, "WAITING FOR A HOST"));
    shot("device");
    bs.state = BLE_ST_SUBSCRIBED;
    bt_cpy(conn_name, "iPad", sizeof conn_name);
    conn_up = 1;
    state_line(st);
    ok("ROLE DEVICE: CONNECTED and the host's name; the header's BT lit", str_eq(st, "CONNECTED iPad") &&
       bt_head_icon() == 2u);
    shot("device-linked");
    conn_up = 0;
    bs.state = BLE_ST_IDLE;
    turn(E_K3, -1);
    ok("ROLE: a knob left -> HOST", bt_role() == 0u && bs_role == 0);
    turn(E_PRESET, 1);                               /* MODE */
    for (int i = 0; i < 4; i++) turn(E_K1, 1);
    {
        bt_set_t s;
        bt_set_get(&s);
        ok("MODE: a knob right steps and stops at FM-LINK", s.mode == BT_MODE_LINK);
        press(B_OCTU);
        bt_set_get(&s);
        ok("MODE: OCT+ wraps to AUTO", s.mode == BT_MODE_AUTO);
    }
    turn(E_PRESET, 1);                               /* RECONNECT */
    turn(E_K2, -1);
    {
        bt_set_t s;
        bt_set_get(&s);
        ok("RECONNECT: a knob left stops at AUTO", s.rate == BT_RATE_AUTO);
        turn(E_K2, 2);
        bt_set_get(&s);
        ok("RECONNECT: right -> FAST (one step a turn)", s.rate == BT_RATE_FAST);
        press(B_OCTU);
        press(B_OCTU);
        bt_set_get(&s);
        ok("RECONNECT: OCT+ twice wraps to AUTO", s.rate == BT_RATE_AUTO);
    }

    /* + ADD A DEVICE */
    turn(E_PRESET, 1);
    ok("the cursor on + ADD A DEVICE", btp.sel == BT_SET_ROWS);
    press(B_OCTU);
    ok("OCT+: the ADD list, its scan", btp.add);
    scan_dev(0, 0x21, "SMC-Mixer", 1, -48);
    scan_dev(1, 0x22, "Felucca FM-1", 0, -40);
    scan_dev(2, 0x23, "Xkey Air 25", 1, -70);
    (void)m2k_bt_view();
    ok("the list: BLE-MIDI devices first, each part by signal", btp.an == 3u && btp.adev[0].addr[0] == 0x21 &&
       btp.adev[1].addr[0] == 0x23 && btp.adev[2].addr[0] == 0x22);
    state_line(st);
    ok("state line: SCANNING and its seconds", st[0] == 'S' && st[1] == 'C');
    shot("add");
    turn(E_PRESET, 1);
    turn(E_PRESET, 1);
    press(B_OCTU);
    ok("a pick: saved first (an FM-1 by its name), the list closed, the cursor on its line", !btp.add &&
       bt_devs_n() == 1u && btp.sel == BT_SET_ROWS);
    {
        bt_dev_t d;
        ok(".. Felucca FM-1 is an FM-1", bt_devs_get(0, &d) && d.kind == BT_KIND_FM1);
    }
    turn(E_PRESET, 1);
    press(B_OCTU);
    (void)m2k_bt_view();
    fm1_ms += BT_ADD_MS + 10u;
    (void)m2k_bt_view();
    ok("the scan's 8 s over: the list stays as it is (the saved one left out)", btp.an == 2u &&
       btp.adev[0].addr[0] == 0x21);
    press(B_OCTU);                                   /* SMC-Mixer */
    ok("a second pick: two saved, the last first", bt_devs_n() == 2u);
    turn(E_PRESET, 1);                               /* (on ADD again) */
    turn(E_PRESET, 1);
    press(B_OCTU);
    bs.ndev = 0;
    (void)m2k_bt_view();
    fm1_ms += BT_ADD_MS + 10u;
    (void)m2k_bt_view();
    state_line(st);
    ok("nothing new around: NOTHING FOUND", str_eq(st, "NOTHING FOUND") && btp.an == 0u);
    shot("nothing");
    press(B_OCTU);
    ok(".. OCT+ scans again", btp.add && fm1_ms - btp.add_t0 < BT_ADD_MS);
    press(B_OCTD);
    ok("OCT-: back to the rows", !btp.add && btp.open);

    /* the device lines: use, forget */
    scan_dev(0, 0x21, "SMC-Mixer", 1, -48);
    memset(&bh.slot[BLE_SLOT_FM1], 0, sizeof bh.slot[0]);
    bh.slot[BLE_SLOT_FM1].st = BLE_LS_UP;
    {
        bt_dev_t d;
        bt_devs_get(1, &d);                          /* the FM-1 */
        memcpy(bh.slot[BLE_SLOT_FM1].addr, d.addr, 6);
    }
    btp.sel = BT_SET_ROWS;
    state_line(st);
    ok("state line: ON, 1 CONNECTED; the header's BT lit", str_eq(st, "ON, 1 CONNECTED") && bt_head_icon() == 2u);
    ok("device lines: the SMC-Mixer NEAR, the FM-1 ON", bt_devs_state(0) == 1 && bt_devs_state(1) == 2);
    {
        const m2k_bt_view_t *v = m2k_bt_view();
        ok("the view: five rows, a rule, the devices (a ring NEAR, a dot ON), + ADD", v->n == 8u && v->rule == 5u &&
           str_eq(v->line[5].value, "NEAR") && v->line[5].dot == 1 && str_eq(v->line[6].value, "ON") &&
           v->line[6].dot == 2 && str_eq(v->line[6].tag, "FM-1") && str_eq(v->line[7].label, "+ ADD A DEVICE") &&
           v->line[5].sel);
    }
    shot("devices");
    btp.sel = BT_SET_ROWS + 1u;                      /* the FM-1, connected */
    press(B_OCTU);
    ok("OCT+ on a connected device: says CONNECTED, the order kept", str_eq(ui.toast, "CONNECTED"));
    btp.sel = BT_SET_ROWS;
    turn(E_K1, -1);
    ok("a knob left on a device: FORGET <name>?", btp.confirm == 1u);
    state_line(st);
    ok("state line: FORGET SMC-Mixer? (asked)", str_eq(st, "FORGET SMC-Mixer?") && m2k_bt_view()->ask);
    shot("forget");
    press(B_OCTD);
    ok("OCT-: kept", btp.confirm == 0u && bt_devs_n() == 2u);
    turn(E_K1, -1);
    press(B_OCTU);
    ok("OCT+: forgotten", bt_devs_n() == 1u);
    {
        bt_dev_t d;
        bt_devs_get(0, &d);
        bt_dev_label(&d, lb);
        ok("an FM-1's line: FM-1 and its address' last four hex digits", str_eq(lb, "FM-1 0022"));
    }
    for (int i = 0; i < 8; i++) {                    /* a full list: four lines in view, the cursor scrolls them */
        bt_dev_t d;
        memset(&d, 0, sizeof d);
        d.addr[0] = (uint8_t)(0x40 + i);
        bt_cpy(d.name, i & 1 ? "A VERY LONG CONTROLLER NAME" : "nanoKEY", sizeof d.name);
        bt_devs_add(&d);
    }
    btp.sel = (uint8_t)(BT_SET_ROWS + bt_devs_n());  /* + ADD, past the eighth */
    {
        const m2k_bt_view_t *v = m2k_bt_view();
        ok("eight saved: four lines in view, scrolled to + ADD", v->n == BT_SET_ROWS + BT_VIS &&
           str_eq(v->line[v->n - 1].label, "+ ADD A DEVICE") && v->line[v->n - 1].sel);
    }
    shot("full");
    bs.state = BLE_ST_FAILED;
    bs.err = "NO RF TRIM IN FLASH";
    state_line(st);
    ok("a failed start: FAILED NO RF TRIM IN FLASH (the unit whose calibration is gone)",
       str_eq(st, "FAILED NO RF TRIM IN FLASH"));
    shot("failed");
    press(B_OCTU);                                   /* (the cursor is not on BLUETOOTH: on + ADD) */
    btp.sel = 0;
    btp.on = 0;
    press(B_OCTU);
    ok("FAILED: BLUETOOTH ON says POWER OFF AND ON TO RETRY, no start", !btp.on && str_eq(ui.toast, "POWER OFF AND ON"));
    bs.state = BLE_ST_IDLE;
    bs.err = 0;
    bt_a_gap = 12100u * FM1_TICKS_PER_US;
    audio_lates = 3;
    ring_short = 1;
    {
        char dg[40], tm[40];
        bt_diag_text(dg, sizeof dg);
        bt_timing_text(tm, sizeof tm);
        ok("the dim and timing lines: the audio's longest gap, the late and the short halves (the device test's numbers)",
           str_eq(dg, "AUDIO GAP 12.1MS") && str_eq(tm, "LATE 3  SHORT 1"));
    }
    shot("timing");
    bt_a_gap = 0;
    audio_lates = ring_short = 0;

    /* the keys the page leaves: the others, and GLO held's combos */
    {
        int t = ui.trk;
        turn(E_SELECT, 1);
        ok("SELECT is not the page's: the next track", ui.trk == (t + 1) % M2K_TRACKS);
    }

    /* leaving */
    go(PG_HOME);
    frame_now();
    ok("another page: the platform hears it left, no page scan", !btp.open && bt_dv.page == 0u);
    oct = ui.oct;
    press(B_OCTU);
    ok("off the page OCT+ is the octave again", ui.oct == oct + 1);
    to_bt();
    go(PG_GLO);
    frame_now();
    ok("GLO again from BT: TIME, the page left", ui.sub == 0 && !btp.open);

    /* AUTOSTART */
    {
        bt_set_t s;
        bt_set_get(&s);
        s.autostart = 1;
        bt_set_put(&s);
        flush();
    }
    bt_fresh(1);                                     /* (the record as saved: AUTOSTART ON) */
    bt_boot_clean = 1;
    bt_page_auto();
    fm1_ms += 2900u;
    bt_page_auto();
    ok("AUTOSTART: not in the first 3 s", bs.starts == 0u);
    fm1_ms += 200u;
    bt_page_auto();
    ok("AUTOSTART ON, a clean boot: BLUETOOTH ON once, 3 s on", btp.on && bs.starts == 1u);
    fm1_ms += 10000u;
    bt_page_auto();
    ok(".. once only", bs.starts == 1u);
    bt_fresh(1);
    bt_boot_clean = 0;
    bt_page_auto();
    fm1_ms += 3100u;
    bt_page_auto();
    ok("AUTOSTART after a crash or a watchdog reset: skipped, said", !btp.on && bs.starts == 0u &&
       str_eq(ui.toast, "BT AUTOSTART SKIPPED"));
    {
        bt_set_t s;
        bt_fresh(0);
        bt_boot_clean = 1;
        bt_set_get(&s);
        bt_page_auto();
        fm1_ms += 5000u;
        bt_page_auto();
        ok("AUTOSTART OFF: Bluetooth stays off", !s.autostart && !btp.on && bs.starts == 0u);
    }

    /* the header's BT, drawn */
    bt_fresh(0);
    frame_now();
    now_ms += 2000;
    go(PG_HOME);
    {
        const uint8_t *f = frame_now();
        int off = 0, struck = 0, lit = 0;
        for (int y = 0; y < 15; y++) for (int x = 150; x < 228; x++) off += f[y * 30 + x / 8] >> (7 - x % 8) & 1;
        btp.on = 1;
        f = frame_now();
        for (int y = 0; y < 15; y++) for (int x = 150; x < 228; x++) struck += f[y * 30 + x / 8] >> (7 - x % 8) & 1;
        shot("head-struck");
        bh.slot[0].st = BLE_LS_UP;
        f = frame_now();
        for (int y = 0; y < 15; y++) for (int x = 150; x < 228; x++) lit += f[y * 30 + x / 8] >> (7 - x % 8) & 1;
        shot("head-lit");
        ok("the header's BT: none off, a tag struck while on, solid while linked", struck > off && lit > struck);
    }
    ok("the text audit: no texts touching, none cut, on every page drawn", audit_bad == 0);
    printf(fails ? "bt_page: %d FAILED\n" : "bt_page: all checks passed\n", fails);
    return fails != 0;
}
