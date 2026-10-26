/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE (from Felucca's ble-kit bt_page.c, as OberVave's)
 * FM-Link: 2026 DEADACTIVE */
/* FELUCCA_BLE (m2k_fm1.c includes it after bt_devs.c): the Bluetooth page, GLO's sixth (BT): its rows and what they do
 * are here, the core draws them in MONO2K's look (m2k_bt_view: the core's m2k_bt.c) and hands it the page's keys
 * (m2k_bt_input). Felucca's rows and behaviour (docs/superpowers/specs/2026-10-22-bt-devices-design.md 5.1, mockup
 * A): a state line (FORGET <name>? asked there), a dim line (the last error; else the audio's longest gap between two
 * halves since the page opened) and, over the key line, the halves the DMA moved on from (LATE) and the ones the ring
 * ran short for (SHORT) since power-on (the device test's numbers: MONO2K has no serial console), the rows BLUETOOTH, AUTOSTART, ROLE (HOST, DEVICE), MODE
 * (AUTO, SMC DAW, MIDI, FM-LINK) and RECONNECT (AUTO, FAST, SLOW: bt_set.c bt_set_slow), then (HOST) the saved
 * devices, a line each (a dot connected / a ring, its name, CTRL or FM-1, ON / NEAR / AWAY), and + ADD A DEVICE, four
 * lines in view, and the key line. On a device line a knob left asks FORGET <name>? (OCT+ yes, OCT- no) and OCT+ uses
 * it now (the first of its kind); + ADD A DEVICE opens the scan list of nearby devices not saved (BLE-MIDI ones
 * first), found in BT_ADD_MS, then kept as they are: OCT+ saves and connects one, OCT- goes back (NOTHING FOUND: OCT+
 * scans again). ROLE DEVICE: the FM-1 is the BLE-MIDI device a host connects to; no device lines then. PRESETS moves
 * the cursor (it wraps), any of KNOB 1..4 changes the row, OCT+ acts on it; GLO tapped, or any other page, leaves it.
 * The keys still play.
 * BLUETOOTH ON is the only call of ble_user_start (gate 3: tools/check_ble_boot.py TRIGGERS has bt_page_on);
 * Bluetooth is off at every power-on and ON is not saved, unless AUTOSTART is ON: then bt_page_auto turns it on once,
 * BT_AUTO_MS after the main loop began, after a clean boot only. AUTOSTART, ROLE, MODE, RECONNECT, the saved devices
 * are saved (bt_store.c). Main loop only. */
#define BT_PAGE_C 1
#define BT_SET_ROWS 5u                              /* BLUETOOTH AUTOSTART ROLE MODE RECONNECT, then (HOST) the
                                                     * devices, ADD */
#define BT_SHOW 6u                                  /* the ADD list's lines (the scan keeps 12) */
#define BT_ADD_MS 8000u                             /* the ADD list's scan, then the list stays (the user: 8 s) */
#define BT_VIS 4u                                   /* device lines in view (the cursor scrolls them) */
_Static_assert(BT_SET_ROWS + BT_VIS <= M2K_BT_LINES && BT_SHOW <= M2K_BT_LINES, "the page's lines");
static const char *const BT_MODE_N[4] = {"AUTO", "SMC DAW", "MIDI", "FM-LINK"};
static const char *const BT_RATE_N[3] = {"AUTO", "FAST", "SLOW"};
static uint32_t bt_role(void) { bt_set_t s; bt_set_get(&s); return s.role; }   /* 1: ROLE DEVICE */
static struct {
    uint8_t on;                                     /* BLUETOOTH ON (not saved) */
    uint8_t open;                                   /* the page is shown */
    uint8_t sel, top;                               /* the cursor (a row), the first device line in view */
    uint8_t confirm;                                /* FORGET asked for saved device confirm - 1 (0: none) */
    uint8_t add, asel, an;                          /* the ADD list open, its cursor, its devices */
    uint32_t add_t0;                                /* .. its scan's start (BT_ADD_MS: then it stays) */
    ble_ui_dev_t adev[BT_SHOW];                     /* the ADD list as shown: a pick takes these */
    m2k_bt_view_t view;                             /* the page as the core draws it */
} btp;                                              /* (in RAM: the pool has the BT heap, the second core's stacks) */

