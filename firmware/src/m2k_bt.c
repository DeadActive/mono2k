/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI: 2026 DEADACTIVE */
/* M2K_BT (the FM-1's Bluetooth build, tools/fm1/build.py FELUCCA_BLE=1; m2k_core.c includes it last): the core's side
 * of Bluetooth. The Bluetooth itself (the BLE unit, the page's rows and what they do, the saved devices, MIDI in and
 * out, FM-Link) is the platform's, firmware/fm1/bt_*.c; between them the M2K_BT part of m2k.h. Here:
 *   - the machine marks' frames, run-length coded in this build (tools/fm1/gen_icons_rle.py: 65 KB of flash for 117),
 *     a frame decoded where it is drawn;
 *   - the frame shown as its negative by the platform (no fourth frame buffer: 7.2 KB of RAM);
 *   - the Bluetooth page, GLO's sixth (BT), drawn in MONO2K's look from the platform's view of it, and its keys
 *     (PRESETS the cursor, KNOB 1..4 the row, OCT- / OCT+) handed over while it shows;
 *   - the header's BT (off: none; on, nothing linked: struck; a link up: solid);
 *   - GLO > MIDI's SYNC IN in four (OFF, ON, BLE, LINK: the platform keeps the choice, S_SYNI says ON or OFF to it);
 *   - the M-VAVE SMC-Mixer's DAW layout on MONO2K's six tracks (m2k_smc_*). */

/* ------------------------------------------------------------------------------------- the marks' frames */

static const uint8_t *icon_frame(int m, int f)
{
    static uint8_t out[ICON_N * ICON_N / 8];
    const uint8_t *s = ICON_RLE + ICON_RLE_AT[m * ICON_FRAMES + f], *e = ICON_RLE + ICON_RLE_AT[m * ICON_FRAMES + f + 1];
    uint32_t n = 0;
    while (n < sizeof out && s < e) {
        uint32_t c = *s++, k = (c & 0x3Fu) + 1u;
        if (c < 0x80u) {
            k = c + 1u;
            if (k > sizeof out - n) k = sizeof out - n;
            m_cpy(out + n, s, k);
            s += c + 1u;
        } else {
            if (k > sizeof out - n) k = sizeof out - n;
            m_set(out + n, c < 0xC0u ? 0 : 0xFF, k);
        }
        n += k;
    }
    if (n < sizeof out) m_set(out + n, 0, sizeof out - n);
    return out;
}

/* ------------------------------------------------------------------------------------- the platform's */

int m2k_frame_negative(void) { return store_.set[S_SCREEN] >= 64; }
void m2k_toast(const char *a, const char *b) { toast(a, b); }

/* ------------------------------------------------------------------------------------- the page */

#define BT_SUB 5                       /* GLO's sixth page */
static uint8_t bt_open;                /* the platform heard it opened */

static int bt_page_up(void) { return ui.page == PG_GLO && ui.sub == BT_SUB; }

static void bt_page_track(void)        /* (m2k_frame, and before the page's keys) */
{
    int up = bt_page_up();
    if (up != bt_open) {
        bt_open = (uint8_t)up;
        m2k_bt_page(up);
    }
}

/* PRESETS, KNOB 1..4 while the page shows (and GLO is not held: its combos are its own): the page's */
__attribute__((noinline)) static int bt_page_enc(int e, int d)
{
    bt_page_track();
    if (!bt_open || held(B_GLO) || held(B_HOME) || held(B_SEQ) || ui.btn[B_EDIT]) return 0;
    if (e == E_PRESET) m2k_bt_input(d, 0, 0);
    else if (e >= E_K1 && e <= E_K4) m2k_bt_input(0, d, 0);
    else return 0;
    return 1;
}
__attribute__((noinline)) static int bt_page_btn(int b, int down)          /* OCT- / OCT+: the page's (presses; their releases dropped) */
{
    bt_page_track();
    if (!bt_open || (b != B_OCTD && b != B_OCTU) || held(B_GLO) || held(B_HOME) || held(B_SEQ) || ui.btn[B_EDIT])
        return 0;
    if (down) m2k_bt_input(0, 0, b == B_OCTU ? 2 : 1);
    return 1;
}

#define BT_Y0 18                       /* the state line, under the header */
#define BT_LH 13                       /* a line: the small type (11) and 2 */
#define BT_RY (BT_Y0 + 2 * BT_LH + 3)  /* the rows */
#define BT_KY 227                      /* the key line */
_Static_assert(BT_RY + M2K_BT_LINES * BT_LH + 2 <= BT_KY - 3 - BT_LH, "the Bluetooth page: its lines over the timing line");

/* a line of the small type within w: tracked if it fits, tight if that does, else cut short a character at a time
 * (a device's name may be long: never clipped through a letter) */