static void bt_say(const char *a) { m2k_toast(a, 0); }
static void bt_retry(void) { m2k_toast("POWER OFF AND ON", "TO RETRY"); }   /* (FAILED: the stack cannot stop) */

/* the header's BT (the core's header; Felucca's top-bar icon, design 5.6): 0 none (Bluetooth off), 1 struck (on,
 * nothing linked: searching, reconnecting, the device role waiting), 2 lit (a link up) */
static uint32_t bt_head_icon(void)
{
    ble_ui_slot_t x;
    uint32_t i, st = ble_ui_state();
    if (!btp.on)
        return 0;
    if (st == BLE_ST_SUBSCRIBED || st == BLE_ST_RECEIVING)
        return 2;
    for (i = 0; i < 2u; i++)
        if (ble_ui_slot(i, &x) && x.st == BLE_LS_UP)
            return 2;
    return 1;
}
int m2k_bt_icon(void) { return (int)bt_head_icon(); }

static uint32_t bt_page_rows(void) { return bt_role() ? BT_SET_ROWS : BT_SET_ROWS + bt_devs_n() + 1u; }

static void bt_cpy(char *o, const char *s, uint32_t n)   /* a C string into o[n] (cut) */
{
    uint32_t i;
    for (i = 0; i + 1u < n && s[i]; i++)
        o[i] = s[i];
    o[i] = 0;
}
static void bt_cat(char *o, const char *s, uint32_t n)   /* appended, within o[n] */
{
    uint32_t i = 0;
    while (i < n && o[i])
        i++;
    if (i < n)
        bt_cpy(o + i, s, n - i);
}
static void bt_catu(char *o, uint32_t v, uint32_t n)     /* a number appended */
{
    char b[12];
    int k = 0;
    char t[12];
    int j = 0;
    do { b[k++] = (char)('0' + v % 10u); v /= 10u; } while (v && k < 10);
    while (k) t[j++] = b[--k];
    t[j] = 0;
    bt_cat(o, t, n);
}
/* a name as the screen can show it: the font's characters only (a device's name is any bytes) */
static void bt_name(char *o, const char *s, uint32_t n)
{
    uint32_t i;
    for (i = 0; i + 1u < n && s[i]; i++)
        o[i] = s[i] >= 0x20 && s[i] < 0x7F ? s[i] : '?';
    o[i] = 0;
}
static char *bt_hex2(char *o, uint8_t v)
{
    static const char H[] = "0123456789ABCDEF";
    *o++ = H[v >> 4];
    *o++ = H[v & 15u];
    return o;
}
/* a saved device's line text (24 B): its name; an FM-1 (they all say Felucca FM-1) FM-1 and its address' last four
 * hex digits, most significant first */
static void bt_dev_label(const bt_dev_t *d, char *o)
{
    if (d->kind == BT_KIND_FM1) {
        bt_cpy(o, "FM-1 ", 24);
        *bt_hex2(bt_hex2(o + 5, d->addr[1]), d->addr[0]) = 0;
    } else {
        bt_name(o, d->name[0] ? d->name : "?", 24);
    }
}
static const char *bt_dev_value(int st) { return st == 2 ? "ON" : st ? "NEAR" : "AWAY"; }

static void bt_state_text(char *o, uint32_t n)      /* the state line */
{
    uint32_t st = ble_ui_state(), code, k, i;
    const char *e = ble_ui_err(&code);
    char t[24];
    bt_dev_t d;
    if (btp.confirm && bt_devs_get(btp.confirm - 1u, &d)) {
        bt_dev_label(&d, t);
        bt_cpy(o, "FORGET ", n);
        bt_cat(o, t, n);
        bt_cat(o, "?", n);
        return;
    }
    if (st == BLE_ST_FAILED) {
        bt_cpy(o, "FAILED ", n);
        bt_cat(o, e, n);
    } else if (st == BLE_ST_STARTING) {
        bt_cpy(o, "STARTING", n);
    } else if (!btp.on || st == BLE_ST_OFF) {
        bt_cpy(o, "OFF", n);
    } else if (st == BLE_ST_ADVERTISING) {
        bt_cpy(o, "WAITING FOR A HOST", n);
    } else if (st == BLE_ST_SUBSCRIBED || st == BLE_ST_RECEIVING) {   /* the device role: its host */
        bt_cpy(o, "CONNECTED ", n);
        if (ble_ui_conn_name(t)) {
            t[23] = 0;
            bt_name(t, t, 24);
            bt_cat(o, t, n);
        }
    } else if (btp.add) {                           /* its scan's seconds left, then what it found */
        k = BT_ADD_MS - (fm1_ms - btp.add_t0);
        if ((int32_t)k > 0) {
            bt_cpy(o, "SCANNING ", n);
            bt_catu(o, (k + 999u) / 1000u, n);
            bt_cat(o, " S", n);
        } else {
            bt_cpy(o, btp.an ? "ADD: PICK A DEVICE" : "NOTHING FOUND", n);
        }
    } else if (!bt_devs_n()) {
        bt_cpy(o, "ON, NO DEVICE SAVED", n);
    } else {
        for (i = 0, k = 0; i < 2u; i++) {
            ble_ui_slot_t x;
            k += ble_ui_slot(i, &x) && x.st == BLE_LS_UP;
        }
        if (k) {
            bt_cpy(o, "ON, ", n);
            bt_catu(o, k, n);
            bt_cat(o, " CONNECTED", n);
        } else {
            bt_cpy(o, "ON, SEARCHING", n);
        }
    }
}

/* the dim line: the last error, else the audio's timing: the longest gap between two audio halves since the page
 * opened (5.8 ms when nothing held the audio interrupt off), the halves late (the DMA had moved on: platform.c) and
 * short (the ring empty: the render behind) since power-on */
static void bt_diag_text(char *o, uint32_t n)
{
    uint32_t code, g;
    const char *e = ble_ui_state() == BLE_ST_FAILED ? "" : ble_ui_err(&code);
    if (e[0]) {
        bt_cpy(o, e, n);
        return;
    }
    g = (bt_a_gap / FM1_TICKS_PER_US + 50u) / 100u;   /* 0.1 ms */
    bt_cpy(o, "AUDIO GAP ", n);
    bt_catu(o, g / 10u, n);
    bt_cat(o, ".", n);
    bt_catu(o, g % 10u, n);
    bt_cat(o, "MS", n);
}
static void bt_timing_text(char *o, uint32_t n)     /* the timing line: the late halves, the short ones */
{
    bt_cpy(o, "LATE ", n);
    bt_catu(o, audio_lates, n);
    bt_cat(o, "  SHORT ", n);
    bt_catu(o, ring_short, n);
}

/* the ADD list: the BLE unit's scan list without the saved devices, BLE-MIDI devices first, each part by signal, the
 * first BT_SHOW; the cursor kept on the device it was on (the list re-sorts as signals change) */
static int bt_saved(const uint8_t a[6])
{
    bt_dev_t d;
    uint32_t i;
    for (i = 0; bt_devs_get(i, &d); i++)
        if (!memcmp(d.addr, a, 6))
            return 1;
    return 0;
}
static void bt_add_list(void)
{
    ble_ui_dev_t d;
    uint8_t was[6];
    uint32_t i, k, pass, n = ble_ui_devs(), had = btp.asel < btp.an;
    if (fm1_ms - btp.add_t0 >= BT_ADD_MS)
        return;                                     /* (its scan is over: the list stays as it is) */
    if (had)
        memcpy(was, btp.adev[btp.asel].addr, 6);
    btp.an = 0;
    for (pass = 0; pass < 2u; pass++)
        for (i = 0; i < n; i++) {
            if (!ble_ui_dev(i, &d) || (d.midi != 0) != (pass == 0u) || bt_saved(d.addr) || btp.an >= BT_SHOW)
                continue;
            d.name[sizeof d.name - 1u] = 0;
            btp.adev[btp.an++] = d;
        }
    for (k = 0; had && k < btp.an; k++)
        if (!memcmp(btp.adev[k].addr, was, 6))
            btp.asel = (uint8_t)k;
    if (btp.asel >= btp.an)
        btp.asel = btp.an ? (uint8_t)(btp.an - 1u) : 0u;
}
static void bt_add_close(void) { btp.add = 0; }