static void bt_fit(int x, int y, const char *s, int w, int ink)
{
    char b[40];
    int n;
    s_cpyn(b, s, sizeof b);
    n = 0;
    while (b[n]) n++;
    for (; n > 0 && text_w(&F_S, b, 0) > w; n--) b[n - 1] = 0;
    text(&F_S, x, y, b, ink, text_w(&F_S, b, 1) <= w);
}

static void bt_ring(int cx, int cy, int ink)
{
    disc(cx, cy, 3, ink);
    disc(cx, cy, 2, !ink);
}

/* the page over the knobs, the stage and the steps (the header stays) */
static void bt_page_draw(void)
{
    const m2k_bt_view_t *v = m2k_bt_view();
    int y, n = v->n < M2K_BT_LINES ? v->n : M2K_BT_LINES;
    rect(0, 16, 240, 224, 0);
    if (v->ask) {
        rect(0, BT_Y0 - 2, 240, BT_LH + 1, 1);
        bt_fit(4, BT_Y0, v->state, 232, 0);
    } else {
        bt_fit(4, BT_Y0, v->state, 232, 1);
    }
    bt_fit(4, BT_Y0 + BT_LH, v->dim, 232, 1);
    bt_fit(4, BT_KY - 3 - BT_LH, v->timing, 232, 1);
    hline(0, BT_RY - 3, 240, 1);
    for (int i = 0; i < n; i++) {
        const m2k_bt_line_t *l = &v->line[i];
        int ink = !l->sel, x = 4;
        y = BT_RY + i * BT_LH;
        if (v->rule && i == v->rule) hline(4, y - 2, 232, 1);
        if (l->sel) rect(0, y - 1, 240, BT_LH - 1, 1);
        if (l->dot == 2) { disc(x + 3, y + 5, 3, ink); x += 11; }
        else if (l->dot == 1) { bt_ring(x + 3, y + 5, ink); x += 11; }
        int vw = l->value[0] ? text_w(&F_S, l->value, 1) : 0, right = 236 - vw - (vw ? 8 : 0);
        if (l->tag[0]) {
            int tw = text_w(&F_S, l->tag, 1);
            text(&F_S, right - tw, y, l->tag, ink, 1);
            right -= tw + 8;
        }
        bt_fit(x, y, l->label, right - x, ink);
        if (vw) {
            if (l->lit && !l->sel) { rect(236 - vw - 3, y - 1, vw + 5, BT_LH - 1, 1); text(&F_S, 236 - vw, y, l->value, 0, 1); }
            else text(&F_S, 236 - vw, y, l->value, ink, 1);
        }
    }
    hline(0, BT_KY - 3, 240, 1);
    int kw = text_w(&F_S, v->keys, 1), kt = text_w(&F_S, v->keys, 0);
    if (kw <= 236) text(&F_S, (240 - kw) / 2, BT_KY, v->keys, 1, 1);
    else if (kt <= 236) text(&F_S, (240 - kt) / 2, BT_KY, v->keys, 1, 0);
    else bt_fit(2, BT_KY, v->keys, 236, 1);
}

/* the header's BT, at the right before the transport's tags (header): returns the width it took */
static int bt_head(int xr)
{
    int s = m2k_bt_icon(), w = text_w(&F_S, "BT", 1) + 6;
    if (!s) return 0;
    tag(xr - w, 0, "BT", s == 2);
    if (s == 1) line(xr - w + 1, 12, xr - 2, 2, 1);   /* struck: on, nothing linked */
    return w + 4;
}

/* ------------------------------------------------------------------------- GLO > MIDI: SYNC IN in four */

static void bt_sync_put(int c)                   /* a turn of SYNC IN: the platform's choice, S_SYNI ON or OFF to it */
{
    c = CLAMP(c, 0, 3);
    m2k_bt_sync_set(c);
    m2k_setting(S_SYNI, c == 1 || c == 2 ? 96 : 32);
}

/* -------------------------------------------------------------------------------- the SMC-Mixer (DAW) */
/* The M-VAVE SMC-Mixer in its DAW (Mackie Control) mode on MONO2K's six tracks (Felucca's smc.c, the reference's
 * layout, on this core):
 *   faders 1..6 (pitch bend, channels 1..6) the tracks' LEVEL (the kit's, as CC 7), with pickup; 7, 8 and the
 *     master: nothing;
 *   knobs 1..6 (CC 16..21, channel 1, sign-magnitude clicks) the tracks' PAN; knob 7 (CC 22) the tempo, a BPM a
 *     click; knob 8 (CC 23) the swing;
 *   buttons (notes on channel 1, presses): REC 0x00.. (the track selected, then REC as the panel's tap), SOLO 0x08..
 *     (on / off, the GLO + key solos' own), MUTE 0x10.., SELECT 0x18.. for strips 1..6 (7, 8: nothing); STOP 0x5D,
 *     PLAY 0x5E, REC 0x5F; BANK / CHANNEL < > 0x2E 0x30 / 0x2F 0x31 the previous / next track.
 * Out (m2k_smc_frame, m2k_bt_send): the LEDs as notes 127 / 0, only changes, all of them on a new link; each fader's
 * position as E0+n v v. */