/* BLUETOOTH ON: links wanted, the role handed over, the stack started if it is not (the user's trigger: the only
 * call of ble_user_start, gate 3); the saved devices are the targets (bt_devs_frame) */
__attribute__((noinline)) static void bt_page_on(void)
{
    if (ble_ui_state() == BLE_ST_FAILED) {          /* the stack cannot stop: only a power cycle starts it again */
        bt_retry();
        return;
    }
    if (btp.on)
        return;
    btp.on = 1;
    ble_ui_role((int)bt_role());                    /* (applied once the stack is up and idle) */
    ble_ui_want(1);
    if (ble_ui_state() == BLE_ST_OFF)
        ble_user_start();
}

/* AUTOSTART (bt_service, every main-loop pass): once per power-on, BT_AUTO_MS after the main loop first served
 * Bluetooth, BLUETOOTH ON as from the page, when AUTOSTART is ON and the boot was clean (no watchdog reset, no soft
 * reset before this one: bt_boot_clean). So a Bluetooth start that crashes or hangs the FM-1 is not tried again on the
 * next power-on: the way to the updater stays clear. Gate 3 (tools/check_ble_boot.py) allows it only from the main
 * loop's service */
#define BT_AUTO_MS 3000u
static struct { uint8_t armed, done; uint32_t t0; } bt_auto;
__attribute__((noinline)) static void bt_page_auto(void)
{
    bt_set_t s;
    if (bt_auto.done)
        return;
    if (!bt_auto.armed) {
        bt_auto.armed = 1;
        bt_auto.t0 = fm1_ms;
        return;
    }
    if (fm1_ms - bt_auto.t0 < BT_AUTO_MS)
        return;
    bt_auto.done = 1;
    bt_set_get(&s);
    if (!s.autostart)
        return;
    if (!bt_boot_clean) {
        bt_say("BT AUTOSTART SKIPPED");             /* (after a crash, a hang or a failed boot) */
        return;
    }
    bt_page_on();
}

/* the core: GLO > BT shown (1) or left (0) */
void m2k_bt_page(int open)
{
    if (open && !btp.open) {
        btp.open = 1;
        btp.sel = btp.top = btp.confirm = 0;
        bt_add_close();
        bt_a_gap = 0;                               /* (the dim line: the longest gap since the page opened) */
        bt_devs_page(!bt_role());                   /* (HOST: scan while the page is open: its ON / NEAR / AWAY) */
    } else if (!open && btp.open) {
        btp.open = 0;
        btp.confirm = 0;
        bt_add_close();
        bt_devs_page(0);
    }
}

/* the core, on the page: PRESETS' detents (the cursor), KNOB 1..4's (the row), OCT- 1 / OCT+ 2 */
__attribute__((noinline)) static void bt_page_key(int e, int s, int oct)
{
    uint32_t ok = oct == 2, back = oct == 1, rows = bt_page_rows(), st = ble_ui_state(), k, n = bt_devs_n();
    bt_set_t set1;
    if (!btp.open)
        return;
    if (btp.add) {                                  /* the ADD list: PRESETS moves, OCT+ adds, OCT- back */
        if (e && btp.an)
            btp.asel = (uint8_t)((btp.asel + (e > 0 ? 1u : btp.an - 1u)) % btp.an);
        if (back) {
            bt_add_close();
        } else if (ok && !btp.an) {
            btp.add_t0 = fm1_ms;                    /* (nothing found: OCT+ scans again) */
        } else if (ok && btp.asel < btp.an) {
            const ble_ui_dev_t *a = &btp.adev[btp.asel];
            bt_dev_t d;
            memset(&d, 0, sizeof d);
            memcpy(d.addr, a->addr, 6);
            d.type = a->type & 1u;
            d.kind = (uint8_t)bt_dev_kind(a->name);
            bt_cpy(d.name, a->name, sizeof d.name);
            bt_devs_add(&d);                        /* (first of its kind: bt_devs_frame connects it) */
            bt_add_close();
            btp.sel = BT_SET_ROWS;                  /* on its line */
        }
        return;
    }
    if (btp.confirm) {                              /* FORGET <name>?: OCT+ yes, OCT- no; anything else no */
        k = btp.confirm - 1u;
        btp.confirm = 0;
        if (ok)
            bt_devs_forget(k);
        if (btp.sel >= bt_page_rows())
            btp.sel = (uint8_t)(bt_page_rows() - 1u);
        return;
    }
    if (e)                                          /* one row per detent, wrapping */
        btp.sel = (uint8_t)((btp.sel + (e > 0 ? 1u : rows - 1u)) % rows);
    switch (btp.sel) {
    case 0:                                         /* BLUETOOTH: a knob right ON, left OFF; OCT+ toggles */
        if (s > 0 || (ok && !btp.on)) {
            bt_page_on();
        } else if ((s < 0 || ok) && btp.on) {
            btp.on = 0;
            ble_ui_want(0);
        }
        break;
    case 1:                                         /* AUTOSTART: a knob right ON, left OFF; OCT+ toggles; saved */
        bt_set_get(&set1);
        k = s > 0 ? 1u : s < 0 ? 0u : ok ? !set1.autostart : set1.autostart;
        if (k != set1.autostart) {
            set1.autostart = (uint8_t)k;
            bt_set_put(&set1);
        }
        break;
    case 2:                                         /* ROLE: a knob right DEVICE, left HOST; OCT+ toggles; saved */
        bt_set_get(&set1);
        k = s > 0 ? 1u : s < 0 ? 0u : ok ? !set1.role : set1.role;
        if (k != set1.role) {
            set1.role = (uint8_t)k;
            set1.link = 0;
            bt_set_put(&set1);
            ble_ui_role((int)k);                    /* (the BLE unit ends its links, a scan or advertising, then switches) */
            bt_devs_page(!k);
            btp.top = 0;
        }
        break;
    case 3:                                         /* MODE, RECONNECT: a knob steps and stops at the ends; OCT+ */
    case 4: {                                       /* wraps; saved */
        uint8_t *v;
        uint32_t m;
        bt_set_get(&set1);
        v = btp.sel == 3u ? &set1.mode : &set1.rate;
        m = btp.sel == 3u ? BT_MODE_LINK : BT_RATE_SLOW;
        k = s > 0 ? (*v < m ? *v + 1u : m) : s < 0 ? (*v ? *v - 1u : 0u) : ok ? (*v < m ? *v + 1u : 0u) : *v;
        if (k != *v) {
            *v = (uint8_t)k;
            bt_set_put(&set1);
        }
        break;
    }
    default:
        k = btp.sel - BT_SET_ROWS;
        if (k == n) {                               /* + ADD A DEVICE */
            if (ok && st == BLE_ST_FAILED)
                bt_retry();
            else if (ok && !btp.on)
                bt_say("BLUETOOTH IS OFF");
            else if (ok) {
                btp.add = 1;
                btp.asel = btp.an = 0;
                btp.add_t0 = fm1_ms;
            }
        } else if (s < 0) {                         /* a device: a knob left asks FORGET */
            btp.confirm = (uint8_t)(k + 1u);
        } else if (ok) {                            /* OCT+: use it now (around, not linked) */
            int at = bt_devs_state(k);
            if (at == 1)
                bt_devs_use(k);
            else
                bt_say(at ? "CONNECTED" : "NOT AROUND");
        }
        break;
    }
}
void m2k_bt_input(int cursor, int knob, int oct) { bt_page_key(cursor, knob, oct); }

static void bt_line(m2k_bt_line_t *l, int sel, const char *label, const char *tag, const char *value, int lit, int dot)
{
    bt_cpy(l->label, label, sizeof l->label);
    bt_cpy(l->tag, tag ? tag : "", sizeof l->tag);
    bt_cpy(l->value, value, sizeof l->value);
    l->sel = (uint8_t)(sel != 0);
    l->lit = (uint8_t)(lit != 0);
    l->dot = (uint8_t)dot;
}