#define SMC_FADERS 8u
#define SMC_LEDS 35u                   /* notes 0x00..0x1F, then 0x5D..0x5F */
static struct {
    uint32_t lit[2], known[2];         /* the LEDs as sent: [0] bit = note 0x00..0x1F, [1] bits 0..2 STOP PLAY REC */
    uint8_t val[SMC_FADERS], last[SMC_FADERS];
    uint8_t up, on, dark, picked, seen, pend;
} smc;

static uint32_t smc_fval(uint32_t n) { return n < M2K_TRACKS ? PAT(cur_pat)->kit[n][P_LEVL] & 127u : 0u; }

static void smc_fresh(void)
{
    smc.known[0] = smc.known[1] = 0;
    smc.picked = smc.seen = 0;
    smc.pend = 0xFFu;
    smc.dark = 0;
    for (uint32_t i = 0; i < SMC_FADERS; i++) smc.val[i] = (uint8_t)smc_fval(i);
    smc.on = 1;
}

static void smc_notice(uint32_t n)     /* moved by other means (the panel, a pattern): unpicked, its position sent */
{
    uint32_t v = smc_fval(n);
    if (v != smc.val[n]) {
        smc.val[n] = (uint8_t)v;
        smc.picked &= (uint8_t)~(1u << n);
        smc.pend |= (uint8_t)(1u << n);
    }
}

static void smc_fader(uint32_t n, uint32_t v)   /* an unpicked fader acts once within 2 of the value or across it */
{
    uint32_t b = 1u << n, s, crossed;
    if (n >= M2K_TRACKS) return;
    smc_notice(n);
    s = smc.val[n];
    crossed = (smc.seen & b) && ((smc.last[n] < s) != (v < s));
    smc.last[n] = (uint8_t)v;
    smc.seen |= (uint8_t)b;
    if (!(smc.picked & b)) {
        if (!crossed && (v + 2u < s || v > s + 2u)) return;
        smc.picked |= (uint8_t)b;
    }
    if (v == s) return;
    smc.val[n] = (uint8_t)v;
    smc.pend |= (uint8_t)b;
    PAT(cur_pat)->kit[n][P_LEVL] = (uint8_t)v;
    trk[n].eff[P_LEVL] = (uint8_t)v;
}

static void smc_say(int t, const char *what, const char *val)    /* "T2 PAN" "L12" */
{
    char a[16] = {'T', (char)('1' + t), ' ', 0};
    s_cpyn(a + 3, what, sizeof a - 3);
    toast(a, val);
}

static void smc_pan(uint32_t k, int32_t d)
{
    char o[8];
    uint8_t *v = &PAT(cur_pat)->kit[k][P_PAN];
    int pn;
    *v = (uint8_t)CLAMP((int32_t)*v + d, 0, 127);
    trk[k].eff[P_PAN] = *v;
    pn = (int)*v - 64;
    if (!pn) s_cpy(o, "C");
    else { o[0] = pn < 0 ? 'L' : 'R'; s_int(o + 1, pn < 0 ? -pn : pn); }
    smc_say((int)k, "PAN", o);
}

static void smc_rec(void)              /* REC as the panel's tap (m2k_ui.c's B_REC) */
{
    sq.rec ^= 1;
    if (sq.rec && !sq.playing) {       /* stopped: step entry from the page in view */
        ui.cursor = (uint8_t)(ui.spage * 16 < seq_len() ? ui.spage * 16 : 0);
        if (ui.grid) toast("REC", "SEQ: KEYS WRITE STEPS");
        else toast("STEP ENTRY", "KEYS WRITE, EDIT RESTS");
    }
    sq.fill = 0;
}