/* the core (each frame of the page): the page as it stands */
const m2k_bt_view_t *m2k_bt_view(void)
{
    m2k_bt_view_t *v = &btp.view;
    char lb[24];
    uint32_t n, rows, i;
    bt_set_t set1;
    bt_dev_t d;
    memset(v, 0, sizeof *v);
    if (btp.add)
        bt_add_list();
    bt_set_get(&set1);
    n = set1.role ? 0u : bt_devs_n();
    rows = bt_page_rows();
    if (btp.sel >= rows)
        btp.sel = (uint8_t)(rows - 1u);
    if (btp.sel < BT_SET_ROWS)                      /* the device line under the cursor in view */
        btp.top = 0;
    else if (btp.sel - BT_SET_ROWS < btp.top)
        btp.top = (uint8_t)(btp.sel - BT_SET_ROWS);
    else if (btp.sel - BT_SET_ROWS >= btp.top + BT_VIS)
        btp.top = (uint8_t)(btp.sel - BT_SET_ROWS - BT_VIS + 1u);
    bt_state_text(v->state, sizeof v->state);
    bt_diag_text(v->dim, sizeof v->dim);
    bt_timing_text(v->timing, sizeof v->timing);
    v->ask = (uint8_t)(btp.confirm != 0);
    if (btp.add) {                                  /* the ADD list over the rows: a device a line, BLE-MIDI tagged */
        for (i = 0; i < btp.an; i++) {
            char r[8];
            const ble_ui_dev_t *a = &btp.adev[i];
            if (a->name[0])
                bt_name(lb, a->name, sizeof lb);
            else
                *bt_hex2(bt_hex2(bt_hex2(lb, a->addr[5]), a->addr[4]), a->addr[3]) = 0;
            r[0] = 0;
            if (a->rssi < 0) {
                bt_cpy(r, "-", sizeof r);
                bt_catu(r, (uint32_t)-a->rssi, sizeof r);
            } else
                bt_catu(r, (uint32_t)a->rssi, sizeof r);
            bt_line(&v->line[i], btp.asel == i, lb, a->midi ? "MIDI" : 0, r, 0, 0);
        }
        v->n = btp.an;
        if (!btp.an)
            bt_line(&v->line[v->n++], 0, fm1_ms - btp.add_t0 < BT_ADD_MS ? "LOOKING FOR DEVICES..." : "OCT+: SCAN AGAIN", 0,
                    "", 0, 0);
        bt_cpy(v->keys, "PRESETS  OCT+ ADD  OCT- BACK", sizeof v->keys);
        return v;
    }
    bt_line(&v->line[0], btp.sel == 0u, "BLUETOOTH", 0, btp.on ? "ON" : "OFF", btp.on, 0);
    bt_line(&v->line[1], btp.sel == 1u, "AUTOSTART", 0, set1.autostart ? "ON" : "OFF", 0, 0);
    bt_line(&v->line[2], btp.sel == 2u, "ROLE", 0, set1.role ? "DEVICE" : "HOST", 0, 0);
    bt_line(&v->line[3], btp.sel == 3u, "MODE", 0, BT_MODE_N[set1.mode & 3u], 0, 0);
    bt_line(&v->line[4], btp.sel == 4u, "RECONNECT", 0, BT_RATE_N[set1.rate % 3u], 0, 0);
    v->n = BT_SET_ROWS;
    if (!set1.role) {                               /* the saved devices and ADD, BT_VIS lines in view */
        uint32_t lines = n + 1u - btp.top < BT_VIS ? n + 1u - btp.top : BT_VIS;
        v->rule = BT_SET_ROWS;
        for (i = 0; i < lines; i++) {
            uint32_t k = btp.top + i;
            int sel = btp.sel == BT_SET_ROWS + k;
            if (k < n && bt_devs_get(k, &d)) {
                int ds = bt_devs_state(k);
                bt_dev_label(&d, lb);
                bt_line(&v->line[v->n++], sel, lb, d.kind == BT_KIND_FM1 ? "FM-1" : "CTRL", bt_dev_value(ds), ds == 2,
                        ds == 2 ? 2 : 1);
            } else {
                bt_line(&v->line[v->n++], sel, "+ ADD A DEVICE", 0, "SCAN", btp.on, 0);
            }
        }
    }
    if (btp.confirm)
        bt_cpy(v->keys, "OCT+ FORGET  OCT- KEEP", sizeof v->keys);
    else if (btp.sel >= BT_SET_ROWS && btp.sel - BT_SET_ROWS < n)
        bt_cpy(v->keys, "KNOB LEFT FORGET  OCT+ USE", sizeof v->keys);
    else
        bt_cpy(v->keys, "PRESETS ROW  KNOB  OCT+ OK", sizeof v->keys);
    return v;
}