static void smc_button(uint32_t n)
{
    uint32_t k = n & 7u;
    if (n < 0x20u) {
        if (k >= M2K_TRACKS) return;                   /* strips 7, 8: nothing */
        if (n < 0x08u) {                               /* REC n: that track, then REC as the panel's tap */
            track_to((int)k);
            smc_rec();
        } else if (n < 0x10u) {
            sq.solo ^= (uint8_t)(1u << k);
            toast(sq.solo >> k & 1u ? "SOLO" : "SOLO OFF", (const char[]){'T', (char)('1' + k), 0});
        } else if (n < 0x18u) {
            m2k_mute((int)k);
        } else {
            track_to((int)k);
        }
        return;
    }
    switch (n) {
    case 0x5Eu: if (!sq.playing) bt_ui = 1; break;     /* PLAY: stopped -> play (FM-Link's, as the panel's) */
    case 0x5Du: if (sq.playing) bt_ui = 2; break;      /* STOP */
    case 0x5Fu: smc_rec(); break;                      /* REC: the track selected */
    case 0x2Eu: case 0x30u: track_to((ui.trk + M2K_TRACKS - 1) % M2K_TRACKS); break;
    case 0x2Fu: case 0x31u: track_to((ui.trk + 1) % M2K_TRACKS); break;
    default: break;                                    /* REW FF, the arrows: nothing */
    }
}

void m2k_smc_in(uint32_t pkt)
{
    uint32_t st = (pkt >> 8) & 0xFFu, ch = st & 15u, d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
    if (!smc.on) smc_fresh();
    switch (st >> 4) {
    case 0xEu:                                         /* fader ch + 1 (9: the master) */
        if (ch < SMC_FADERS) smc_fader(ch, (d2 << 7 | d1) >> 7);
        break;
    case 0xBu:                                         /* knobs; bit 6 = counter-clockwise */
        if (!ch && d1 >= 16u && d1 <= 23u && (d2 & 0x3Fu)) {
            int32_t c = d2 & 0x40u ? -(int32_t)(d2 & 0x3Fu) : (int32_t)(d2 & 0x3Fu);
            if (d1 < 16u + M2K_TRACKS) smc_pan(d1 - 16u, c);
            else if (d1 == 22u) m2k_tempo((int)c);
            else if (d1 == 23u) {
                uint8_t *g = &PAT(cur_pat)->glob[G_SWNG];
                char o[8];
                *g = (uint8_t)CLAMP((int32_t)*g + c, 0, 127);
                s_int(o, *g);
                toast("SWING", o);
            }
        }
        break;
    case 0x9u:
        if (!ch && d2) smc_button(d1);
        break;
    default: break;
    }
}

/* the platform: linked in SMC DAW mode (1), or no longer (0: the link lost, or another MODE while linked: then its
 * LEDs are turned off once, by m2k_smc_frame) */
void m2k_smc_link(int up)
{
    smc.dark = (uint8_t)(!up && smc.up && smc.on ? SMC_LEDS : 0u);
    smc.up = (uint8_t)(up != 0);
    smc.on = 0;
}

static int smc_send(uint32_t a, uint32_t b, uint32_t c)
{
    uint8_t m[3] = {(uint8_t)a, (uint8_t)b, (uint8_t)c};
    return m2k_bt_send(m, 3) == 1;
}

/* the platform's main loop, while the SMC DAW layout is the link's (up) or just left it (up 0 after a mode change) */
void m2k_smc_frame(void)
{
    uint32_t w[2], i, j, b;
    if (!smc.up) {                                     /* the layout left while linked: the LEDs off once */
        for (; smc.dark; smc.dark--)
            if (!smc_send(0x90u, SMC_LEDS - smc.dark < 32u ? SMC_LEDS - smc.dark : 0x5Du + 3u - smc.dark, 0)) return;
        return;
    }
    if (!smc.on) smc_fresh();
    for (i = 0; i < SMC_FADERS; i++) smc_notice(i);
    w[0] = (sq.rec ? 1u << ui.trk : 0u) | (uint32_t)(sq.solo & 0x3Fu) << 8 | (uint32_t)(PAT(cur_pat)->mute & 0x3Fu) << 16 |
           1u << (24u + ui.trk);
    w[1] = (sq.playing ? 2u : 1u) | (sq.rec ? 4u : 0u);
    for (j = 0; j < 2u; j++) {
        uint32_t dirty = ((w[j] ^ smc.lit[j]) | ~smc.known[j]) & (j ? 7u : 0xFFFFFFFFu);
        for (i = 0; dirty && i < 32u; i++) {
            b = 1u << i;
            if (!(dirty & b)) continue;
            if (!smc_send(0x90u, j ? 0x5Du + i : i, (w[j] & b) ? 127u : 0u)) return;
            smc.lit[j] = (smc.lit[j] & ~b) | (w[j] & b);
            smc.known[j] |= b;
            dirty &= ~b;
        }
    }
    for (i = 0; smc.pend && i < SMC_FADERS; i++)
        if ((smc.pend >> i) & 1u) {
            if (!smc_send(0xE0u + i, smc.val[i], smc.val[i])) return;
            smc.pend &= (uint8_t)~(1u << i);
        }
}
