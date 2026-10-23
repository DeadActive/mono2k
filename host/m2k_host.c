/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on a computer: renders the factory patterns to WAV, drives the controls through a script and saves the
 * screen as PBM frames, and measures the render time. Build and run:
 *   cc -O2 -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/host/m2k_host host/m2k_host.c
 *   build/host/m2k_host build/host */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#define M2K_TEXT_AUDIT 1                  /* every frame's text checked (ta_check) */
#include "../firmware/src/m2k_core.c"

static void wav_write(const char *path, const int16_t *s, int frames)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    uint32_t data = (uint32_t)frames * 4u, riff = 36 + data, sr = M2K_SR, br = sr * 4;
    uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
    uint32_t fmt_len = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&fmt_len, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f);
    fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    fwrite(s, 4, (size_t)frames, f);
    fclose(f);
}

static void pbm_write(const char *path, const uint8_t *fbuf)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P4\n240 240\n");
    fwrite(fbuf, 1, M2K_FB_BYTES, f);
    fclose(f);
}

typedef struct { int ms; char kind; int a, b; const char *shot; } ev_t;

/* the tour: kind b button (a, b = down), e encoder (a, delta), k key (a, down), s screenshot */
static const ev_t TOUR[] = {
    {900, 's', 0, 0, "00_boot"},
    {2400, 's', 0, 0, "01_home_stopped"},
    {2500, 'b', B_PLAY, 1, 0}, {2520, 'b', B_PLAY, 0, 0},
    {3400, 's', 0, 0, "02_home_playing"},
    {3500, 'e', E_K3, 4, 0}, {3560, 's', 0, 0, "03_home_tempo_roll"}, {3900, 's', 0, 0, "04_home_tempo"},
    {5600, 'e', E_SELECT, 1, 0},
    {5700, 'b', B_EDIT, 1, 0}, {5720, 'b', B_EDIT, 0, 0},
    {6300, 's', 0, 0, "05_syn_card"},
    {6400, 'e', E_K1, 3, 0}, {6460, 's', 0, 0, "06_syn_roll"}, {6800, 's', 0, 0, "07_syn_focus"},
    {7000, 'b', B_ENV, 1, 0}, {7020, 'b', B_ENV, 0, 0}, {7090, 's', 0, 0, "08_amp_dissolve"}, {7600, 's', 0, 0, "09_amp"},
    {7700, 'b', B_SEL, 1, 0}, {7720, 'b', B_SEL, 0, 0}, {8300, 's', 0, 0, "10_filt"},
    {8400, 'b', B_FX, 1, 0}, {8420, 'b', B_FX, 0, 0}, {8900, 's', 0, 0, "11_fx_eq"},
    {9000, 'b', B_FX, 1, 0}, {9020, 'b', B_FX, 0, 0}, {9500, 's', 0, 0, "12_fx_delay"},
    {9600, 'b', B_FX, 1, 0}, {9620, 'b', B_FX, 0, 0}, {9700, 'e', E_K2, 30, 0}, {10200, 's', 0, 0, "13_fx_destroy"},
    {10300, 'b', B_LFO, 1, 0}, {10320, 'b', B_LFO, 0, 0}, {11200, 's', 0, 0, "14_lfo"},
    {11300, 'b', B_GLO, 1, 0}, {11320, 'b', B_GLO, 0, 0}, {11900, 's', 0, 0, "15_glo_time"},
    {12000, 'b', B_GLO, 1, 0}, {12020, 'b', B_GLO, 0, 0}, {12600, 's', 0, 0, "16_glo_bus"},
    {12700, 'b', B_SEQ, 1, 0}, {12720, 'b', B_SEQ, 0, 0},
    {12800, 'b', B_EDIT, 1, 0}, {12820, 'b', B_EDIT, 0, 0},
    {13300, 'k', 7, 1, 0}, {13350, 'e', E_PRESET, 2, 0}, {13360, 'b', B_OCTU, 1, 0}, {13380, 'b', B_OCTU, 0, 0}, {13500, 's', 0, 0, "17a_grid_step_card"}, {13520, 'k', 7, 0, 0},
    {13600, 'k', 9, 1, 0}, {13700, 'e', E_K2, 20, 0}, {13800, 's', 0, 0, "17b_grid_lock"}, {14000, 'k', 9, 0, 0},
    {14200, 'e', E_ALGO, 1, 0}, {14260, 's', 0, 0, "18_machine_glitch"}, {14900, 's', 0, 0, "19_machine_card"},
    {15000, 'b', B_ARP, 1, 0}, {15020, 'b', B_ARP, 0, 0}, {15600, 's', 0, 0, "20_arp"},
    {15700, 'b', B_GLO, 1, 0}, {15720, 'b', B_GLO, 0, 0}, {15740, 'b', B_GLO, 1, 0}, {15760, 'b', B_GLO, 0, 0},
    {15780, 'b', B_GLO, 1, 0}, {15800, 'b', B_GLO, 0, 0}, {15820, 'b', B_GLO, 1, 0}, {15840, 'b', B_GLO, 0, 0},
    {16300, 's', 0, 0, "21_set_light"},
    {16400, 'e', E_K1, 1, 0}, {16900, 's', 0, 0, "22_set_dark"},
    {17000, 'b', B_HOME, 1, 0}, {17020, 'b', B_HOME, 0, 0}, {17600, 's', 0, 0, "23_home_dark"},
    {17700, 'b', B_EDIT, 1, 0}, {17720, 'b', B_EDIT, 0, 0}, {18400, 's', 0, 0, "24_syn_dark"},
    {18500, 'b', B_EDIT, 1, 0}, {18520, 'b', B_EDIT, 0, 0}, {18600, 'b', B_EDIT, 1, 0}, {18620, 'b', B_EDIT, 0, 0},
    {20300, 's', 0, 0, "26_syn_snd_dark"},
    {20400, 'x', 0, 0, 0},
};

/* the manual's scenes: B01's T1 with a trig of each of the eight types on every other step, in GRID (30); step 5's
 * FILTER trig held (31); the sound library, SYN > SND (25) */
static const ev_t MANUAL[] = {
    {2600, 'b', B_SEQ, 1, 0}, {2620, 'b', B_SEQ, 0, 0},
    {3400, 's', 0, 0, "30_grid_trig_types"},
    {5000, 'k', 7, 1, 0}, {5400, 's', 0, 0, "31_trig_filter"}, {5500, 'k', 7, 0, 0},
    {5600, 'b', B_SEQ, 1, 0}, {5620, 'b', B_SEQ, 0, 0},
    {5700, 'b', B_EDIT, 1, 0}, {5720, 'b', B_EDIT, 0, 0}, {5800, 'b', B_EDIT, 1, 0}, {5820, 'b', B_EDIT, 0, 0},
    {5900, 'b', B_EDIT, 1, 0}, {5920, 'b', B_EDIT, 0, 0}, {7200, 's', 0, 0, "25_syn_snd"},
    {7300, 'e', E_K4, 1, 0}, {8400, 's', 0, 0, "26_snd_kick"},
    {8500, 'b', B_GLO, 1, 0}, {9100, 's', 0, 0, "27_glo_help"}, {9200, 'b', B_ARP, 1, 0}, {9220, 'b', B_ARP, 0, 0},
    {9300, 'b', B_GLO, 0, 0}, {9400, 'b', B_HOME, 1, 0}, {9420, 'b', B_HOME, 0, 0},
    {9500, 'k', 0, 1, 0}, {9520, 'k', 0, 0, 0}, {9540, 'k', 4, 1, 0}, {9560, 'k', 4, 0, 0}, {10200, 's', 0, 0, "28_pads"},
    {10300, 'b', B_HOME, 1, 0}, {10400, 'k', 2, 1, 0}, {10420, 'k', 2, 0, 0}, {11000, 's', 0, 0, "29_home_held"},
    {11100, 'b', B_HOME, 0, 0}, {11200, 'b', B_EDIT, 1, 0}, {11900, 's', 0, 0, "29b_edit_held"}, {12000, 'b', B_EDIT, 0, 0},
    {12100, 'b', B_SEQ, 1, 0}, {12800, 's', 0, 0, "29c_seq_held"}, {12900, 'b', B_SEQ, 0, 0},
    {13000, 'b', B_HOME, 1, 0}, {13020, 'b', B_HOME, 0, 0}, {13600, 'b', B_GLO, 1, 0}, {13700, 'b', B_SEQ, 1, 0},
    {14300, 's', 0, 0, "29d_clear_ring"}, {14400, 'b', B_SEQ, 0, 0}, {14420, 'b', B_GLO, 0, 0},
    {14500, 'b', B_GLO, 1, 0}, {14520, 'b', B_GLO, 0, 0}, {14540, 'b', B_GLO, 1, 0}, {14560, 'b', B_GLO, 0, 0},
    {14580, 'b', B_GLO, 1, 0}, {14600, 'b', B_GLO, 0, 0}, {14620, 'b', B_GLO, 1, 0}, {14640, 'b', B_GLO, 0, 0},
    {15200, 'e', E_K3, 1, 0}, {15800, 's', 0, 0, "29e_ask"}, {15900, 'b', B_OCTD, 1, 0}, {15920, 'b', B_OCTD, 0, 0},
    {16000, 'b', B_PLAY, 1, 0}, {16020, 'b', B_PLAY, 0, 0}, {16100, 'b', B_FX, 1, 0}, {16200, 'k', 5, 1, 0},
    {16800, 's', 0, 0, "29f_perform"}, {16900, 'k', 5, 0, 0}, {16920, 'b', B_FX, 0, 0},
    {17000, 'b', B_LFO, 1, 0}, {17600, 's', 0, 0, "29g_lfo_held"}, {17700, 'b', B_LFO, 0, 0},
    {17800, 'b', B_GLO, 1, 0}, {18400, 's', 0, 0, "29h_glo_map"}, {18500, 'b', B_GLO, 0, 0},
    {18600, 'b', B_PLAY, 1, 0}, {18620, 'b', B_PLAY, 0, 0},
    {18700, 'x', 0, 0, 0},
};

static void manual_scene(void)
{
    m2k_init(2000);
    cur_pat = 16;
    sq.queued = 16;
    lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[16].kit[t], P_COUNT);
    for (int i = 0; i < 8; i++) pat[16].step[0][2 * i].flags = (uint8_t)(ST_TRIG | TTYPE[i]);
}

/* the text audit: in a frame just drawn, two texts whose ink touches (or is a dot apart) or a text cut short by the
 * screen or a clip. Each found once, with when; the count is what the tour returns */
static int ta_found;
static uint32_t ta_seen[256];
static int ta_nseen;
static void ta_report(const char *kind, const ta_t *a, const ta_t *b, int ms)
{
    uint32_t h = 2166136261u;
    for (const char *c = kind; *c; c++) h = (h ^ (uint8_t)*c) * 16777619u;
    for (const char *c = a->s; *c; c++) h = (h ^ (uint8_t)*c) * 16777619u;
    h = (h ^ (uint32_t)a->x0) * 16777619u;
    if (b) { for (const char *c = b->s; *c; c++) h = (h ^ (uint8_t)*c) * 16777619u; h = (h ^ (uint32_t)b->x0) * 16777619u; }
    for (int i = 0; i < ta_nseen; i++) if (ta_seen[i] == h) return;
    if (ta_nseen < 256) ta_seen[ta_nseen++] = h;
    ta_found++;
    if (b) printf("TEXT %s at %d ms: \"%s\" (%d,%d..%d,%d) and \"%s\" (%d,%d..%d,%d) [page %d.%d]\n", kind, ms, a->s, a->x0, a->y0,
                  a->x1, a->y1, b->s, b->x0, b->y0, b->x1, b->y1, ui.page, ui.sub);
    else printf("TEXT %s at %d ms: \"%s\" (%d,%d..%d,%d) [page %d.%d]\n", kind, ms, a->s, a->x0, a->y0, a->x1, a->y1, ui.page, ui.sub);
}

static void ta_check(int ms)
{
    if (ui.boot_t0 == 0xFFFFFFFFu || (uint32_t)ms - ui.boot_t0 < 2600u) return;      /* the boot splash is art */
    if (ui.trans_t && (uint32_t)ms - ui.trans_t < 400u) return;                       /* a glitch between pages */
    if (ui.toast_t && (uint32_t)ms - ui.toast_t < 130u) return;                       /* a message sweeping in */
    for (int i = 0; i < ta_n; i++) {
        const ta_t *a = &ta[i];
        if (a->cut) ta_report("cut", a, 0, ms);
        for (int k = ta_nwin - 1; k >= 0; k--)           /* begun in a window: it must end inside its frame */
            if (a->x0 > ta_win[k].x0 && a->y0 > ta_win[k].y0 && a->x0 < ta_win[k].x1 && a->y0 < ta_win[k].y1) {
                if (a->x1 > ta_win[k].x1 - 2 || a->y1 > ta_win[k].y1 - 2) ta_report("past its window", a, 0, ms);
                break;
            }
        for (int j = i + 1; j < ta_n; j++) {
            const ta_t *b = &ta[j];
            if (a->x1 < 0 || b->x1 < 0) continue;
            if (a->x0 <= b->x1 + 1 && b->x0 <= a->x1 + 1 && a->y0 <= b->y1 + 1 && b->y0 <= a->y1 + 1) ta_report("touch", a, b, ms);
        }
    }
}

/* every screen the knobs reach, for the text audit: each machine on T1, each page and its pages, at rest and with
 * each knob turned to its least, middle and most; a held step with every tag; the library's lists */
static int text_sweep(void)
{
    m2k_init(2000);
    uint32_t now = 4000;
    static int16_t b[256];
    for (int f = 0; f < 44100 * 3; f += 128) m2k_render(b, 128);
    m2k_frame(now);
    for (int m = 0; m < M_COUNT; m++) {
        ui.trk = 0;
        set_machine(m);
        for (int pg = 0; pg < PG_COUNT; pg++)
            for (int sub = 0; sub < nsub(pg); sub++) {
                ui.page = (uint8_t)pg; ui.sub = (uint8_t)sub;
                now += 3000; ui.trans_t = 0; ui.toast_t = 0; ui.card_t = 0; ui.focus_t = 0;
                m2k_frame(now); ta_check((int)now);
                for (int k = 0; k < 4; k++) {
                    int code = knob_param(pg, sub, k);
                    if (code == NONE) continue;
                    static const int V[3] = {0, 64, 127};
                    for (int vi = 0; vi < 3; vi++) {
                        if (code < 0x80) PAT(cur_pat)->kit[0][code] = (uint8_t)V[vi];
                        else if (code < 0xC0) PAT(cur_pat)->glob[code & 0x7F] = (uint8_t)V[vi];
                        ui.focus = (uint8_t)k; ui.focus_t = now; ui.roll_t = now - 2000;
                        now += 40; ui.trans_t = 0; ui.toast_t = 0;
                        m2k_frame(now); ta_check((int)now);
                    }
                }
            }
    }
    /* a held step in GRID with every tag: a condition, a ratchet, a trig type, accent and slide */
    m2k_init(2001);
    ui.page = PG_HOME; ui.grid = 1;
    m2k_step_t *st = &PAT(cur_pat)->step[0][3];
    st->flags = ST_TRIG | ST_ACC | ST_SLD | ST_NOFLT; st->cond = C_COUNT - 1; st->rtrg = 4; st->note = 61;
    ui.trk = 0; ui.held[0] = 3; ui.nheld = 1;
    now += 4000; ui.trans_t = 0; ui.toast_t = 0;
    m2k_frame(now); ta_check((int)now);
    ui.nheld = 0; ui.grid = 0;
    /* the library: every category shown */
    ui.page = PG_SYN; ui.sub = (uint8_t)(nsub(PG_SYN) - 1);
    for (int c = 0; c < CAT_COUNT; c++) { ui.snd_cat = (uint8_t)c; now += 3000; ui.trans_t = 0; ui.toast_t = 0; m2k_frame(now); ta_check((int)now); }
    printf("text sweep: %s\n", ta_found ? "TEXT TOUCHES OR IS CUT (above)" : "every text clear");
    return ta_found != 0;
}

/* events against the screen's clock, a screenshot where asked, the sound rendered between */
static void run_events(const ev_t *E, const char *out)
{
    char path[512];
    int16_t blkbuf[256];
    int ms = 0, ei = 0;
    for (int s = 0; ; s += 128) {
        int now = (int)((int64_t)s * 1000 / M2K_SR);
        while (E[ei].ms <= now) {
            const ev_t *e = &E[ei++];
            if (e->kind == 'x') return;
            if (e->kind == 'b') m2k_button(e->a, e->b);
            if (e->kind == 'e') m2k_encoder(e->a, e->b);
            if (e->kind == 'k') m2k_key(e->a, e->b);
            if (e->kind == 's') {
                snprintf(path, sizeof path, "%s/%s.pbm", out, e->shot);
                pbm_write(path, m2k_frame((uint32_t)now));
            }
        }
        m2k_render(blkbuf, 128);
        if (now - ms >= 16) { ms = now; m2k_frame((uint32_t)now); ta_check(now); }
    }
}

/* measure()'s render on the store as it stands (no init): a pattern built in place */
static void fx_measure(int p, int mask, double sec, int16_t *keep, double *pk_db, double *rms_db)
{
    cur_pat = p;
    sq.queued = (uint8_t)p;
    pat[p].mute = (uint8_t)(~mask & 0x3F);
    lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[p].kit[t], P_COUNT);
    seq_play(1);
    int frames = (int)(M2K_SR * sec) / 128 * 128;
    static int16_t buf[2 * 128];
    double ss = 0;
    int peak = 0;
    for (int f = 0; f < frames; f += 128) {
        m2k_render(buf, 128);
        for (int i = 0; i < 256; i++) { int v = buf[i], a = v < 0 ? -v : v; if (a > peak) peak = a; ss += (double)v * v; }
        if (keep) m_cpy(keep + 2 * f, buf, sizeof buf);
    }
    *pk_db = 20 * log10(peak / 32767.0 + 1e-9);
    *rms_db = 10 * log10(ss / (frames * 2.0) / (32767.0 * 32767.0) + 1e-12);
}

/* render seconds of pattern p with the tracks in mask; peak and rms (dB). demo_dry: without the bus's drive (a
 * part balanced under the knee before the bank's master drive) */
static int demo_dry;
static void measure(int p, int mask, double sec, int16_t *keep, double *pk_db, double *rms_db)
{
    m2k_init(2000);
    if (demo_dry) pat[p].glob[G_DRIV] = 0;
    cur_pat = p;
    sq.queued = (uint8_t)p;
    pat[p].mute = (uint8_t)(~mask & 0x3F);
    lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[p].kit[t], P_COUNT);
    seq_play(1);
    int frames = (int)(M2K_SR * sec) / 128 * 128;
    static int16_t buf[2 * 128];
    double ss = 0;
    int peak = 0;
    for (int f = 0; f < frames; f += 128) {
        m2k_render(buf, 128);
        for (int i = 0; i < 256; i++) {
            int v = buf[i], a = v < 0 ? -v : v;
            if (a > peak) peak = a;
            ss += (double)v * v;
        }
        if (keep) m_cpy(keep + 2 * f, buf, sizeof buf);
    }
    *pk_db = 20 * log10(peak / 32767.0 + 1e-9);
    *rms_db = 10 * log10(ss / (frames * 2.0) / (32767.0 * 32767.0) + 1e-12);
}

static int analyze(const char *out)
{
    char path[512];
    int bad = 0;
    printf("pattern   mix pk/rms   T1        T2        T3        T4        T5        T6     (rms dB, crest)\n");
    for (int p = 0; p < 16; p++) {             /* bank A: the demos */
        double pk, rms;
        double sec = 10.0;
        int frames = (int)(M2K_SR * sec) / 128 * 128;
        int16_t *keep = malloc((size_t)frames * 4);
        measure(p, 0x3F, sec, keep, &pk, &rms);
        snprintf(path, sizeof path, "%s/a%02d_%s.wav", out, p + 1, pat[p].name);
        for (char *c = path + strlen(out) + 1; *c; c++) if (*c == ' ') *c = '_';
        wav_write(path, keep, frames);
        free(keep);
        printf("A%02d %-7s %5.1f/%5.1f", p + 1, pat[p].name, pk, rms);
        if (pk > -0.3) { printf(" CLIP"); bad++; }
        for (int t = 0; t < M2K_TRACKS; t++) {
            double tp, tr;
            measure(p, 1 << t, 6.0, 0, &tp, &tr);
            const char *flag = tr < -48 ? "!quiet" : (tp - tr) > 30 ? "!click" : "";
            if (*flag) bad++;
            printf("  %5.1f/%2.0f%-6s", tr, tp - tr, flag);
        }
        printf("\n");
    }
    printf("%d flags\n", bad);
    return 0;
}

/* bank A's patterns for listening: each from its start, every track, for some seconds (the batch's slots, or all) */
static int demo_render(const char *out, int argc, char **argv)
{
    double sec = argc > 3 ? atof(argv[3]) : 30.0;
    char path[512];
    for (int p = 0; p < 16; p++) {
        int want = argc <= 4;
        for (int i = 4; i < argc; i++) want |= atoi(argv[i]) == p + 1;
        if (!want) continue;
        int frames = (int)(M2K_SR * sec) / 128 * 128;
        int16_t *keep = malloc((size_t)frames * 4);
        double pk, rms;
        measure(p, 0x3F, sec, keep, &pk, &rms);
        snprintf(path, sizeof path, "%s/a%02d_%s.wav", out, p + 1, pat[p].name);
        wav_write(path, keep, frames);
        free(keep);
        printf("A%02d %-7s %.0f s: peak %.1f dBFS, rms %.1f dBFS -> %s\n", p + 1, pat[p].name, sec, pk, rms, path);
    }
    return 0;
}

/* each track of bank A alone, two loops, and every trig it plays: the frame, the note the engine was given, the
 * track's parameters as they stood; then its most played note held on its kit (tools/demo_pitch.py measures each
 * against its note). The factory is rebuilt between tracks, so the held note's kit change does not stay */
/* a bank of 16 patterns as packed blobs (each: its length, two bytes LE, then pat_pack's bytes): written from bank A,
 * read back over bank A after every m2k_init, so a bank kept as a fixture plays on today's engine (converted on load) */
static uint8_t bank_fix[16 * BLOB_MAX];
static int bank_len;

static int bank_export(const char *path)
{
    static uint8_t bl[BLOB_MAX];
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    m2k_init(2000);
    for (int p = 0; p < 16; p++) {
        int n = pat_pack(&pat[p], bl);
        uint8_t h[2] = {(uint8_t)n, (uint8_t)(n >> 8)};
        fwrite(h, 1, 2, f);
        fwrite(bl, 1, (size_t)n, f);
    }
    fclose(f);
    printf("bank A: 16 patterns into %s\n", path);
    return 0;
}

static int bank_load(void)
{
    int o = 0;
    for (int p = 0; p < 16 && o + 2 <= bank_len; p++) {
        int n = bank_fix[o] | (bank_fix[o + 1] << 8);
        if (o + 2 + n > bank_len || !pat_unpack(&pat[p], bank_fix + o + 2, n)) return 0;
        o += 2 + n;
    }
    return 1;
}

static int pitch_dump(const char *out)
{
    char path[512];
    for (int p = 0; p < 16; p++) {
        m2k_init(2000);
        if (bank_len && !bank_load()) { fprintf(stderr, "the bank fixture does not load\n"); return 1; }
        int steps = pat[p].glob[G_LEN] < 1 ? 1 : pat[p].glob[G_LEN] > 64 ? 64 : pat[p].glob[G_LEN];
        int frames = (int)(2.0 * steps * M2K_SR * 15.0 / (30 + pat[p].glob[G_TMPO]) + M2K_SR) / 16 * 16;
        for (int t = 0; t < M2K_TRACKS; t++) {
            m2k_init(2000);
            if (bank_len) bank_load();
            cur_pat = p;
            sq.queued = (uint8_t)p;
            pat[p].mute = (uint8_t)(~(1 << t) & 0x3F);
            lockmap_build();
            for (int k = 0; k < M2K_TRACKS; k++) m_cpy(trk[k].eff, pat[p].kit[k], P_COUNT);
            seq_play(1);
            int16_t *keep = malloc((size_t)frames * 4);
            snprintf(path, sizeof path, "%s/p%02d_t%d.txt", out, p + 1, t + 1);
            FILE *ev = fopen(path, "w");
            if (!ev || !keep) { perror(path); return 1; }
            fprintf(ev, "# A%02d %s T%d machine %d bpm %d steps %d\n", p + 1, pat[p].name, t + 1, pat[p].machine[t],
                    30 + pat[p].glob[G_TMPO], steps);
            uint32_t seen = mon.trig[t];
            for (int f = 0; f < frames; f += 16) {
                m2k_render(keep + 2 * f, 16);
                if (mon.trig[t] != seen) {
                    fprintf(ev, "%d %d %d %u", f, trk[t].note, trk[t].vel, (unsigned)(mon.trig[t] - seen));
                    for (int k = 0; k < P_COUNT; k++) fprintf(ev, " %d", trk[t].eff[k]);
                    fprintf(ev, "\n");
                    seen = mon.trig[t];
                }
            }
            fclose(ev);
            snprintf(path, sizeof path, "%s/p%02d_t%d.wav", out, p + 1, t + 1);
            wav_write(path, keep, frames);
            /* and the track's most played note held 1.5 s on its kit, the sequencer stopped: the engine's own tuning */
            int count[128] = {0}, note = 60, best = 0;
            for (int s = 0; s < steps; s++)
                if ((pat[p].step[t][s].flags & ST_TRIG) && ++count[pat[p].step[t][s].note & 127] > best) {
                    best = count[pat[p].step[t][s].note & 127];
                    note = pat[p].step[t][s].note & 127;
                }
            m2k_init(2000);
            if (bank_len) bank_load();
            cur_pat = p;
            int held = (int)(1.5 * M2K_SR) / 16 * 16;
            pat[p].kit[t][P_HOLD] = 127;
            pat[p].kit[t][P_DEC] = 127;
            m2k_midi(0x90 | t, note, 100);
            for (int f = 0; f < held; f += 16) m2k_render(keep + 2 * f, 16);
            snprintf(path, sizeof path, "%s/p%02d_t%d_held%d.wav", out, p + 1, t + 1, note);
            wav_write(path, keep, held);
            free(keep);
        }
    }
    printf("bank A: 16 patterns x %d tracks into %s\n", M2K_TRACKS, out);
    return 0;
}

#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); fails++; } else printf("ok   %s\n", m); } while (0)

/* B01, the blank kit, opened as the panel opens a pattern: its locks and every track's settings (A01's would stay
 * otherwise, and an LFO of A01's reaching across tracks with them) */
static void open_b01(void)
{
    cur_pat = 16;
    lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[16].kit[t], P_COUNT);
}

/* the bus's tape at a TAPE value on a held 1047 Hz sine: the pitch per cycle (between interpolated rising zero
 * crossings), its slow part (a 159 ms average: one flutter period) and the rest; peak deviation in cents of each */
static void tape_pitch_at(int tape, double *wow_c, double *flut_c)
{
    m2k_init(7);
    open_b01();
    pat[16].machine[0] = M_GSIN;
    pat[16].kit[0][P_SYN + 3] = 0;                 /* no pitch envelope */
    pat[16].kit[0][P_SYN + 5] = 30;                /* the click the bus's references were measured with (GND-SIN's init before 12) */
    pat[16].kit[0][P_HOLD] = 127;
    pat[16].kit[0][P_DEC] = 127;
    pat[16].glob[G_TAPE] = (uint8_t)tape;
    m2k_midi(0x90, 84, 100);
    const int frames = 5 * M2K_SR / 16 * 16;                   /* whole blocks: the render writes 16 at a time */
    int16_t *b = malloc((size_t)frames * 4);
    for (int f = 0; f < frames; f += 16) m2k_render(b + 2 * f, 16);
    static double hz[8000];
    int n = 0;
    double last = -1;
    for (int i = M2K_SR; i < frames - 1 && n < 8000; i++) {
        double a = b[2 * i], c = b[2 * i + 2];
        if (a < 0 && c >= 0) {
            double x = i + a / (a - c);
            if (last > 0) hz[n++] = M2K_SR / (x - last);
            last = x;
        }
    }
    free(b);
    int w = (int)(0.159 * 1047);
    double mean = 0;
    for (int i = 0; i < n; i++) mean += hz[i];
    mean /= n;
    *wow_c = *flut_c = 0;
    for (int i = w; i < n - w; i++) {
        double s = 0;
        for (int k = -w / 2; k < w / 2; k++) s += hz[i + k];
        s /= 2 * (w / 2);
        double cw = fabs(1200 * log2(s / mean)), cf = fabs(1200 * log2(hz[i] / s));
        if (cw > *wow_c) *wow_c = cw;
        if (cf > *flut_c) *flut_c = cf;
    }
}

/* ------------------------------------------------------------------------------------- the bus measured */

/* the reference the bus's effects are measured on (B01; nothing left to chance, so two renders differ only by the
 * control): with tracks, a kick on the beat, eighth hats, a saw bass line and a held chord, at 120; without, the
 * sequencer running on silence (the tape rolls: hiss and dust, and the glitch's clock) */
static void bus_ref(int tracks, int ctl, int v, int gsiz)
{
    m2k_init(2000);
    m2k_pattern_t *p = &pat[16];
    p->glob[G_TMPO] = 120 - 30;
    p->glob[G_LEN] = 16;
    p->glob[G_VERB] = 0;
    p->glob[G_GSIZ] = (uint8_t)gsiz;
    if (tracks) {
        static const uint8_t M[4] = {M_GSIN, M_GPRC, M_SAW, M_ENS}, LEV[4] = {87, 55, 87, 66};
        for (int t = 0; t < 4; t++) { kit_default(p, t, M[t]); p->kit[t][P_LEVL] = LEV[t]; p->kit[t][P_VOL] = 127; }
        p->kit[0][P_SYN + 3] = 60; p->kit[0][P_SYN + 4] = 40; p->kit[0][P_SYN + 5] = 30;   /* PDEP PDEC CLIK (page 12) */
        p->kit[0][P_DEC] = 30;
        p->kit[1][P_SYN] = 112; p->kit[1][P_DEC] = 12; p->kit[1][P_BASE] = 40;
        p->kit[2][P_DEC] = 44; p->kit[2][P_WDTH] = 40;
        p->kit[3][P_HOLD] = 127; p->kit[3][P_DEC] = 127; p->kit[3][P_GATE] = 127; p->kit[3][P_WDTH] = 40;
        static const uint8_t BASS[16] = {36, 0, 0, 36, 0, 0, 43, 0, 0, 0, 46, 0, 0, 0, 0, 0};
        for (int s = 0; s < 16; s++) {
            if (s % 4 == 0) p->step[0][s] = (m2k_step_t){.note = 36, .flags = ST_TRIG};
            if (s % 2 == 1) p->step[1][s] = (m2k_step_t){.note = 60, .flags = ST_TRIG};
            if (BASS[s]) p->step[2][s] = (m2k_step_t){.note = BASS[s], .flags = ST_TRIG};
        }
        p->step[3][0] = (m2k_step_t){.note = 60, .flags = ST_TRIG};
    }
    p->glob[ctl] = (uint8_t)v;
    cur_pat = 16;
    sq.queued = 16;
    lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, p->kit[t], P_COUNT);
    seq_play(1);
}

/* every value of the bus's controls, measured on the engine's output (DIR/bus.tsv; the hiss as WAVs, separated from
 * its crackle by tools/bus_curves.py):
 *   GLCH  repeats started per bar and the share of the time the mix is replaced (the engine's glitch flag), 256 bars
 *         of the sequencer on silence at 120, repeats of a sixteenth
 *   ADPC  DRIV  the reference mix through the control against itself without: the gain (least squares) and what is
 *         left over it (dB under the clean part), the output's rms and peak
 *   TAPE  wow and flutter (cents, a held 1047 Hz sine), the loss at 8.4 kHz (a held sine, dB) */
static int bus_measure(const char *dir)
{
    char path[512];
    snprintf(path, sizeof path, "%s/bus.tsv", dir);
    FILE *o = fopen(path, "w");
    if (!o) { perror(path); return 1; }
    fprintf(o, "ctl\tv\ta\tb\tc\td\n");
    /* the glitch: starts per bar, duty (256 bars: a chance's count wants many) */
    for (int v = 0; v < 128; v++) {
        bus_ref(0, G_GLCH, v, 64);
        const int bars = 256, frames = bars * 2 * M2K_SR / 16 * 16;
        int16_t lr[32];
        long starts = 0, on = 0, blocks = 0;
        int last = 0;
        for (int f = 0; f < frames; f += 16) {
            m2k_render(lr, 16);
            starts += mon.glitch && !last;
            on += mon.glitch;
            last = mon.glitch;
            blocks++;
        }
        fprintf(o, "GLCH\t%d\t%.4f\t%.3f\t0\t0\n", v, (double)starts / bars, 100.0 * on / blocks);
    }
    /* the hiss and dust: 12 s after a second's fade-in, as mono WAVs (the left: both carry the same) */
    {
        const int settle = M2K_SR / 16 * 16, frames = 12 * M2K_SR / 16 * 16;
        int16_t *b = malloc((size_t)frames * 4);
        for (int v = 0; v < 128; v++) {
            bus_ref(0, G_HISS, v, 64);
            int16_t lr[32];
            for (int f = 0; f < settle; f += 16) m2k_render(lr, 16);
            for (int f = 0; f < frames; f += 16) m2k_render(b + 2 * f, 16);
            for (int i = 0; i < frames; i++) b[i] = b[2 * i];
            snprintf(path, sizeof path, "%s/hiss_%03d.wav", dir, v);
            FILE *w = fopen(path, "wb");
            if (!w) { perror(path); return 1; }
            uint32_t data = (uint32_t)frames * 2u, riff = 36 + data, sr = M2K_SR, br = sr * 2, fl = 16;
            uint16_t pcm = 1, ch = 1, ba = 2, bits = 16;
            fwrite("RIFF", 1, 4, w); fwrite(&riff, 4, 1, w); fwrite("WAVEfmt ", 1, 8, w); fwrite(&fl, 4, 1, w);
            fwrite(&pcm, 2, 1, w); fwrite(&ch, 2, 1, w); fwrite(&sr, 4, 1, w); fwrite(&br, 4, 1, w);
            fwrite(&ba, 2, 1, w); fwrite(&bits, 2, 1, w); fwrite("data", 1, 4, w); fwrite(&data, 4, 1, w);
            fwrite(b, 2, (size_t)frames, w);
            fclose(w);
        }
        free(b);
    }
    /* the crunch and the drive against the clean mix */
    {
        const int frames = 8 * M2K_SR / 16 * 16;
        int16_t *ref = malloc((size_t)frames * 4), *b = malloc((size_t)frames * 4);
        static const int CTL[2] = {G_ADPC, G_DRIV};
        for (int c = 0; c < 2; c++)
            for (int v = 0; v < 128; v++) {
                bus_ref(1, CTL[c], v, 64);
                int16_t *dst = v ? b : ref;
                for (int f = 0; f < frames; f += 16) m2k_render(dst + 2 * f, 16);
                double xy = 0, xx = 0, yy = 0, pk = 0;
                for (int i = 2 * M2K_SR; i < 2 * frames; i++) {        /* past the first second */
                    double x = ref[i], y = dst[i];
                    xy += x * y; xx += x * x; yy += y * y;
                    if (fabs(y) > pk) pk = fabs(y);
                }
                double g = xy / xx, res = yy - 2 * g * xy + g * g * xx, n = 2.0 * frames - 2 * M2K_SR;
                fprintf(o, "%s\t%d\t%.3f\t%.2f\t%.2f\t%.2f\n", c ? "DRIV" : "ADPC", v, 20 * log10(g),
                        res > 0 ? 10 * log10(res / (g * g * xx)) : -150.0, 10 * log10(yy / n / (32767.0 * 32767.0)),
                        20 * log10(pk / 32767.0));
            }
        free(ref);
        free(b);
    }
    /* the tape: wow, flutter; the loss at 8.4 kHz */
    {
        const int frames = 2 * M2K_SR / 16 * 16;
        int16_t *b = malloc((size_t)frames * 4);
        double r0 = 0;
        for (int v = 0; v < 128; v++) {
            double w, fl;
            tape_pitch_at(v, &w, &fl);
            m2k_init(7);
            open_b01();
            pat[16].machine[0] = M_GSIN;
            pat[16].kit[0][P_SYN + 3] = 0;
            pat[16].kit[0][P_SYN + 5] = 30;
            pat[16].kit[0][P_HOLD] = 127;
            pat[16].kit[0][P_DEC] = 127;
            pat[16].glob[G_TAPE] = (uint8_t)v;
            m2k_midi(0x90, 120, 100);
            for (int f = 0; f < frames; f += 16) m2k_render(b + 2 * f, 16);
            double s = 0;
            for (int i = M2K_SR; i < 2 * frames; i++) s += (double)b[i] * b[i];
            if (!v) r0 = s;
            fprintf(o, "TAPE\t%d\t%.2f\t%.2f\t%.2f\t0\n", v, w, fl, 10 * log10(s / r0));
        }
        free(b);
    }
    fclose(o);
    printf("the bus's controls, every value: %s/bus.tsv (and the hiss's WAVs)\n", dir);
    return 0;
}

/* the trig types: step 1 trigs ALL, step 5 with SKIP masked; after step 5, the amp run's level (a short GND-SIN, silent
 * by then), the filter run's brightness against just before (a held SWAV-SAW, its gate open, its low-pass shut and
 * opened by its envelope, closed again by then) and whether LFO 1 started over (a slow rising ramp, TRIG) */
static void trig_probe(unsigned skip, double *amp_db, double *bright_db, int *lfo_restart)
{
    const int at = 4 * M2K_SR / 8;                                  /* step 5 at 120: 0.5 s */
    const int frames = (at + M2K_SR / 16) / 16 * 16;
    int16_t *b = malloc((size_t)frames * 4);
    for (int run = 0; run < 2; run++) {
        m2k_init(7);
        open_b01();
        m2k_pattern_t *p = &pat[16];
        p->glob[G_TMPO] = 120 - 30;
        p->glob[G_LEN] = 16;
        kit_default(p, 0, run ? M_SAW : M_GSIN);
        uint8_t *k = p->kit[0];
        k[P_ATK] = 0;
        if (run) { k[P_HOLD] = 127; k[P_DEC] = 127; k[P_GATE] = 127; k[P_BASE] = 0; k[P_WDTH] = 20; k[P_FATK] = 0; k[P_FDEC] = 30; k[P_WOF] = 127; }
        else { k[P_HOLD] = 0; k[P_DEC] = 30; k[P_BASE] = 0; k[P_WDTH] = 127; k[P_WOF] = 64; }
        k[P_LFO + L_TRCK] = 0; k[P_LFO + L_DEST] = P_PAN + 1; k[P_LFO + L_WAVE] = 92; k[P_LFO + L_TRIG] = 30;   /* ramp, TRIG */
        k[P_LFO + L_MULT] = 0; k[P_LFO + L_SPD] = 20; k[P_LFO + L_PHAS] = 0; k[P_LFO + L_DPTH] = 64;              /* 0.16 Hz, no depth */
        p->step[0][0] = (m2k_step_t){.note = 48, .flags = ST_TRIG};
        p->step[0][4] = (m2k_step_t){.note = 48, .flags = (uint8_t)(ST_TRIG | skip)};
        open_b01();
        seq_play(1);
        int32_t lfo0 = 0;
        for (int fr = 0; fr < frames; fr += 16) {
            if (fr == (at - M2K_SR / 100) / 16 * 16) lfo0 = mon.lfo[0][0];
            m2k_render(b + 2 * fr, 16);
        }
        if (!run) *lfo_restart = mon.lfo[0][0] < lfo0 / 2;
        /* the level after; the brightness as the share of the slope's energy in the signal's, after against before */
        double e0 = 0, e1 = 0, d0 = 0, d1 = 0;
        for (int i = at - M2K_SR / 16; i < at - M2K_SR / 100; i++) {
            double d = (double)b[2 * i] - b[2 * i - 2];
            e0 += (double)b[2 * i] * b[2 * i]; d0 += d * d;
        }
        for (int i = at + M2K_SR / 200; i < frames; i++) {
            double d = (double)b[2 * i] - b[2 * i - 2];
            e1 += (double)b[2 * i] * b[2 * i]; d1 += d * d;
        }
        if (run) *bright_db = 10 * log10(((d1 + 1) / (e1 + 1)) / ((d0 + 1) / (e0 + 1)));
        else *amp_db = 10 * log10(e1 / (frames - at - M2K_SR / 200) / (32768.0 * 32768.0) + 1e-12);
    }
    free(b);
}

/* the second core, played in turn: its job run when started (1) or when waited for (2) */
static int par_mode;
static void (*par_pending)(void);
static int par_test_begin(void (*job)(void)) { if (par_mode == 1) job(); else par_pending = job; return 1; }
static void par_test_end(void) { if (par_mode == 2 && par_pending) par_pending(); par_pending = 0; }

static int selftest(void)
{
    int fails = 0;
    double wow_c, flut_c;
    tape_pitch_at(127, &wow_c, &flut_c);
    printf("     tape at 127: wow +-%.1f c, flutter +-%.1f c\n", wow_c, flut_c);
    CHECK(wow_c > 25 && wow_c < 55 && flut_c < wow_c / 3,
          "tape: the wow drifts (25..55 c at TAPE 127), the flutter stays under a third of it");
    m2k_init(7);
    CHECK(!strcmp(pat[0].name, "MAMMOTH") && !strcmp(pat[2].name, "HAZE") && !strcmp(pat[16].name, "INIT"),
          "factory: A01 MAMMOTH, A03 HAZE, B01 INIT");
    CHECK(!strcmp(m2k_project_name(0), "DEMO") && !strcmp(m2k_project_name(1), "P2"), "projects: DEMO, P2");
    printf("     DEMO packed: %u bytes for 64 patterns (A01 %d bytes, B01 %d)\n", proj[0].used, proj[0].len[0], proj[0].len[16]);
    /* the pattern formats: older ones convert once, on their path; today's loads unchanged */
    {
        /* the demos are written on today's chain: none is converted (A03 HAZE's T6 DTIM 64 would become 127) */
        CHECK(pat[2].kit[5][P_DTIM] == 64 && pat[2].kit[0][P_DSND] == 64 && pat[2].kit[0][P_DIST] == 66,
              "format: the factory is written on today's chain, not converted");
        static m2k_pattern_t a, b, c;
        static uint8_t bl[BLOB_MAX], bl2[BLOB_MAX];
        /* a version-4 pattern: the old blank kit on two tracks, a few old settings, locks, trigs on one note */
        m_set(&a, 0, sizeof a);
        pat_init(&a, "OLD");
        kit_v4(&a, 0, M_GSIN);
        kit_v4(&a, 1, M_SAW);
        a.kit[0][P_DIST] = 34; a.kit[0][P_DTIM] = 8; a.kit[0][P_DFB] = 64; a.kit[0][P_BASE] = 40; a.kit[0][P_WDTH] = 30;
        a.kit[0][P_SYN + 2] = 77; a.kit[0][P_LFO + L_SPD] = 21; a.kit[0][P_BITS] = 9;
        a.kit[0][P_SYN] = 63; a.kit[1][P_SYN] = 63;                  /* GND-SIN's TUNE a semitone down; SWAV-SAW's A */
        for (int st = 0; st < 16; st += 4) { a.step[0][st].flags = ST_TRIG; a.step[0][st].note = 36; }
        lk(&a, 0, 4, P_DTIM, 16);
        lk(&a, 0, 8, P_SYN + 1, 99);
        lk(&a, 0, 12, P_SYN, 65);
        int n = pat_pack(&a, bl);
        CHECK(bl[1] == 14, "format: patterns are written as version 14");
        n = pat_pack_v(&a, bl, 4);
        CHECK(pat_unpack(&b, bl, n), "format: a version-4 pattern loads");
        CHECK(b.kit[0][P_DIST] == T_V5_DIST[34] && b.kit[0][P_DTIM] == 32 && b.kit[0][P_DSND] == 64 &&
              b.kit[0][P_DFB] == (64 * 270 * 64 + 16384) >> 15 && b.kit[0][P_VOL] == T_V5_VOL[100],
              "format: version 4 -> 5 converts the chain (DIST, DTIM, DSND, DFB, VOL)");
        CHECK(b.kit[0][P_BASE] == v5_base(T_V5_FPOS[40], 36), "format: the filter is placed against the track's note");
        /* (GND-SIN's page since version 12: TUNE on H, its other knobs one earlier, values as they were) */
        CHECK(b.kit[0][P_SYN + 1] == 77 && b.kit[0][P_LFO + L_SPD] == 21 && b.kit[0][P_BITS] == 9 &&
              !memcmp(b.kit[1], a.kit[1], 8) && b.step[0][4].note == 36 && b.step[0][4].flags == ST_TRIG,
              "format: machine, LFO and MONO2K's own settings, trigs untouched");
        int lock_ok = 0;
        for (int i = 0; i < M2K_LOCKS; i++) {
            if (b.lock[i].trk == 0 && b.lock[i].param == P_DTIM && b.lock[i].val == 64) lock_ok |= 1;
            if (b.lock[i].trk == 0 && b.lock[i].param == P_SYN + 0 && b.lock[i].val == 99) lock_ok |= 2;
        }
        CHECK(lock_ok == 3, "format: a chain lock converts, a machine lock does not");
        int tl = -1;
        for (int i = 0; i < M2K_LOCKS; i++) if (b.lock[i].trk == 0 && b.lock[i].param == P_SYN + 7) tl = b.lock[i].val;
        CHECK(b.kit[0][P_SYN + 7] == 32 && tl == 96 && b.kit[1][P_SYN] == 63,
              "format: version 5 -> 6 keeps GND-SIN's TUNE in semitones (-1: 32, a lock +1: 96), SWAV-SAW's A as it was");
        CHECK(pat_unpack(&c, bl, n) && !memcmp(&b, &c, sizeof b), "format: the conversion is deterministic");
        int n2 = pat_pack(&b, bl2);
        CHECK(pat_unpack(&c, bl2, n2) && !memcmp(b.kit, c.kit, sizeof b.kit) && !memcmp(b.lock, c.lock, sizeof b.lock),
              "format: version 6 loads unchanged (no second conversion)");
        n = pat_pack_v(&a, bl, 5);
        CHECK(pat_unpack(&c, bl, n) && c.kit[0][P_SYN + 7] == 32 && c.kit[0][P_DTIM] == 8,
              "format: version 5 takes only the TUNE");
        n = pat_pack_v(&a, bl, 3);
        CHECK(pat_unpack(&c, bl, n) && c.kit[1][0] == 0 && c.kit[1][7] == 64 && c.kit[0][P_DTIM] == 32,
              "format: version 3 takes SWAV-SAW's page and the chain");
        n = pat_pack(&pat[0], bl);
        CHECK(pat_unpack(&c, bl, n) && !memcmp(pat[0].kit, c.kit, sizeof c.kit), "format: a saved demo reloads as it was");
        /* no conversion moves a pitch: every demo saved and reloaded as version 4 and 5 keeps its notes, its PTCH,
         * its machines' pages and the locks on them (GND-SIN's page converts before 12, its locks with it, and no demo
         * moves its TUNE) */
        int moved = 0;
        for (int d = 0; d < 16; d++)
            for (int ver = 4; ver <= 5; ver++) {
                n = pat_pack_v(&pat[d], bl, ver);
                if (!pat_unpack(&c, bl, n)) { moved++; continue; }
                for (int t = 0; t < M2K_TRACKS; t++) {               /* SWAV-ENS's page converts at 7, FM+'s at 10 */
                    int conv = pat[d].machine[t] == M_ENS || pat[d].machine[t] == M_FMS || pat[d].machine[t] == M_FMD ||
                               pat[d].machine[t] == M_SID || pat[d].machine[t] == M_GSIN || pat[d].machine[t] == M_VOX;
                    moved += c.kit[t][P_PTCH] != pat[d].kit[t][P_PTCH] || (!conv && memcmp(c.kit[t], pat[d].kit[t], 8) != 0);
                    for (int s = 0; s < M2K_STEPS; s++)
                        moved += c.step[t][s].note != pat[d].step[t][s].note || c.step[t][s].flags != pat[d].step[t][s].flags;
                }
                for (int i = 0; i < M2K_LOCKS; i++) {
                    int m = pat[d].lock[i].trk != 0xFF ? pat[d].machine[pat[d].lock[i].trk] : M_ENS;
                    if (m != M_ENS && m != M_FMS && m != M_FMD && m != M_SID && m != M_GSIN && m != M_VOX && (pat[d].lock[i].param < 8 || pat[d].lock[i].param == P_PTCH))
                        moved += c.lock[i].val != pat[d].lock[i].val;
                }
            }
        CHECK(moved == 0, "format: versions 4 and 5 move no demo's notes, PTCH, machine page or their locks");
        /* SWAV-ENS onto the Monomachine's page (7): every old interval folds by octaves into -11..+12, keeping its
         * pitch class, exact from 1 to 12, 0 and 24 to +12; never OFF (61-63) nor the octave down (0-1) */
        int fold_ok = 1;
        for (int v = 0; v < 128; v++) {
            int old_i = (v * 25) >> 7, nv = ens_fold(v), c64 = T_ENS_PCH[nv], semi = c64 / 64;
            if (nv <= 1 || (nv >= 61 && nv <= 63) || c64 == ENS_OFF || c64 % 64 || ((semi - old_i) % 12 + 12) % 12 ||
                (old_i >= 1 && old_i <= 12 && semi != old_i) || ((old_i == 0 || old_i == 24) && semi != 12)) fold_ok = 0;
        }
        CHECK(fold_ok, "format: an old SWAV-ENS interval keeps its pitch class (exact 1..12, 0 and 24 to +12), never OFF");
        /* the bank (8): a version-7 kit (70 wide) loads with MONO2K's 32 and every value where it was; today's keeps it */
        {
            static m2k_pattern_t w, w2;
            static uint8_t wb[BLOB_MAX];
            m_cpy(&w, &pat[9], sizeof w);                             /* A10 SMOKE (no FM, no SID) */
            for (int t = 0; t < M2K_TRACKS; t++) if (w.machine[t] == M_GSIN) w.machine[t] = M_GPRC;   /* (no page that converts) */
            for (int t = 0; t < M2K_TRACKS; t++) for (int k = 0; k < P_WBNK; k++) w.kit[t][k] = (uint8_t)((t * 31 + k * 7) & 127);
            for (int t = 0; t < M2K_TRACKS; t++) w.kit[t][P_HOLD] = 127;  /* INF: HOLD's conversion (13) leaves it */
            int wn = pat_pack_v(&w, wb, 7);
            int same = pat_unpack(&w2, wb, wn);
            for (int t = 0; t < M2K_TRACKS; t++) same = same && !memcmp(w2.kit[t], w.kit[t], P_WBNK) && w2.kit[t][P_WBNK] == 0;
            CHECK(same, "format: version 7 -> 8 keeps every kit value, the bank MONO2K's 32");
            w.kit[3][P_WBNK] = 127;
            wn = pat_pack(&w, wb);
            CHECK(pat_unpack(&w2, wb, wn) && w2.kit[3][P_WBNK] == 127 && !memcmp(w2.kit, w.kit, sizeof w.kit),
                  "format: version 8 keeps the bank");
        }
        /* the bus's laws (9): every old value of each control goes to a new one that sounds as it (the old laws as the
         * code had them: GLCH o/256 a sixteenth, DRIV 256 + 10 o, TAPE o, ADPC its mix and its whole-sample rate,
         * HISS's crackle o / 2^18 a sample at (16 + o) / 64, its hiss o^2) */
        {
            int bad = 0;
            for (int o = 1; o < 128; o++) {
                int vg = T_V9_GLCH[o], vd = T_V9_DRIV[o], vt = T_V9_TAPE[o], va = T_V9_ADPC[o], vh = T_V9_HISS[o];
                if (fabs(log(T_GLCH_P[vg] / (256.0 * o))) > 0.025) bad++;
                if (fabs(20 * log10(T_DRIV[vd] / (256.0 + 10 * o))) > 0.15) bad++;
                if (o > 3 ? fabs(log(T_TAPE_Q8[vt] / (256.0 * o))) > 0.02 : vt != 1) bad++;
                if ((va * 2 > 127 ? 127 : va * 2) != (o * 2 > 127 ? 127 : o * 2) || T_ADPC_DIV[va] != 256 * (1 + (o > 64 ? (o - 64) / 22 : 0))) bad++;
                if (fabs(log(T_CRK_P[vh] / (8.0 * o))) > 0.025 || fabs(log(T_CRK_A[vh] / (16.0 * (16 + o)))) > 0.02 ||
                    20 * log10(T_HISS_G[vh] / (double)(o * o)) < -0.5) bad++;
            }
            CHECK(!bad, "format: version 8 -> 9, every old bus value to one that sounds as it (GLCH DRIV TAPE ADPC HISS)");
            static m2k_pattern_t w, w2;
            static uint8_t wb[BLOB_MAX];
            m_cpy(&w, &pat[16], sizeof w);
            w.glob[G_GLCH] = 30; w.glob[G_HISS] = 10; w.glob[G_ADPC] = 90; w.glob[G_DRIV] = 20; w.glob[G_TAPE] = 30;
            int wn = pat_pack_v(&w, wb, 8);
            int conv = pat_unpack(&w2, wb, wn) && w2.glob[G_GLCH] == 91 && w2.glob[G_HISS] == 64 && w2.glob[G_ADPC] == 96 &&
                       w2.glob[G_DRIV] == 81 && w2.glob[G_TAPE] == 78;
            wn = pat_pack(&w, wb);
            CHECK(conv && pat_unpack(&w2, wb, wn) && !memcmp(w2.glob, w.glob, G_COUNT),
                  "format: a version-8 pattern's bus converts on load (GLCH 30 -> 91 ...), today's loads as it is");
        }
        /* FM+'s pages (10): an old FM+STAT and FM+DYN (RAT1 DEP1 DEC1 RAT2 DEP2 DEC2 FDBK ALGO / SWEP) carried over */
        {
            static m2k_pattern_t w, w2;
            static uint8_t wb[BLOB_MAX];
            m_set(&w, 0, sizeof w);
            pat_init(&w, "OLDFM");
            w.machine[1] = M_FMS; w.machine[2] = M_FMD;
            /* STAT: ratio 1.41 (T_RATIO 4: a step and 1FIN), 2 (6); DEP1 static, DEP2, feedback */
            const uint8_t os[8] = {16, 30, 127, 24, 90, 40, 50, 0}, od[8] = {12, 80, 60, 40, 30, 127, 20, 96};
            m_cpy(&w.kit[1][P_SYN], os, 8);
            m_cpy(&w.kit[2][P_SYN], od, 8);                  /* DYN: ratio 1, 3.5; DEP1 decaying, SWEP up */
            lk(&w, 1, 3, P_SYN + 0, 37);                     /* RAT1 to 3.0 (T_RATIO 9) */
            lk(&w, 1, 5, P_SYN + 2, 40);                     /* DEC1: goes */
            lk(&w, 2, 7, P_SYN + 4, 70);                     /* DEP2, held: to 2ENV */
            lk(&w, 1, 9, P_SYN + 1, 20);                     /* STAT's DEP1: to 1ENV */
            lk(&w, 1, 11, P_SYN + 6, 40);                    /* its FDBK: to 1FB */
            w.kit[0][P_LFO + L_TRCK] = 1 * 128 / M2K_TRACKS + 1; w.kit[0][P_LFO + L_DEST] = P_SYN + 4 + 1;   /* T2's DEP2 */
            int wn = pat_pack_v(&w, wb, 9);
            int ok = pat_unpack(&w2, wb, wn);
            const uint8_t *s2 = &w2.kit[1][P_SYN], *d2 = &w2.kit[2][P_SYN];
            double r1 = T_FMR24[fr24(s2[0])] / 65536.0 * (192 + s2[1]) / 256;
            ok = ok && fabs(r1 / 1.41 - 1) < 0.01 && s2[1] != 64 && fr24(s2[4]) == 19 &&
                 s2[2] == near_u32(T_FMB, 0, 64, T_FMK[30]) && s2[5] == near_u32(T_FMB, 0, 127, T_FMK[90]) &&
                 s2[3] == near_u32(T_FMFB, 0, 127, T_FMK[50] >> 1) && s2[6] == 64 && s2[7] == 64;
            CHECK(ok, "format: version 9 -> 10, an old FM+STAT's ratios to the 24 steps (1FIN the rest), its depths and feedback");
            int vl = near_u32(T_FMDEC, 1, 63, T_ENV_COEF[60]);
            ok = d2[0] == 64 && d2[4] == 119 && d2[3] == 64 + vl && d2[1] > 64 && d2[5] == near_u32(T_FMDV, 0, 64, T_FMK[30]) &&
                 d2[2] == CLAMP(near_u32(T_FMDV, 0, 256, T_FMK[80]) - (vl < 32 ? 4 * vl : 128), 0, 127) && d2[7] == 64;
            CHECK(ok, "format: an old FM+DYN's ratios to /64 and squared /64, its decay to 1VEN, SWEP to 1FEN up");
            int lr = 0, ld = 0, l2 = 0, l1 = 0, lf = 0;
            for (int i = 0; i < M2K_LOCKS; i++) {
                const m2k_lock_t *l = &w2.lock[i];
                if (l->trk == 1 && l->step == 3) lr = l->param == P_SYN + 0 && fr24(l->val) == 21;
                if (l->trk == 1 && l->step == 5) ld = 1;
                if (l->trk == 2 && l->step == 7) l2 = l->param == P_SYN + 5 && l->val == near_u32(T_FMDV, 0, 64, T_FMK[70]);
                if (l->trk == 1 && l->step == 9) l1 = l->param == P_SYN + 2 && l->val == near_u32(T_FMB, 0, 64, T_FMK[20]);
                if (l->trk == 1 && l->step == 11) lf = l->param == P_SYN + 3 && l->val == near_u32(T_FMFB, 0, 127, T_FMK[40] >> 1);
            }
            CHECK(lr && !ld && l2 && l1 && lf && w2.kit[0][P_LFO + L_DEST] == P_SYN + 5 + 1,
                  "format: an FM lock goes with its parameter (RAT1 to 1FRQ, DEP2 to 2ENV), DEC1's goes; an LFO follows");
            static uint8_t wb2[BLOB_MAX];
            wn = pat_pack(&w2, wb);
            int wn2 = pat_unpack(&w, wb, wn) ? pat_pack(&w, wb2) : 0;
            CHECK(wn2 == wn && !memcmp(wb, wb2, (size_t)wn), "format: today's version loads unchanged");
            for (int t = 0; t < M2K_TRACKS; t++)             /* (a version-10 SID, GND-SIN or VOX-6 page is the old one: none) */
                if (w2.machine[t] == M_GSIN || w2.machine[t] == M_SID || w2.machine[t] == M_VOX) w2.machine[t] = M_GPRC;
            wn = pat_pack_v(&w2, wb, 10);                   /* and a version-10 pattern: its FM pages already the new ones */
            wn2 = pat_unpack(&w, wb, wn) ? pat_pack(&w, wb2) : 0;
            int wn3 = pat_pack(&w2, wb);
            CHECK(wn2 == wn3 && !memcmp(wb, wb2, (size_t)wn3), "format: version 10's FM pages load unconverted");
        }
        /* SID's page (11): an old SID (WAV1 PW PWM WAV2 DTN2 FINE MODE MIX) carried over */
        {
            static m2k_pattern_t w, w2;
            static uint8_t wb[BLOB_MAX];
            m_set(&w, 0, sizeof w);
            pat_init(&w, "OLDSID");
            w.machine[2] = M_SID;
            const uint8_t os[8] = {32, 16, 40, 0, 88, 64, 70, 100};  /* PUL, duty 1/8, PWM, -, +9 semi, -, RING, a MIX */
            m_cpy(&w.kit[2][P_SYN], os, 8);
            lk(&w, 2, 4, P_SYN + 0, 70);                             /* WAV1 to T+S: MIX */
            lk(&w, 2, 6, P_SYN + 5, 70);                             /* FINE: goes */
            w.kit[0][P_LFO + 8 + L_TRCK] = 2 * 128 / M2K_TRACKS + 1; w.kit[0][P_LFO + 8 + L_DEST] = P_SYN + 1 + 1;   /* T3's PW */
            int wn = pat_pack_v(&w, wb, 10);
            const uint8_t *k = &w2.kit[2][P_SYN];
            int ok = pat_unpack(&w2, wb, wn) && (k[3] * 5) >> 7 == 2 && k[0] == 48 && k[1] == 20 && k[2] >= 64 &&
                     k[4] >> 5 == 1 && k[5] < 64 && k[6] == 32 + 9 * 32 / 12 && k[7] == 64;
            int lw = 0, lf = 0;
            for (int i = 0; i < M2K_LOCKS; i++) {
                if (w2.lock[i].trk == 2 && w2.lock[i].step == 4) lw = w2.lock[i].param == P_SYN + 3 && (w2.lock[i].val * 5) >> 7 == 3;
                if (w2.lock[i].trk == 2 && w2.lock[i].step == 6) lf = 1;
            }
            CHECK(ok && lw && !lf && w2.kit[0][P_LFO + 8 + L_DEST] == P_SYN + 0 + 1,
                  "format: version 10 -> 11, an old SID's wave, duty, sweep, ring, interval to SID-6581's page; an LFO follows");
        }
        /* bank A is written on GND-SIN's old page and turned at build: every GND-SIN track's TUNE where it went, H, at
         * its centre (as every part has it) */
        {
            int gs = 0, gok = 0;
            m2k_init(2000);
            for (int d = 0; d < 16; d++)
                for (int t = 0; t < M2K_TRACKS; t++)
                    if (pat[d].machine[t] == M_GSIN) { gs++; gok += pat[d].kit[t][P_SYN + 7] == 64; }
            CHECK(gs > 0 && gok == gs, "factory: bank A's GND-SIN tracks on today's page (TUNE on H)");
        }
        /* and its HOLDs, written in 14.6 ms steps, in beats at each part's tempo: A01 MAMMOTH (164 BPM) T4's 263 ms
         * (18) is 23 steps of 11.4 ms, its locks 24 14 30 20 26 (350 .. 438 ms) 31 18 38 26 33 */
        {
            m2k_init(2000);
            int ok = pat[0].glob[G_TMPO] == 164 - 30 && pat[0].kit[3][P_HOLD] == 23, nl = 0;
            for (int i = 0; i < M2K_LOCKS; i++)
                if (pat[0].lock[i].trk == 3 && pat[0].lock[i].param == P_HOLD) {
                    int v = pat[0].lock[i].val;
                    nl++;
                    ok = ok && (v == 23 || v == 31 || v == 18 || v == 38 || v == 26 || v == 33);
                }
            CHECK(ok && nl > 0, "factory: bank A's HOLDs in beats at each part's tempo (A01's T4: 263 ms, 23 steps at 164)");
        }
        /* VOX-6 onto VO-6's page (14): the old vowel to the nearest VOC1 VOC2 (an E, 400 and 2100 Hz, lands within 8 %
         * of both), BRTH to VOIC; a VOWL lock to VOC1's, a BRTH lock to VOIC's, the rest gone; an LFO on BRTH moves to
         * VOIC, one on VIBR stops; a version-14 pattern as it is; A02 CUTUP's voice turned at build */
        {
            static m2k_pattern_t w, w2, w3;
            static uint8_t wb[BLOB_MAX];
            m_cpy(&w, &pat[2], sizeof w);                                         /* A03 HAZE */
            for (int i = 0; i < M2K_LOCKS; i++) w.lock[i].trk = 0xFF;
            w.machine[0] = M_VOX;
            const uint8_t ov[8] = {36, 70, 30, 64, 80, 70, 40, 20};              /* VOWL (an E) VSWP VTIM SHFT BRTH .. */
            m_cpy(&w.kit[0][P_SYN], ov, 8);
            lk(&w, 0, 2, P_SYN + 0, 0);                                           /* a VOWL lock: an A */
            lk(&w, 0, 3, P_SYN + 4, 127);                                         /* a BRTH lock */
            lk(&w, 0, 4, P_SYN + 5, 50);                                          /* a RESO lock: gone */
            w.kit[1][P_LFO + L_TRCK] = enc(0, M2K_TRACKS); w.kit[1][P_LFO + L_DEST] = P_SYN + 4 + 1;          /* on BRTH */
            w.kit[1][P_LFO + 8 + L_TRCK] = enc(0, M2K_TRACKS); w.kit[1][P_LFO + 8 + L_DEST] = P_SYN + 7 + 1;  /* on VIBR */
            w.kit[2][P_HOLD] = 40;                                                /* a version-13 HOLD: in beats already */
            int wn = pat_pack_v(&w, wb, 13);
            int ok = pat_unpack(&w2, wb, wn) && w2.kit[2][P_HOLD] == 40;
            const uint8_t *k = &w2.kit[0][P_SYN];
            double e1 = T_VO_F1HZ[k[0]] / 400.0, e2 = T_VO_F2HZ[k[1]] / 2100.0;
            ok = ok && e1 > 0.92 && e1 < 1.08 && e2 > 0.92 && e2 < 1.08 && k[0] == T_VOX14[36][0] && k[1] == T_VOX14[36][1];
            ok = ok && k[2] == 0 && k[3] == T_VOX14_BR[80] && k[3] > 0 && k[4] == 0 && k[5] == 64 && k[6] == 64 && k[7] == 64;
            int la = 0, lb = 0, lr = 0;
            for (int i = 0; i < M2K_LOCKS; i++) {
                const m2k_lock_t *l = &w2.lock[i];
                if (l->trk != 0) continue;
                if (l->step == 2) la = l->param == P_SYN + 0 && l->val == T_VOX14[0][0];
                if (l->step == 3) lb = l->param == P_SYN + 3 && l->val == T_VOX14_BR[127];
                if (l->step == 4) lr = 1;
            }
            ok = ok && la && lb && !lr && w2.kit[1][P_LFO + L_DEST] == P_SYN + 3 + 1 && w2.kit[1][P_LFO + 8 + L_DEST] == 0;
            int wn2 = pat_pack(&w2, wb);
            ok = ok && pat_unpack(&w3, wb, wn2) && !memcmp(&w3.kit[0][P_SYN], k, 8);
            m2k_init(2000);
            ok = ok && pat[1].machine[3] == M_VOX && pat[1].kit[3][P_SYN + 0] == T_VOX14[0][0] && pat[1].kit[3][P_SYN + 5] == 64;
            CHECK(ok, "format: version 13 -> 14, VOX-6 onto VO-6's page (the vowel to its F1 F2, BRTH to VOIC, their locks and"
                      " LFOs with them, the rest gone; its HOLD not turned again); version 14 as it is; CUTUP's voice turned at build");
        }
        /* HOLD in beats (13): a version-12 HOLD (14.6 ms a step) to the step nearest the time it held at the pattern's
         * tempo, its locks with it, INF kept; a version-13 pattern as it is */
        {
            static m2k_pattern_t w, w2;
            static uint8_t wb[BLOB_MAX];
            m_cpy(&w, &pat[2], sizeof w);                                         /* A03 HAZE */
            for (int i = 0; i < M2K_LOCKS; i++) w.lock[i].trk = 0xFF;
            w.glob[G_TMPO] = 164 - 30;                                            /* 164 BPM: a step 11.4 ms */
            w.kit[0][P_HOLD] = 18; w.kit[1][P_HOLD] = 0; w.kit[2][P_HOLD] = 127; w.kit[3][P_HOLD] = 120;
            lk(&w, 0, 4, P_HOLD, 40);
            w.machine[5] = M_GSIN;                                                /* a version-12 GND-SIN page: as it is */
            const uint8_t gs[8] = {11, 22, 33, 44, 55, 66, 77, 60};
            m_cpy(&w.kit[5][P_SYN], gs, 8);
            int wn = pat_pack_v(&w, wb, 12);
            int ok = pat_unpack(&w2, wb, wn) && !memcmp(&w2.kit[5][P_SYN], gs, 8);
            int lh = 0;
            for (int i = 0; i < M2K_LOCKS; i++) if (w2.lock[i].trk == 0 && w2.lock[i].step == 4) lh = w2.lock[i].param == P_HOLD && w2.lock[i].val == 51;
            /* 18 x 14.6 = 263 ms: 23 x 11.43 = 263; 40 x 14.6 = 584 ms: 51; 120 x 14.6 = 1752 ms: 153 steps, held at 126 */
            ok = ok && w2.kit[0][P_HOLD] == 23 && w2.kit[1][P_HOLD] == 0 && w2.kit[2][P_HOLD] == 127 && w2.kit[3][P_HOLD] == 126 && lh;
            int wn2 = pat_pack(&w2, wb);
            static m2k_pattern_t w3;
            ok = ok && pat_unpack(&w3, wb, wn2) && w3.kit[0][P_HOLD] == 23;
            w.glob[G_TMPO] = 60 - 30;                                             /* 60 BPM: a step 31.25 ms */
            wn = pat_pack_v(&w, wb, 12);
            ok = ok && pat_unpack(&w2, wb, wn) && w2.kit[0][P_HOLD] == 8;          /* 263 ms: 8 x 31.25 = 250 */
            CHECK(ok, "format: version 12 -> 13, HOLD from 14.6 ms a step to 1 / 32 beat at the pattern's tempo, its locks too,"
                      " INF kept, 126 at most; GND-SIN's page as it was; version 13 as it is");
        }
        /* GND-SIN onto the Monomachine's page (12): TUNE to H, the other knobs one earlier, values as they were; its
         * locks and an LFO aimed at it from another track go with them; another machine's page stays */
        {
            static m2k_pattern_t w, w2;
            static uint8_t wb[BLOB_MAX];
            m_cpy(&w, &pat[2], sizeof w);                                         /* A03 HAZE */
            w.machine[0] = M_GSIN; w.machine[1] = M_SAW; w.machine[3] = M_SID;
            const uint8_t og[8] = {60, 11, 22, 33, 44, 55, 66, 77};               /* TUNE FOLD SUB NOIS PDEP PDEC CLIK HARM */
            m_cpy(&w.kit[0][P_SYN], og, 8);
            m_cpy(&w.kit[1][P_SYN], og, 8);
            m_cpy(&w.kit[3][P_SYN], og, 8);                                       /* a version-11 SID page: today's */
            for (int i = 0; i < M2K_LOCKS; i++) w.lock[i].trk = 0xFF;
            lk(&w, 0, 2, P_SYN + 0, 70);                                          /* a TUNE lock */
            lk(&w, 0, 3, P_SYN + 6, 99);                                          /* a CLIK lock */
            lk(&w, 1, 3, P_SYN + 0, 50);                                          /* SWAV-SAW's A: stays */
            w.kit[2][P_LFO + L_TRCK] = enc(0, M2K_TRACKS); w.kit[2][P_LFO + L_DEST] = P_SYN + 4 + 1;   /* T3's LFO on T1's PDEP */
            w.kit[2][P_LFO + 8 + L_TRCK] = enc(1, M2K_TRACKS); w.kit[2][P_LFO + 8 + L_DEST] = P_SYN + 4 + 1;   /* and T2's E */
            int wn = pat_pack_v(&w, wb, 11);
            const uint8_t ng[8] = {11, 22, 33, 44, 55, 66, 77, 60};               /* FOLD SUB NOIS PDEP PDEC CLIK HARM TUNE */
            int ok = pat_unpack(&w2, wb, wn) && !memcmp(&w2.kit[0][P_SYN], ng, 8) && !memcmp(&w2.kit[1][P_SYN], og, 8) &&
                     !memcmp(&w2.kit[3][P_SYN], og, 8);
            int lt = 0, lc = 0, ls = 0;
            for (int i = 0; i < M2K_LOCKS; i++) {
                const m2k_lock_t *l = &w2.lock[i];
                if (l->trk == 0 && l->step == 2) lt = l->param == P_SYN + 7 && l->val == 70;
                if (l->trk == 0 && l->step == 3) lc = l->param == P_SYN + 5 && l->val == 99;
                if (l->trk == 1 && l->step == 3) ls = l->param == P_SYN + 0 && l->val == 50;
            }
            CHECK(ok && lt && lc && ls && w2.kit[2][P_LFO + L_DEST] == P_SYN + 3 + 1 && w2.kit[2][P_LFO + 8 + L_DEST] == P_SYN + 4 + 1,
                  "format: version 11 -> 12, GND-SIN's TUNE to H and the rest a knob earlier, its locks and an LFO with them;"
                  " SWAV-SAW's and SID-6581's pages as they were");
            static uint8_t wb2[BLOB_MAX];
            wn = pat_pack(&w2, wb);
            int wn2 = pat_unpack(&w, wb, wn) ? pat_pack(&w, wb2) : 0;
            CHECK(wn2 == wn && !memcmp(wb, wb2, (size_t)wn), "format: version 12 loads unchanged (no second conversion)");
        }
        /* the FX machines (MONO2K's own designs, no reference to hold them to): a SWAV-SAW on T1, out of the mix by its
         * VOL, into an FX on T2 opened with each of its trigs; CHORUS widens by WID, RING changes the sound by MIX, DYNAMIX
         * brings a soft hit up to a loud one */
        {
            static int16_t kb[4 * M2K_SR + 512];
            double pk, rm;
            const int fp = 47;                                         /* C16, a scratch slot */
            static const uint8_t GSRC[8] = {0, 0, 0, 40, 40, 30, 0, 64};    /* a GND-SIN source as these were written on */
            #define FXPAT(fxm, src, vl) do { m2k_init(2000); pat_init(&pat[fp], "FXT"); kit_default(&pat[fp], 0, src); kit_default(&pat[fp], 1, fxm); \
                if ((src) == M_GSIN) m_cpy(&pat[fp].kit[0][P_SYN], GSRC, 8); \
                pat[fp].kit[0][P_VOL] = 0; pat[fp].kit[1][P_HOLD] = 127; pat[fp].glob[G_DRIV] = 0; \
                for (int s_ = 0; s_ < 16; s_ += 4) { pat[fp].step[0][s_].flags = ST_TRIG; pat[fp].step[0][s_].note = 48; \
                    pat[fp].step[1][s_].flags = ST_TRIG; pat[fp].step[1][s_].note = 48; } (void)(vl); } while (0)
            double corr[2];
            for (int w = 0; w < 2; w++) {
                FXPAT(M_FXC, M_SAW, 0);
                pat[fp].kit[1][P_SYN + 5] = (uint8_t)(w ? 127 : 0);
                pat[fp].kit[1][P_SYN + 3] = 127;
                fx_measure(fp, 0x3F, 1.0, kb, &pk, &rm);
                double sl = 0, sr = 0, slr = 0;
                for (int i = M2K_SR / 4; i < M2K_SR; i++) { sl += (double)kb[2 * i] * kb[2 * i]; sr += (double)kb[2 * i + 1] * kb[2 * i + 1]; slr += (double)kb[2 * i] * kb[2 * i + 1]; }
                corr[w] = slr / sqrt(sl * sr + 1);
            }
            CHECK(corr[0] > 0.999 && corr[1] < 0.9, "FX-CHORUS: mono at WID 0, wide at 127");
            /* its feedback rings: a short pluck through it at MIX 127, the energy 80-250 ms after the hit against the
             * hit's first 25 ms, FB 127 (+0.9) against FB 64 (none); the Monomachine's metal clangs are made so */
            double fbring[2];
            for (int f = 0; f < 2; f++) {
                FXPAT(M_FXC, M_GSIN, 0);
                pat[fp].kit[0][P_HOLD] = 0; pat[fp].kit[0][P_DEC] = 10;
                pat[fp].kit[1][P_SYN + 3] = 127; pat[fp].kit[1][P_SYN + 4] = (uint8_t)(f ? 127 : 64);
                fx_measure(fp, 0x3F, 1.0, kb, &pk, &rm);
                double e0 = 0, e1 = 0;
                for (int i = 0; i < M2K_SR / 40; i++) e0 += (double)kb[2 * i] * kb[2 * i];
                for (int i = M2K_SR * 8 / 100; i < M2K_SR / 4; i++) e1 += (double)kb[2 * i] * kb[2 * i];
                fbring[f] = 10 * log10((e1 + 1) / (e0 + 1));
            }
            printf("     FX-CHORUS ring, 80-250 ms against the hit: FB 64 %.1f dB, FB 127 %.1f dB\n", fbring[0], fbring[1]);
            CHECK(fbring[1] > fbring[0] + 10, "FX-CHORUS: its feedback rings (+0.9 at FB 127)");
            /* FX-RING hears the track before after its VOL and its LEV, as the Monomachine's RINGMOD: the source
             * panned left, the FX right (its VOL 96 and LEV 127 unity); at MIX 0 and INP 45 (unity) the FX's side is the
             * source's, at MIX 127 it is
             * rung, and at the source's LEV 0 silent */
            double rl[3], rr[3];
            for (int c = 0; c < 3; c++) {
                FXPAT(M_FXR, M_GSIN, 0);
                pat[fp].kit[0][P_VOL] = 64; pat[fp].kit[0][P_PAN] = 0; pat[fp].kit[1][P_PAN] = 127;
                pat[fp].kit[0][P_LEVL] = pat[fp].kit[1][P_LEVL] = 127; pat[fp].kit[1][P_VOL] = 96;   /* the FX's own at unity */
                pat[fp].kit[0][P_HOLD] = 127; pat[fp].kit[0][P_GATE] = pat[fp].kit[1][P_GATE] = 127;  /* both held: steady */
                pat[fp].kit[1][P_SYN + 7] = 45; pat[fp].kit[1][P_SYN + 3] = (uint8_t)(c ? 127 : 0);
                if (c == 2) pat[fp].kit[0][P_LEVL] = 0;
                fx_measure(fp, 0x3F, 1.0, kb, &pk, &rm);
                double sl = 0, sr = 0;
                for (int i = M2K_SR / 4; i < M2K_SR; i++) { sl += (double)kb[2 * i] * kb[2 * i]; sr += (double)kb[2 * i + 1] * kb[2 * i + 1]; }
                rl[c] = 10 * log10(sl + 1); rr[c] = 10 * log10(sr + 1);
            }
            printf("     FX-RING, its side against the source's: MIX 0 %+.2f dB, MIX 127 %+.2f dB; the source at LEV 0 %.0f dB\n",
                   rr[0] - rl[0], rr[1] - rl[1], rr[2] - rr[1]);
            CHECK(fabs(rr[0] - rl[0]) < 1.0 && fabs(rr[1] - rr[0]) > 1.0 && rr[2] - rr[1] < -60,
                  "FX-RING: the source as it is at MIX 0 (INP 45), rung at 127, silent at the source's LEV 0");
            double spread[2];
            for (int c = 0; c < 2; c++) {
                FXPAT(M_FXD, M_GSIN, 0);
                for (int s_ = 0; s_ < 16; s_ += 4) pat[fp].step[0][s_].note = 36;
                pat[fp].kit[0][P_VOL] = 0;
                pat[fp].kit[1][P_SYN + 4] = (uint8_t)(c ? 127 : 0);      /* RAT */
                pat[fp].kit[1][P_SYN + 2] = 40;                          /* THRS */
                pat[fp].kit[1][P_SYN + 5] = (uint8_t)(c ? 90 : 0);       /* GAIN */
                pat[fp].kit[0][P_DEC] = 30;
                lk(&pat[fp], 0, 4, P_DIST, 46);                          /* the second and fourth hits lower at the source */
                lk(&pat[fp], 0, 12, P_DIST, 46);
                fx_measure(fp, 0x3F, 2.0, kb, &pk, &rm);
                int a0 = 0, a1 = 0;
                int step = M2K_SR / 8;                                   /* a sixteenth at the blank's 120 bpm, in frames */
                for (int i = 0; i < 2 * step; i++) { int v = abs(kb[2 * i]); if (v > a0) a0 = v; v = abs(kb[2 * (i + 4 * step)]); if (v > a1) a1 = v; }
                spread[c] = 20 * log10((a0 + 1.0) / (a1 + 1.0));
            }
            printf("     FX-DYNAMIX: loud against soft %.1f dB at RAT 0, %.1f dB at RAT 127\n", spread[0], spread[1]);
            CHECK(spread[1] < spread[0] - 6, "FX-DYNAMIX: brings a soft hit up to a loud one");
            /* the MOD effect's chorus across the stereo (MONO2K's own): a saw through it wide, a low sine through it still
             * in the middle (the side high-passed), the phaser and MIX 0 mono */
            double mc[4];
            for (int c = 0; c < 4; c++) {
                m2k_init(2000);
                pat_init(&pat[fp], "MOD");
                kit_default(&pat[fp], 0, c == 1 ? M_GSIN : M_SAW);
                pat[fp].kit[0][P_HOLD] = 127; pat[fp].glob[G_DRIV] = 0;
                pat[fp].kit[0][P_MODT] = enc(c == 2 ? 2 : 0, 3); pat[fp].kit[0][P_MODR] = 60;
                pat[fp].kit[0][P_MODD] = 127; pat[fp].kit[0][P_MODM] = (uint8_t)(c == 3 ? 0 : 100);
                for (int s_ = 0; s_ < 16; s_ += 4) {
                    pat[fp].step[0][s_].flags = ST_TRIG; pat[fp].step[0][s_].note = (uint8_t)(c == 1 ? 29 : 48);
                }
                fx_measure(fp, 0x3F, 1.0, kb, &pk, &rm);
                double sl = 0, sr = 0, slr = 0;
                for (int i = M2K_SR / 4; i < M2K_SR; i++) { sl += (double)kb[2 * i] * kb[2 * i]; sr += (double)kb[2 * i + 1] * kb[2 * i + 1]; slr += (double)kb[2 * i] * kb[2 * i + 1]; }
                mc[c] = slr / sqrt(sl * sr + 1);
            }
            printf("     MOD chorus L/R: a saw %.3f, an F1 sine %.4f; the phaser %.4f, MIX 0 %.4f\n", mc[0], mc[1], mc[2], mc[3]);
            CHECK(mc[0] < 0.9 && mc[1] > 0.99 && mc[2] > 0.999 && mc[3] > 0.999,
                  "MOD: the chorus wide on a saw, an F1 sine in the middle; the phaser and MIX 0 mono");
            /* GND-NOIS on the Monomachine's page: ST with STON across the stereo (L/R falls as ST rises), without it in
             * the middle; RED darkens (its top falls). GND-PERC, MONO2K's own noise, keeps the machine number the old
             * GND-NOIS had, so saved patterns load as they were */
            double gc[4], top[2];
            for (int c = 0; c < 6; c++) {
                static const uint8_t ST[6] = {127, 64, 127, 0, 0, 0}, STON[6] = {127, 127, 0, 0, 0, 0}, RED[6] = {0, 0, 0, 0, 0, 127};
                m2k_init(2000);
                pat_init(&pat[fp], "NOIS");
                kit_default(&pat[fp], 0, M_GNOI);
                pat[fp].kit[0][P_HOLD] = 127; pat[fp].glob[G_DRIV] = 0;
                pat[fp].kit[0][P_SYN + 0] = ST[c]; pat[fp].kit[0][P_SYN + 1] = RED[c]; pat[fp].kit[0][P_SYN + 2] = STON[c];
                for (int s_ = 0; s_ < 16; s_ += 4) { pat[fp].step[0][s_].flags = ST_TRIG; pat[fp].step[0][s_].note = 60; }
                fx_measure(fp, 0x3F, 1.0, kb, &pk, &rm);
                double sl = 0, sr = 0, slr = 0, hf = 0, all = 0;
                for (int i = M2K_SR / 4; i < M2K_SR; i++) {
                    sl += (double)kb[2 * i] * kb[2 * i]; sr += (double)kb[2 * i + 1] * kb[2 * i + 1]; slr += (double)kb[2 * i] * kb[2 * i + 1];
                    double d1 = (double)kb[2 * i] - kb[2 * i - 2];                /* the first difference: the top */
                    hf += d1 * d1; all += (double)kb[2 * i] * kb[2 * i];
                }
                if (c < 4) gc[c] = slr / sqrt(sl * sr + 1);
                if (c >= 4) top[c - 4] = 10 * log10(hf / (all + 1) + 1e-12);
            }
            printf("     GND-NOIS L/R: ST 127 STON %.3f, ST 64 STON %.3f, ST 127 alone %.4f, ST 0 %.4f; its top, RED 127 against 0: %.1f dB\n",
                   gc[0], gc[1], gc[2], gc[3], top[1] - top[0]);
            CHECK(gc[0] < 0.1 && gc[1] > 0.45 && gc[1] < 0.75 && gc[2] > 0.999 && gc[3] > 0.999 && top[1] < top[0] - 15,
                  "GND-NOIS: ST with STON across the stereo, without it in the middle; RED darkens");
            CHECK(M_GPRC == 1 && M_GNOI == M_COUNT - 1, "GND-PERC keeps the old GND-NOIS's number, GND-NOIS is the newest");
            #undef FXPAT
        }
        /* the bank on the screen: a DIGI-WAV track's SYN has a third page, BANK; the wave knob names the bank's waves */
        {
            m2k_init(7);
            open_b01();                                               /* T1 GND-SIN, T4 DIGI-WAV */
            kit_default(&pat[16], 3, M_WAV);
            int wt = -1;
            for (int t = 0; t < M2K_TRACKS; t++) if (pat[16].machine[t] == M_WAV) { wt = t; break; }
            ui.trk = (uint8_t)(wt < 0 ? 0 : wt);
            char v1[16], v2[16], n1[16], n2[16];
            const char *u;
            pat[16].kit[ui.trk][P_WBNK] = 0;
            fmt(pinfo(ui.trk, P_WBNK), ui.trk, 0, v1, &u);
            fmt(pinfo(ui.trk, P_SYN), ui.trk, 10, n1, &u);
            pat[16].kit[ui.trk][P_WBNK] = 127;
            fmt(pinfo(ui.trk, P_WBNK), ui.trk, 127, v2, &u);
            fmt(pinfo(ui.trk, P_SYN), ui.trk, 10, n2, &u);
            int three = nsub(PG_SYN) == 4 && knob_param(PG_SYN, 2, 0) == P_WBNK && knob_param(PG_SYN, 3, 0) == SETTING(18);
            ui.trk = 0;                                               /* T1: GND-SIN */
            int two = nsub(PG_SYN) == 3 && knob_param(PG_SYN, 2, 0) == SETTING(18) && knob_param(PG_SYN, 2, 2) == SETTING(20);
            CHECK(wt >= 0 && three && two && !strcmp(v1, "M2K") && !strcmp(v2, "M64") && !strcmp(n2, "OS06") && strcmp(n1, n2),
                  "ui: a wave machine's SYN has a BANK page (M2K / M64) before SND, its WAVE named from the bank; the others SND");
            m2k_init(7);
        }
        /* the owner's 64 as DIGI-WAV plays them, C4 held a second: the machine's own output (before the chain) stays in
         * 16 bits, no wave reaching the rail, and carries no DC (a mean over 261 cycles: a part cycle's worth at most) */
        {
            int over = 0, dcw = 0;
            long worst = 0;
            for (int k = 0; k < 64; k++) {
                m2k_init(7);
                open_b01();
                pat[16].machine[0] = M_WAV;
                static const uint8_t syn[8] = {0, 64, 127, 0, 0, 64, 0, 0};
                for (int j = 0; j < 8; j++) pat[16].kit[0][P_SYN + j] = syn[j];
                pat[16].kit[0][P_SYN] = (uint8_t)(2 * k);
                pat[16].kit[0][P_WBNK] = 127;
                pat[16].kit[0][P_HOLD] = 127;
                pat[16].kit[0][P_DEC] = 127;
                m2k_midi(0x90, 60, 100);
                int16_t lr[32];
                int pk = 0;
                long sum = 0, n = 0;
                for (int f = 0; f < M2K_SR; f += 16) {
                    m2k_render(lr, 16);
                    if (f < M2K_SR / 10) continue;                    /* past the onset */
                    for (int i = 0; i < M2K_SUB; i++) {
                        int v = blk[0][i];
                        pk = v > pk ? v : -v > pk ? -v : pk;
                        sum += v;
                        n++;
                    }
                }
                long mean = sum / n;
                over += pk >= 32767;
                dcw += mean > 300 || mean < -300;
                if (labs(mean) > worst) worst = labs(mean);
            }
            m2k_init(7);
            printf("     the owner's 64 played: %d at the rail, %d with DC left, the worst mean %ld\n", over, dcw, worst);
            CHECK(!over && !dcw, "digi-wav: the owner's 64 play in 16 bits, their DC out");
        }
        /* LEV's silence step by step, and LFOs off an ENS page: an old pattern made for it */
        {
            static m2k_pattern_t e, g;
            static uint8_t eb[BLOB_MAX];
            m_set(&e, 0, sizeof e);
            pat_init(&e, "ENSOLD");
            const uint8_t oldk[8] = {21, 36, 61, 30, 0, 64, 0, 0};             /* +4 +7 +11, LEV 0 */
            for (int t = 1; t <= 3; t++) { e.machine[t] = M_ENS; m_cpy(e.kit[t], oldk, 8); }
            e.kit[2][P_SYN + 6] = 100;                                          /* T3: LEV up, one trig locked to 0 */
            e.machine[4] = M_GSIN;
            for (int t = 1; t <= 3; t++) for (int st = 0; st < 16; st += 4) { e.step[t][st].flags = ST_TRIG; e.step[t][st].note = 48; }
            lk(&e, 1, 4, P_SYN + 6, 100); lk(&e, 1, 12, P_SYN + 6, 100);       /* T2: raised on 4 and 12 */
            lk(&e, 1, 12, P_SYN + 0, 26);                                      /* and an interval locked there (+5) */
            lk(&e, 2, 8, P_SYN + 6, 0);
            uint8_t *lf = &e.kit[0][P_LFO];                                     /* T1's LFOs: onto T2's PCH2, T5's A */
            lf[L_TRCK] = enc(1, M2K_TRACKS); lf[L_DEST] = P_SYN + 0 + 1; lf[L_DPTH] = 90;
            lf[8 + L_TRCK] = enc(4, M2K_TRACKS); lf[8 + L_DEST] = P_SYN + 0 + 1; lf[8 + L_DPTH] = 90;
            lf[16 + L_TRCK] = enc(1, M2K_TRACKS); lf[16 + L_DEST] = P_ATK + 1;
            lk(&e, 0, 4, P_LFO + 16 + L_DEST, P_SYN + 2 + 1);                 /* LFO 3 locked onto T2's PCH3 at step 4 */
            int en = pat_pack_v(&e, eb, 6);
            int ok_load = pat_unpack(&g, eb, en);
            int off2[16] = {0}, off3[16] = {0}, up12 = -1, lev_left = 0, lfo_dest_lock = -1;
            for (int i = 0; i < M2K_LOCKS; i++) {
                const m2k_lock_t *l = &g.lock[i];
                if (l->trk == 0xFF) continue;
                if (l->trk == 1 && l->param <= P_SYN + 2 && l->val == 62) off2[l->step]++;
                if (l->trk == 2 && l->param <= P_SYN + 2 && l->val == 62) off3[l->step]++;
                if (l->trk == 1 && l->step == 12 && l->param == P_SYN) up12 = l->val;
                if (l->trk >= 1 && l->trk <= 3 && l->param == P_SYN + 6) lev_left++;
                if (l->trk == 0 && l->param == P_LFO + 16 + L_DEST) lfo_dest_lock = l->val;
            }
            CHECK(ok_load && g.kit[1][P_SYN] == T_ENS_ET[4 + 10] && off2[0] == 3 && off2[8] == 3 && !off2[4] && !off2[12] &&
                  up12 == T_ENS_ET[5 + 10] && lev_left == 0,
                  "format: a chord LEV 0 silenced but locks raise keeps its voices there, OFF on the other trigs");
            CHECK(off3[8] == 3 && !off3[0] && !off3[4] && g.kit[3][P_SYN] == 62 && g.kit[3][P_SYN + 1] == 62,
                  "format: a LEV lock to 0 is an OFF trig; a chord never raised is OFF");
            CHECK(g.kit[0][P_LFO + L_DEST] == 0 && g.kit[0][P_LFO + 8 + L_DEST] == P_SYN + 7 + 1 && g.kit[0][P_LFO + 16 + L_DEST] == P_ATK + 1 &&
                  lfo_dest_lock == 0, "format: an LFO aimed at an ENS interval is cleared (also a step's lock onto one); others kept"
                  " (the one on T5's GND-SIN TUNE where TUNE went, H)");
        }
        /* and every chord of bank A as it stood at ea69d53 (the fixture, version 6) keeps its notes at every trig */
        {
            FILE *f = fopen("tools/ref/fixtures/bank_a_ea69d53.bin", "rb");
            static uint8_t fx[16 * BLOB_MAX];
            int fl = f ? (int)fread(fx, 1, sizeof fx, f) : 0, o = 0, chords = 0, kept = 0, exact_bad = 0, off_bad = 0;
            if (f) fclose(f);
            for (int d = 0; d < 16 && o + 2 <= fl; d++) {
                int n6 = fx[o] | (fx[o + 1] << 8);
                static m2k_pattern_t was, now;
                static uint8_t raw[BLOB_MAX];
                m_cpy(raw, fx + o + 2, (size_t)n6);
                raw[1] = 7;                                       /* read as is: the old page's values */
                int ok1 = pat_unpack(&was, raw, n6), ok2 = pat_unpack(&now, fx + o + 2, n6);
                o += 2 + n6;
                if (!ok1 || !ok2) { off_bad++; continue; }
                for (int t = 0; t < M2K_TRACKS; t++) {
                    if (was.machine[t] != M_ENS) continue;
                    for (int st = 0; st < M2K_STEPS; st++) {
                        if (!(was.step[t][st].flags & ST_TRIG)) continue;
                        uint8_t ok[8], nk[8];
                        m_cpy(ok, was.kit[t], 8); m_cpy(nk, now.kit[t], 8);
                        for (int i = 0; i < M2K_LOCKS; i++) {
                            if (was.lock[i].trk == t && was.lock[i].step == st && was.lock[i].param < 8) ok[was.lock[i].param] = was.lock[i].val;
                            if (now.lock[i].trk == t && now.lock[i].step == st && now.lock[i].param < 8) nk[now.lock[i].param] = now.lock[i].val;
                        }
                        unsigned pcw = 1, pcn = 1;                    /* the root's class: bit 0 */
                        for (int k = 0; k < 3; k++) {
                            int oi = (ok[k] * 25) >> 7, c64 = T_ENS_PCH[nk[k]];
                            if (ok[6]) pcw |= 1u << (oi % 12);
                            if (c64 == ENS_OFF) { if (ok[6]) off_bad++; continue; }
                            pcn |= 1u << (((c64 / 64) % 12 + 12) % 12);
                            if (oi >= 1 && oi <= 12 && c64 != oi * 64) exact_bad++;
                            if (nk[k] >= 61 && nk[k] <= 63) off_bad++;
                        }
                        chords++;
                        kept += pcw == pcn;
                    }
                }
            }
            printf("     bank A's SWAV-ENS trigs: %d, their notes kept: %d\n", chords, kept);
            CHECK(fl > 0 && chords > 0 && kept == chords && exact_bad == 0 && off_bad == 0,
                  "format: every chord of bank A (ea69d53) keeps its notes through version 7, none in OFF");
        }
    }
    /* the clipboard, a sound: track 6's copied with EDIT held + REC, pasted onto track 1 with EDIT + PLAY */
    {
        m2k_init(7);
        uint32_t now = 100000;
        #define TAP_B(bb) do { m2k_button(bb, 1); m2k_frame(now += 20); m2k_button(bb, 0); m2k_frame(now += 20); } while (0)
        #define GLO_B(bb) do { m2k_button(B_EDIT, 1); m2k_frame(now += 20); TAP_B(bb); m2k_button(B_EDIT, 0); m2k_frame(now += 20); } while (0)
        cur_pat = 0;
        m2k_track(5);
        GLO_B(B_REC);
        CHECK(!strcmp(ui.toast, "COPY") && !strcmp(ui.toast2, "T6 SOUND"), "clipboard: track 6's sound copied (T6 SOUND)");
        m2k_track(0);
        GLO_B(B_PLAY);
        CHECK(!strcmp(ui.toast, "T1 SOUND PASTED") && pat[0].machine[0] == pat[0].machine[5] &&
              !memcmp(pat[0].kit[0], pat[0].kit[5], P_COUNT), "clipboard: pasted onto track 1, machine and kit");
        #undef GLO_B
        #undef TAP_B
        m2k_init(7);                                             /* the paste is not saved: the tests after start clean */
    }
    /* every value's text fits the smallest buffer it is written to (12 bytes), on every page and machine */
    {
        int longest = 0;
        char buf[64];
        const char *u;
        const pi_t *sets[] = {PI_COMMON, PI_LFO, PI_MOD, PI_GLOB, PI_SET};
        const int counts[] = {P_LFO - 8, 8, 4, G_COUNT, 3};
        for (int m = 0; m < M_COUNT + 5; m++) {
            const pi_t *tab = m < M_COUNT ? MI[m].syn : sets[m - M_COUNT];
            int cnt = m < M_COUNT ? 8 : counts[m - M_COUNT];
            for (int i = 0; i < cnt; i++)
                for (int t = 0; t < M2K_TRACKS; t++)
                    for (int v = 0; v < 128; v++) {
                        memset(buf, 0, sizeof buf);
                        fmt(&tab[i], t, v, buf, &u);
                        int n = (int)strlen(buf) + 1;
                        if (n > longest) longest = n;
                    }
        }
        printf("     the longest value text: %d bytes\n", longest);
        CHECK(longest <= 12, "ui: every value's text fits in 12 bytes");
    }
    /* the sequencer's keys, as a hand plays them: pages, the length, step entry */
    {
        uint32_t now = 100000;
        #define TAP_B(bb) do { m2k_button(bb, 1); m2k_frame(now += 20); m2k_button(bb, 0); m2k_frame(now += 20); } while (0)
        int kp[4], kw0 = -1, kw1 = -1, kw2 = -1;
        for (int k = 0; k < M2K_KEYS; k++) {
            if (ui.kb[k] >= 6 && ui.kb[k] < 10) kp[ui.kb[k] - 6] = k;
            if (ui.kw[k] == 0) kw0 = k;
            if (ui.kw[k] == 2) kw1 = k;
            if (ui.kw[k] == 4) kw2 = k;
        }
        m2k_pattern(16);                                         /* B01 INIT, 16 steps */
        m2k_frame(now);
        TAP_B(B_SEQ);                                            /* GRID */
        m2k_key(kp[3], 1); m2k_frame(now += 50); m2k_key(kp[3], 0); m2k_frame(now += 50);
        CHECK(ui.spage == 0 && seq_len() == 16, "grid: P4 past a 16-step pattern's end keeps the steps in view");
        m2k_pattern_t keep = pat[16];
        m2k_key(kp[1], 1);
        for (int i = 0; i < 40; i++) m2k_frame(now += 20);       /* held 0.8 s */
        m2k_key(kp[1], 0); m2k_frame(now += 20);
        int same = 1;
        for (int t = 0; t < M2K_TRACKS; t++) for (int st = 0; st < 16; st++)
            if (memcmp(&pat[16].step[t][st + 16], &keep.step[t][st], sizeof keep.step[t][st])) same = 0;
        CHECK(seq_len() == 32 && ui.spage == 1 && same, "grid: holding P2 makes the pattern 32 steps, the beat going on");
        pat[16] = keep;
        TAP_B(B_SEQ);                                            /* KEYS */
        TAP_B(B_REC);                                            /* stopped: step entry */
        int c0 = ui.cursor;
        m2k_key(kw0, 1); m2k_key(kw0, 0);
        m2k_key(kw2, 1); m2k_key(kw2, 0);
        m2k_button(B_EDIT, 1); m2k_button(B_EDIT, 0);
        m2k_key(kw1, 1); m2k_key(kw1, 0);
        m2k_step_t *sp = pat[16].step[ui.trk];
        CHECK(sp[c0].note == 53 + kw0 && sp[c0 + 1].note == 53 + kw2 && !(sp[c0 + 2].flags & ST_TRIG) &&
              sp[c0 + 3].note == 53 + kw1 && (sp[c0 + 3].flags & ST_TRIG), "keys: step entry writes notes, EDIT a rest");
        TAP_B(B_REC);
        pat[16] = keep;
        m2k_pattern(0);
        m2k_frame(now += 20);
        #undef TAP_B
    }
    /* edit, reload */
    int v0 = pat[0].kit[0][P_DEC];
    pat[0].kit[0][P_DEC] = 3;
    pattern_reload();
    CHECK(pat[0].kit[0][P_DEC] == v0, "reload brings the saved pattern back");
    /* save a pattern, edit, reload: the saved edit stays */
    pat[0].kit[0][P_DEC] = 11;
    pattern_save();
    pat[0].kit[0][P_DEC] = 99;
    pattern_reload();
    CHECK(pat[0].kit[0][P_DEC] == 11, "save sets the reload point");
    /* the trig types (the Monomachine's trig tracks): each of the eight restarts exactly the amp, the filter's envelope
     * and the LFOs it names, and none of the others */
    {
        static const unsigned SK[8] = {0, ST_NOFLT | ST_NOLFO, ST_NOAMP | ST_NOLFO, ST_NOAMP | ST_NOFLT, ST_NOLFO, ST_NOFLT,
                                       ST_NOAMP, ST_TTYPE};
        int ok = 1;
        for (int i = 0; i < 8; i++) {
            double a, br; int lr;
            trig_probe(SK[i], &a, &br, &lr);
            int wa = !(SK[i] & ST_NOAMP), wf = !(SK[i] & ST_NOFLT), wl = !(SK[i] & ST_NOLFO);
            int good = (wa ? a > -40 : a < -90) && (wf ? br > 10 : br < 3) && lr == wl;
            printf("     trig %-7s amp %6.1f dB  filter %+5.1f dB  LFO %s%s\n", (const char *[]){"ALL", "AMP", "FILTER",
                   "LFO", "AMP+FLT", "AMP+LFO", "FLT+LFO", "NONE"}[i], a, br, lr ? "restarts" : "runs on", good ? "" : "  <- wrong");
            ok &= good;
        }
        CHECK(ok, "trig types: each restarts the amp, the filter's envelope and the LFOs it names, no other");
        m2k_init(7);
        open_b01();
        m2k_pattern_t *p = &pat[16];
        p->step[0][4] = (m2k_step_t){.note = 48, .flags = ST_TRIG};
        ui.grid = 1; ui.nheld = 1; ui.held[0] = 4; ui.trk = 0;
        m2k_encoder(E_MASTER, 1);
        int t1 = p->step[0][4].flags;
        m2k_encoder(E_MASTER, 1);
        m2k_encoder(E_ALGO, 1);                                     /* the accent / slide knob keeps the trig type */
        int t2 = p->step[0][4].flags;
        m2k_encoder(E_MASTER, -1); m2k_encoder(E_MASTER, -1); m2k_encoder(E_MASTER, -1);
        int t3 = p->step[0][4].flags & ST_TTYPE;
        ui.grid = 0; ui.nheld = 0;
        CHECK(t1 == (ST_TRIG | ST_NOFLT | ST_NOLFO) && t2 == (ST_TRIG | ST_ACC | ST_NOAMP | ST_NOLFO) && t3 == ST_TTYPE,
              "trig types: a held step and VOL step through them (ALL AMP FILTER ... NONE), the accent knob keeps them");
        static uint8_t tb[BLOB_MAX];
        static m2k_pattern_t tw;
        p->step[0][9] = (m2k_step_t){.note = 50, .flags = ST_TRIG | ST_NOFLT};
        int tn = pat_pack(p, tb);
        CHECK(pat_unpack(&tw, tb, tn) && tw.step[0][9].flags == (ST_TRIG | ST_NOFLT), "trig types: saved and loaded with the step");
    }
    /* trigs and locks survive a pack */
    int trigs = 0, locks = 0;
    for (int t = 0; t < 6; t++) for (int st = 0; st < 64; st++) trigs += pat[0].step[t][st].flags & 1;
    for (int i = 0; i < M2K_LOCKS; i++) locks += pat[0].lock[i].trk != 0xFF;
    m2k_pattern_t keep = pat[0];
    pattern_save(); pattern_reload();
    int same = !memcmp(&keep.kit, &pat[0].kit, sizeof keep.kit) && !memcmp(keep.machine, pat[0].machine, 6);
    for (int t = 0; t < 6; t++) for (int st = 0; st < 64; st++)
        if ((keep.step[t][st].flags & 1) && memcmp(&keep.step[t][st], &pat[0].step[t][st], 4)) same = 0;
    CHECK(same, "pack / unpack keeps sounds, trigs, notes, conditions");
    printf("     A01: %d trigs, %d locks\n", trigs, locks);
    /* a project: save into P2, change, load back */
    project_save(1);
    CHECK(cur_proj == 1, "save project into P2 opens it");
    pat[5].kit[2][P_VOL] = 1;
    project_load(1);
    CHECK(pat[5].kit[2][P_VOL] != 1, "load project discards unsaved work");
    project_load(0);
    CHECK(!strcmp(pat[1].name, "CUTUP"), "load DEMO again");
    /* the store, out and back */
    pat[3].kit[1][P_PAN] = 17;
    int n = m2k_store_pack();
    printf("     store: %d bytes\n", n);
    m2k_init(9);
    CHECK(pat[3].kit[1][P_PAN] != 17, "fresh init is the factory");
    CHECK(m2k_store_unpack(n) == 1, "store unpacks");
    CHECK(pat[3].kit[1][P_PAN] == 17, "the working state comes back");
    /* export / import */
    int e = m2k_proj_export(0);
    CHECK(m2k_proj_import(3, e) == 1 && !strcmp(m2k_project_name(3), "DEMO"), "export DEMO, import into P4");
    store_buf[0] = 'X';
    CHECK(m2k_proj_import(2, e) == 0, "a bad file is refused");
    /* a full project: fill every pattern with trigs and locks until the image refuses */
    for (int i = 0; i < M2K_PATTERNS; i++) {
        for (int t = 0; t < 6; t++) for (int st = 0; st < 64; st++) { pat[i].step[t][st].flags = 1; pat[i].step[t][st].note = 60; }
        for (int k = 0; k < M2K_LOCKS; k++) pat[i].lock[k] = (m2k_lock_t){(uint8_t)(k % 6), (uint8_t)(k % 64), 9, 9};
    }
    CHECK(project_save(2) == 0, "a project too big for an image is refused, not corrupted");
    CHECK(!strcmp(m2k_project_name(2), "P3"), "the refused slot is untouched");
    /* a store saved before today's demos: the slots still holding the ones its revision shipped take today's, the
     * owner's stay. Revision 0 (before the store kept one: A16 was INIT, the owner's once written in), 1 to 8 (7: the
     * bank before its reorder, every part in another slot; 8: before CUTUP and RIVETS swapped) */
    for (int rev = 0; rev < 9; rev++) {
        static const char *const OLDN[9] = {"SPU RM", "DAYDRM", "GLITCH", "DREXC", "FACETS", "TORQUE", "PYLONS", "HAZE", "RIVETS"};
        int sl = rev == 2 ? 4 : rev == 3 || rev == 6 ? 8 : rev == 4 ? 3 : rev == 5 ? 5 : rev == 7 ? 0 : rev == 8 ? 1 : 2;   /* a slot the next revision replaced */
        m2k_init(11);
        m2k_pattern_t keep2 = pat[sl], keep15 = pat[15], keep0 = pat[0], keep6 = pat[6];
        s_cpy(pat[sl].name, OLDN[rev]);                      /* an old demo, as shipped */
        if (rev == 3) { s_cpy(pat[0].name, "HAZE"); pat[0].kit[5][P_DIST] = 74; }   /* A01 as revision 3 shipped it: HAZE's lead at DIST 74 */
        if (rev == 7) pat[6].kit[5][P_DIST] = 74;            /* A07 VESPER, in its slot before and after the reorder */
        s_cpy(pat[7].name, "MYSONG");                        /* the owner's */
        if (rev) {
            s_cpy(pat[15].name, rev >= 8 ? "SPLICE" : rev >= 5 ? "FOUNDRY" : "SHARDS");   /* the A16 its revision shipped */
        } else {
            pattern_init_blank(&pat[15]);
            pat[15].step[0][0].flags = ST_TRIG;              /* A16 INIT, but written in: the owner's */
        }
        project_save(0);
        int n = m2k_store_pack();
        if (rev & 1) {                                       /* version 3, at the factory's revision it was saved at */
            store_buf[7] = (uint8_t)rev;
        } else if (rev) {                                    /* version 2: no counts (the build's own), no sounds */
            store_buf[7] = (uint8_t)rev;
            memmove(store_buf + 8, store_buf + 10, (size_t)n - 10);
            store_buf[4] = 2;
            n -= 2;
        } else {
            memmove(store_buf + 7, store_buf + 10, (size_t)n - 10);  /* version 1: before the factory's revision was kept */
            store_buf[4] = 1;
            n -= 3;
        }
        m2k_init(12);
        CHECK(m2k_store_unpack(n) == 1, rev ? "store: one saved at an earlier factory revision loads" : "store: one saved before today's demos loads");
        CHECK(!strcmp(pat[sl].name, keep2.name) && !memcmp(pat[sl].kit, keep2.kit, sizeof keep2.kit),
              "store: an old demo left in its slot becomes today's");
        int a16 = rev ? !strcmp(pat[15].name, keep15.name) && !memcmp(pat[15].kit, keep15.kit, sizeof keep15.kit)
                      : !strcmp(pat[15].name, "INIT") && (pat[15].step[0][0].flags & ST_TRIG);
        CHECK(!strcmp(pat[7].name, "MYSONG") && a16, rev ? "store: the owner's kept, an earlier revision's A16 today's"
                                                         : "store: the owner's patterns are kept");
        if (rev == 3)
            CHECK(!memcmp(pat[0].kit, keep0.kit, sizeof keep0.kit), "store: A01 as revision 3 shipped it (HAZE) takes today's");
        if (rev == 7)
            CHECK(!memcmp(pat[6].kit, keep6.kit, sizeof keep6.kit), "store: a demo kept by its name (VESPER) takes today's settings");
    }
    /* GLO > MIDI: the clock out (24 a beat at the tempo, start first, stop last), the clock in (a step every sixth
     * pulse, the tempo from them), the notes out, the channel */
    {
        static int16_t mb[2 * 128];
        uint8_t m[3];
        m2k_init(13);
        open_b01();
        m2k_pattern_t *p = &pat[16];
        p->glob[G_TMPO] = 120 - 30;
        p->glob[G_SWNG] = 0;
        p->step[0][0] = (m2k_step_t){.note = 60, .flags = ST_TRIG};
        p->step[0][2] = (m2k_step_t){.note = 62, .flags = ST_TRIG};
        m2k_setting(S_SYNO, 127);
        m2k_setting(S_NOTO, 127);
        seq_play(1);
        int pulses = 0, start = 0, ons = 0, offs = 0, first = -1;
        for (int s = 0; s < M2K_SR / 2; s += 128) {         /* half a second at 120 BPM: a beat */
            m2k_render(mb, 128);
            int k;
            while ((k = m2k_midi_out(m, m2k_samples())) > 0) {
                if (first < 0) first = m[0];
                pulses += m[0] == 0xF8; start += m[0] == 0xFA;
                ons += k == 3 && m[0] == 0x90 && m[2]; offs += k == 3 && m[0] == 0x80;
            }
        }
        seq_play(0);
        int stop = 0, k;
        while ((k = m2k_midi_out(m, m2k_samples())) > 0) stop += m[0] == 0xFC;
        CHECK(first == 0xFA && start == 1 && pulses >= 23 && pulses <= 25 && stop == 1,
              "MIDI: the clock out, 24 pulses a beat at the tempo, start first and stop at the end");
        CHECK(ons == 2 && offs >= 1, "MIDI: the notes out, each track's trigs on its channel, and their ends");
        /* the clock in at 100 BPM: 1102.5 samples a pulse, delivered between blocks */
        m2k_init(14);
        open_b01();
        m2k_setting(S_SYNI, 127);
        m2k_midi(0xFA, 0, 0);
        double next = 0;
        int sent = 0;
        for (int s = 0; sent < 6 * 9; s += 16) {
            while (s >= next && sent < 6 * 9) { m2k_midi(0xF8, 0, 0); sent++; next += 1102.5; }
            m2k_render(mb, 16);
        }
        for (int i = 0; i < 8; i++) m2k_render(mb, 16);
        CHECK(sq.playing && sq.step == 9 % seq_len() && seq_bpm() >= 99 && seq_bpm() <= 101,
              "MIDI: the clock in, a step every sixth pulse, its tempo the sequencer's");
        m2k_midi(0xFC, 0, 0);
        CHECK(!sq.playing, "MIDI: stop from the clock in");
        /* the channel: track 1 on channel 3, so 0x92 plays track 1 and 0x90 the track in view */
        m2k_setting(S_MCH, 2 * 8 + 4);
        ui.trk = 4; sq.ktrk = 4;
        m2k_midi(0x92, 64, 100);
        int t0 = trk[0].note == 64 && trk[0].gate;
        m2k_midi(0x82, 64, 0);
        CHECK(t0, "MIDI: CHAN puts track 1 on its channel");
        m2k_setting(S_MCH, 0); m2k_setting(S_SYNI, 0);
    }
    /* the store, version 3: the sound library with it; a store for the FM-1 (16 patterns, one project) read here (64,
     * four): bank A as it was, the rest blank, the sounds kept; a project file of version 1 still imported */
    {
        m2k_init(31);
        CHECK(snd[0].name[0] && snd[0].machine == pat[0].machine[0] && !memcmp(snd[0].kit, pat[0].kit[0], P_COUNT),
              "sounds: the factory's library holds bank A's tracks, A01 T1 first");
        sound_save(63, &pat[2], 4, 0);
        s_cpy(snd[63].name, "MINE");
        int n = m2k_store_pack();
        m2k_init(32);
        CHECK(m2k_store_unpack(n) == 1 && !strcmp(snd[63].name, "MINE") && snd[63].machine == pat[2].machine[4],
              "sounds: the library goes out and back with the store");
        pat[5].kit[1][P_PAN] = 21;
        pat[20].kit[1][P_PAN] = 22;
        n = m2k_store_pack_as(16, 1);
        m2k_init(33);
        int ok = m2k_store_unpack(n) == 1;
        m2k_pattern_t blank;
        pattern_init_blank(&blank);
        CHECK(ok && pat[5].kit[1][P_PAN] == 21 && !strcmp(pat[20].name, blank.name) && pat[20].kit[1][P_PAN] != 22 &&
              !strcmp(m2k_project_name(0), "DEMO") && !strcmp(m2k_project_name(2), "P3") && !strcmp(snd[63].name, "MINE"),
              "store: the FM-1's (16 patterns, one project) read on 64 and four: bank A kept, the rest blank, the sounds kept");
        int e = m2k_proj_export(0);
        memmove(store_buf + 5, store_buf + 6, (size_t)e - 6);   /* version 1: no count, 64 patterns */
        store_buf[4] = 1;
        CHECK(m2k_proj_import(3, e - 1) == 1 && !strcmp(m2k_project_name(3), "DEMO"), "project file: version 1 still imported");
        m2k_init(35);
        s_cpy(proj[2].name, "THIRD");
        m2k_project(2, 0);
        n = m2k_store_pack_as(16, 1);
        CHECK(n > 0 && m2k_store_unpack(n) == 1 && !strcmp(m2k_project_name(0), "THIRD") && m2k_project_current() == 0,
              "store for the FM-1: the open project goes, whichever it is");
    }
    /* SYN > SND: 128 slots, bank A's 96 sounds then 32 INIT; SAVE turned once keeps the track's sound in an INIT slot
     * (twice over a sound), LOAD once puts it on a track, an INIT slot loaded starts the machine afresh; CAT shows one
     * category (and the INIT slots), SLOT steps through those */
    {
        m2k_init(34);
        open_b01();
        CHECK(snd[95].name[0] && !snd[96].name[0] && !snd[127].name[0] && snd_cat[0] && !snd_cat[100],
              "sounds: 128 slots, bank A's 96 sounds (each with a category) and 32 INIT");
        ui.trk = 2;
        ui.page = PG_SYN; ui.sub = (uint8_t)(nsub(PG_SYN) - 1);
        ui.snd_sel = 100;
        ui.now = 5000;                                            /* the screen's clock, for the second turn */
        int mach = pat[16].machine[2] = M_SID;
        pat[16].kit[2][P_DEC] = 77;
        edit(2, 1);                                               /* SAVE, into INIT: at once */
        int saved = snd[100].machine == mach && snd[100].kit[P_DEC] == 77 && snd[100].name[0] && snd_cat[100];
        pat[16].kit[2][P_DEC] = 30;
        ui.now = 9000;
        edit(2, 1);                                               /* over a sound: asked */
        int kept = snd[100].kit[P_DEC] == 77 && ask.kind == ASK_SND_OVER;
        m2k_button(B_OCTU, 1); m2k_button(B_OCTU, 0);             /* YES */
        int replaced = snd[100].kit[P_DEC] == 30 && !ask.kind;
        ui.trk = 4;
        edit(1, 1);                                               /* LOAD, once */
        int loaded = pat[16].machine[4] == mach && pat[16].kit[4][P_DEC] == 30 && trk[4].eff[P_DEC] == 30;
        ui.snd_sel = 120;
        pat[16].kit[4][P_DEC] = 99;
        edit(1, 1);                                               /* an INIT slot: the machine afresh */
        m2k_pattern_t fresh;
        pat_init(&fresh, "X");
        kit_default(&fresh, 4, mach);
        int init = pat[16].machine[4] == mach && !memcmp(pat[16].kit[4], fresh.kit[4], P_COUNT);
        CHECK(saved && kept && replaced && loaded && init,
              "sounds: SAVE into INIT at once, over a sound when OCT+ says YES, LOAD once, INIT afresh");
        ui.snd_sel = 0;
        int c0 = snd_cat[0];
        edit(3, 1);                                               /* CAT: KICK */
        int shown_ok = 1, n = 0;
        snd_list(ui.snd_sel, &n);
        for (int k = 0; k < n; k++) { int i = snd_nth(k); if (snd[i].name[0] && snd_cat[i] != ui.snd_cat) shown_ok = 0; }
        for (int k = 0; k < 40; k++) edit(0, 1);                  /* SLOT through them */
        CHECK(ui.snd_cat == CAT_KICK && shown_ok && n >= 32 && snd_shown(ui.snd_sel) && (c0 || 1),
              "sounds: CAT shows one category and the INIT slots; SLOT steps through those alone");
    }
    /* the verbs: GLO + SEQ clears the pattern's steps (the sounds stay), again within 3 s undoes it, held a second
     * makes it INIT; GLO + OCT- / OCT+ undo and redo; in GRID they act on the track; GLO + EDIT opens the library */
    {
        m2k_init(36);
        m2k_pattern_t orig = pat[0], blank;
        pattern_init_blank(&blank);
        ui.page = PG_HOME;
        ui.now = 1000;
        m2k_button(B_GLO, 1); m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0);
        int cleared = 1;
        for (int t = 0; t < M2K_TRACKS; t++) for (int st = 0; st < M2K_STEPS; st++) cleared &= !(pat[0].step[t][st].flags & ST_TRIG);
        int sounds = !strcmp(pat[0].name, "MAMMOTH") && pat[0].machine[2] == M_ENS;
        ui.now = 2000;
        m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0);                  /* again: undone */
        int undone = !memcmp(&pat[0], &orig, sizeof orig);
        m2k_button(B_OCTU, 1); m2k_button(B_OCTU, 0);                /* REDO: cleared again */
        int redone = !(pat[0].step[0][0].flags & ST_TRIG) && !strcmp(pat[0].name, "MAMMOTH");
        m2k_button(B_OCTD, 1); m2k_button(B_OCTD, 0);                /* UNDO */
        int undo2 = !memcmp(&pat[0], &orig, sizeof orig);
        ui.now = 9000;
        m2k_button(B_SEQ, 1);                                        /* held: the ring, then INIT */
        ui.now = 9600; ring_tick();
        int half = !strcmp(pat[0].name, "MAMMOTH");
        ui.now = 10100; ring_tick();
        m2k_button(B_SEQ, 0);
        int init = !memcmp(&pat[0], &blank, sizeof blank);
        m2k_button(B_OCTD, 1); m2k_button(B_OCTD, 0);
        int back = !memcmp(&pat[0], &orig, sizeof orig);
        ui.grid = 1; ui.trk = 3;                                     /* in GRID: the track */
        ui.now = 20000;
        m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0);
        int trk_only = !(pat[0].step[3][0].flags & ST_TRIG) && memcmp(pat[0].step[0], orig.step[0], sizeof orig.step[0]) == 0;
        ui.grid = 0;
        m2k_button(B_EDIT, 1); m2k_button(B_EDIT, 0); m2k_button(B_GLO, 0);
        int lib = snd_page();
        CHECK(cleared && sounds && undone && redone && undo2 && half && init && back && trk_only && lib,
              "GLO + SEQ: clear (sounds stay), again: undo, held: INIT; GLO + OCT-/+ undo, redo; in GRID the track; GLO + EDIT: SND");
        m2k_init(37);
        m2k_button(B_GLO, 1); m2k_button(B_ARP, 1); m2k_button(B_ARP, 0); m2k_button(B_GLO, 0);
        int pads = ui.pads && !ui.grid;
        uint32_t t0 = mon.trig[0], t2 = mon.trig[2];
        static int16_t b16[256];
        m2k_key(0, 1); m2k_key(0, 0);                 /* F3: white 1, T1 */
        m2k_key(4, 1); m2k_key(4, 0);                 /* A3: white 3, T3 */
        m2k_render(b16, 128);
        int hit = mon.trig[0] != t0 && mon.trig[2] != t2;
        for (int t = 0; t < M2K_TRACKS; t++) for (int st = 0; st < M2K_STEPS; st++) pat[0].step[t][st].flags = 0;
        seq_play(1);
        sq.rec = 1;
        for (int f = 0; f < 44100 / 2; f += 128) m2k_render(b16, 128);
        m2k_key(14, 1); m2k_key(14, 0);               /* G4: white 9, T1 again */
        int written = 0;
        for (int st = 0; st < M2K_STEPS; st++) written += pat[0].step[0][st].flags & ST_TRIG ? 1 : 0;
        seq_play(0);
        CHECK(pads && hit && written == 1, "PADS (GLO + ARP): white keys hit T1-T6, each on its own; with REC, a trig written");
        /* GLO > SET: WIPE asks, OCT+ makes every pattern INIT (the sounds kept), OCT- or any other button keeps them;
         * DEMO asks, OCT+ brings bank A's back */
        m2k_init(38);
        ui.page = PG_GLO; ui.sub = 3;
        ui.now = 3000;
        int wk = -1, dk = -1;
        for (int k = 0; k < 4; k++) { int c = knob_param(PG_GLO, 3, k); if (c == SETTING(22)) wk = k; if (c == SETTING(23)) dk = k; }
        edit(wk, 1);
        m2k_button(B_OCTD, 1); m2k_button(B_OCTD, 0);             /* NO */
        int first = !strcmp(pat[0].name, "MAMMOTH") && !ask.kind;
        ui.now = 3500;
        edit(wk, 1);
        m2k_button(B_OCTU, 1); m2k_button(B_OCTU, 0);             /* YES */
        int wiped = 1;
        for (int i = 0; i < M2K_PATTERNS; i++) wiped &= !strcmp(pat[i].name, "INIT");
        int kept = snd[0].name[0] != 0;
        int demos = 1;
        if (dk >= 0) {
            ui.now = 6000; edit(dk, 1); m2k_button(B_OCTU, 1); m2k_button(B_OCTU, 0);
            demos = !strcmp(pat[0].name, "MAMMOTH") && !strcmp(pat[15].name, "SPLICE") && !strcmp(pat[16].name, "INIT");
        }
        CHECK(wk >= 0 && first && wiped && kept && demos, "GLO > SET: WIPE asks (OCT- NO, OCT+ YES: every pattern INIT, the sounds kept); DEMO, bank A back");
        /* HOME held: the white keys pick patterns, SEQ clears them (held: INIT), REC copies, PLAY pastes, the black
         * keys 1-4 the bank; EDIT held: SEQ INITs the sound, SAVE keeps it in the first INIT slot */
        {
        m2k_init(39);
        ui.now = 1000;
        m2k_button(B_HOME, 1);
        m2k_key(2, 1); m2k_key(2, 0);                 /* white 2: A02 */
        m2k_key(7, 1); m2k_key(7, 0);                 /* C4, white 5: A05 */
        int picked = cur_pat == 4;
        m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0);
        int clr = 1;
        for (int t = 0; t < M2K_TRACKS; t++) for (int st = 0; st < M2K_STEPS; st++) clr &= !(pat[1].step[t][st].flags & ST_TRIG) && !(pat[4].step[t][st].flags & ST_TRIG);
        int others = 0;
        for (int st = 0; st < M2K_STEPS; st++) others += pat[0].step[0][st].flags & ST_TRIG ? 1 : 0;
        int sounds = !strcmp(pat[1].name, "CUTUP");
        ui.now = 1500;
        m2k_button(B_SEQ, 1);                         /* held a second: INIT */
        ui.now = 2600; ring_tick();
        m2k_button(B_SEQ, 0);
        int init = !strcmp(pat[1].name, "INIT") && !strcmp(pat[4].name, "INIT");
        m2k_button(B_HOME, 0);
        m2k_pattern(0);
        m2k_button(B_HOME, 1);
        m2k_button(B_REC, 1); m2k_button(B_REC, 0);   /* copy A01 */
        m2k_key(3, 1); m2k_key(3, 0);                 /* G#3, black 2: bank B */
        m2k_key(4, 1); m2k_key(4, 0);                 /* A3, white 3: B03 */
        m2k_key(6, 1); m2k_key(6, 0);                 /* B3, white 4: B04 */
        m2k_button(B_PLAY, 1); m2k_button(B_PLAY, 0);
        m2k_button(B_HOME, 0);
        int pasted = 0;
        for (int i = 16; i < 32; i++) pasted += !strcmp(pat[i].name, "MAMMOTH");
        CHECK(picked && clr && others && sounds && init && pasted == 2 && sq.playing == 0,
              "HOME held: keys pick patterns, SEQ clears them (sounds kept), held INIT; REC copies, black 2 bank B, PLAY pastes");
        m2k_init(40);
        ui.trk = 2;
        pat[0].kit[2][P_DEC] = 3;
        m2k_button(B_EDIT, 1);
        m2k_button(B_SAVE, 1); m2k_button(B_SAVE, 0);
        int kept_snd = snd[96].name[0] && snd[96].kit[P_DEC] == 3;
        m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0);
        m2k_button(B_EDIT, 0);
        m2k_pattern_t fr;
        pat_init(&fr, "X");
        kit_default(&fr, 2, pat[0].machine[2]);
        CHECK(kept_snd && pat[0].kit[2][P_DEC] == fr.kit[2][P_DEC] && !ui.grid,
              "EDIT held: SAVE keeps the sound in the first INIT slot, SEQ INITs it (no GRID)");
        }
        /* SEQ: a tap is GRID; held, the white keys are tools on the track's steps (their locks with them), the knobs
         * LEN SWING ACCENT TEMPO, and letting go is not a tap */
        {
        m2k_init(41);
        ui.now = 1000;
        m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0);
        int tap = ui.grid == 1;
        m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0);
        tap &= ui.grid == 0;
        m2k_pattern_t *q = &pat[0];
        int len = seq_len(), t = ui.trk;
        for (int st = 0; st < M2K_STEPS; st++) q->step[t][st].flags = 0;
        for (int k = 0; k < M2K_LOCKS; k++) if (q->lock[k].trk == t) q->lock[k].trk = 0xFF;
        q->step[t][0].flags = ST_TRIG; q->step[t][0].note = 50;
        q->step[t][5].flags = ST_TRIG; q->step[t][5].note = 55;
        lock_set(t, 5, P_DEC, 99);
        m2k_button(B_SEQ, 1);
        m2k_key(2, 1); m2k_key(2, 0);                 /* G3, white 2: REVERSE */
        int rev = (q->step[t][len - 1].flags & ST_TRIG) && q->step[t][len - 6].note == 55 && lock_find(t, len - 6, P_DEC) >= 0;
        m2k_key(4, 1); m2k_key(4, 0);                 /* A3, white 3: SHIFT < */
        int shl = (q->step[t][len - 2].flags & ST_TRIG) && lock_find(t, len - 7, P_DEC) >= 0;
        m2k_button(B_REC, 1); m2k_button(B_REC, 0);   /* REC: copy the track's steps */
        int len0 = PAT(cur_pat)->glob[G_LEN];
        edit(0, -1);                                  /* KNOB 1 in the layer: LEN */
        int lenk = PAT(cur_pat)->glob[G_LEN] == len0 - 1;
        edit(0, 1);
        m2k_button(B_SEQ, 0);
        int nogrid = ui.grid == 0;
        ui.trk = (uint8_t)((t + 1) % M2K_TRACKS);
        m2k_button(B_SEQ, 1); m2k_button(B_PLAY, 1); m2k_button(B_PLAY, 0); m2k_button(B_SEQ, 0);   /* PLAY: paste */
        int pasted = !memcmp(q->step[ui.trk], q->step[t], sizeof q->step[t]) && lock_find(ui.trk, len - 7, P_DEC) >= 0;
        m2k_button(B_SEQ, 1); m2k_key(12, 1); m2k_key(12, 0); m2k_button(B_SEQ, 0);   /* F4, white 8: HALF */
        int half = seq_len() == len / 2;
        CHECK(tap && rev && shl && lenk && nogrid && pasted && half,
              "SEQ: a tap is GRID; held, REVERSE and SHIFT move the steps and their locks, REC / PLAY copy and paste, HALF, KNOB 1 LEN");
        /* a step held in GRID: REC copies it, PLAY pastes it onto another held, SEQ clears the held step's locks */
        m2k_init(42);
        ui.grid = 1; ui.trk = 0; ui.spage = 0;
        m2k_pattern_t *r = &pat[0];
        for (int st = 0; st < M2K_STEPS; st++) r->step[0][st].flags = 0;
        for (int k = 0; k < M2K_LOCKS; k++) if (r->lock[k].trk == 0) r->lock[k].trk = 0xFF;
        r->step[0][0].flags = ST_TRIG | ST_ACC; r->step[0][0].note = 67; lock_set(0, 0, P_DEC, 12);
        m2k_key(0, 1);                                /* F3: step 1 held */
        m2k_button(B_REC, 1); m2k_button(B_REC, 0);
        m2k_key(0, 0);
        m2k_key(7, 1);                                /* C4: step 5 held */
        m2k_button(B_PLAY, 1); m2k_button(B_PLAY, 0);
        int spasted = r->step[0][4].note == 67 && (r->step[0][4].flags & ST_ACC) && lock_find(0, 4, P_DEC) >= 0;
        m2k_button(B_SEQ, 1); m2k_button(B_SEQ, 0);   /* SEQ: its locks cleared, the step kept */
        int lclr = lock_find(0, 4, P_DEC) < 0 && (r->step[0][4].flags & ST_TRIG);
        m2k_key(7, 0);
        int stays = (r->step[0][4].flags & ST_TRIG) && ui.grid == 1;
        CHECK(spasted && lclr && stays, "a held step: REC copies it, PLAY pastes it onto another, SEQ clears its locks");
        }
    }
    /* performance: FX tapped is its page, held its black keys PERFORM (a repeat while held, KILL silences); LFO held,
     * the knobs snap back (OCT+ keeps them); GLO + white 9-14 solo while held, 7 a fill for the next loop, 8 taps the
     * tempo, 15 unmutes all; GLO + SELECT the tempo */
    {
        m2k_init(43);
        static int16_t pb[256];
        ui.now = 1000;
        m2k_button(B_FX, 1); m2k_button(B_FX, 0);
        int fxpage = ui.page == PG_FX;
        ui.page = PG_HOME;
        seq_play(1);
        for (int f = 0; f < 44100; f += 128) m2k_render(pb, 128);
        m2k_button(B_FX, 1);
        m2k_key(1, 1);                                /* F#3, black 1: a quarter's repeat */
        for (int f = 0; f < 4410; f += 128) m2k_render(pb, 128);
        int rep_on = bus.g_on && bus.g_left > 100000;
        m2k_key(1, 0);
        for (int f = 0; f < 512; f += 128) m2k_render(pb, 128);
        int rep_off = !bus.g_on;
        m2k_key(25, 1);                               /* F#5, black 11: KILL */
        int32_t pk = 0;
        for (int f = 0; f < 4410; f += 128) { m2k_render(pb, 128); if (f > 1000) for (int i = 0; i < 256; i++) pk = pb[i] > pk ? pb[i] : -pb[i] > pk ? -pb[i] : pk; }
        m2k_key(25, 0);
        m2k_button(B_FX, 0);
        int killed = pk == 0 && ui.page == PG_HOME;
        seq_play(0);
        ui.page = PG_AMP; ui.sub = 0; ui.trk = 1;
        int dec0 = pat[0].kit[1][P_DEC];
        m2k_button(B_LFO, 1);
        edit(2, 10);                                  /* KNOB 3: DEC */
        int moved = pat[0].kit[1][P_DEC] != dec0;
        m2k_button(B_LFO, 0);
        int back = pat[0].kit[1][P_DEC] == dec0 && ui.page == PG_AMP;
        m2k_button(B_LFO, 1); edit(2, 10); m2k_button(B_OCTU, 1); m2k_button(B_OCTU, 0); m2k_button(B_LFO, 0);
        int kept = pat[0].kit[1][P_DEC] != dec0;
        CHECK(fxpage && rep_on && rep_off && killed && moved && back && kept,
              "PERFORM: FX tapped its page, held a repeat while a black key is held, KILL silent; LFO held: the knobs snap back, OCT+ keeps");
        m2k_init(44);
        m2k_button(B_GLO, 1);
        ui.now = 5000; m2k_key(19, 1); m2k_key(19, 0);   /* G4, white 9... (white 8 is F4) */
        m2k_button(B_GLO, 0);
        int fkey = -1, tkey = -1, skey = -1, ukey = -1;
        for (int k = 0; k < M2K_KEYS; k++) { if (ui.kw[k] == 6) fkey = k; if (ui.kw[k] == 7) tkey = k; if (ui.kw[k] == 8) skey = k; if (ui.kw[k] == 14) ukey = k; }
        m2k_button(B_GLO, 1);
        ui.now = 10000; m2k_key(tkey, 1); m2k_key(tkey, 0);
        ui.now = 10500; m2k_key(tkey, 1); m2k_key(tkey, 0);
        ui.now = 11000; m2k_key(tkey, 1); m2k_key(tkey, 0);
        int tapped = (int)seq_bpm() == 120;
        int tmp0 = pat[0].glob[G_TMPO];
        m2k_encoder(E_SELECT, 2);
        int sel = pat[0].glob[G_TMPO] == tmp0 + 2;
        m2k_key(fkey, 1); m2k_key(fkey, 0);
        int fillq = sq.fill_next == 1;
        m2k_key(skey, 1);                             /* solo T1 while held */
        int solo = sq.solo == 1;
        m2k_key(skey, 0);
        int unsolo = sq.solo == 0;
        pat[0].mute = 0x2A;
        m2k_key(ukey, 1); m2k_key(ukey, 0);
        int allon = pat[0].mute == 0;
        m2k_button(B_GLO, 0);
        seq_play(1);
        static int16_t pb2[256];
        int saw_fill = 0;
        for (int f = 0; f < 44100 * 30 && !saw_fill; f += 128) { m2k_render(pb2, 128); saw_fill |= sq.fill; }
        int fill_ends = 0;
        for (int f = 0; f < 44100 * 30 && !fill_ends; f += 128) { m2k_render(pb2, 128); fill_ends |= !sq.fill; }
        seq_play(0);
        CHECK(tapped && sel && fillq && solo && unsolo && allon && saw_fill && fill_ends,
              "GLO: white 8 taps the tempo (120), SELECT turns it, 7 fills the next loop and only it, 9-14 solo while held, 15 unmutes all");
    }
    /* the render on two cores (the FM-1's): the second core's tracks run before or after the first's (as they may,
     * at once) and every pattern of bank A sounds sample for sample as on one */
    {
        static int16_t a[2 * 44100], b[2 * 44100];
        int same = 1, split_seen = 0;
        for (int p = 0; p < 16; p++) {
            for (int mode = 0; mode < 3; mode++) {
                par_mode = mode;
                m2k_par_begin = mode ? par_test_begin : 0;
                m2k_par_end = mode ? par_test_end : 0;
                m2k_init(21);
                m2k_pattern(p);
                seq_play(1);
                int16_t *o = mode ? b : a;
                for (int f = 0; f < 44100; f += 128) m2k_render(o + 2 * f, 128 < 44100 - f ? 128 : (44100 - f) & ~15);
                if (mode && memcmp(a, b, sizeof a)) { same = 0; printf("     A%02d differs with the second core's tracks %s\n", p + 1, mode == 1 ? "first" : "last"); }
                split_seen |= par_from < M2K_TRACKS;
            }
        }
        m2k_par_begin = 0; m2k_par_end = 0;
        CHECK(same && split_seen, "two cores: bank A the same sample for sample, the second core's tracks first or last");
    }
    printf("%s (%d failed)\n", fails ? "SELFTEST FAILED" : "selftest passed", fails);
    return fails != 0;
}

/* the browser and the FM-1's stores, through the Patch Editor's link (host/m2k_link_test.c): --web-store PATH writes
 * a store as the browser sends it to the FM-1 (64 patterns, one project; A01 named WEB16, sound 6 WEBSND);
 * --fm1-store PATH reads one the FM-1 sent (the link test's: A01 LINKA, sound 6 SNDA) as the browser does */
static int link_store(const char *mode, const char *path)
{
    int max;
    uint8_t *b = m2k_store_buf(&max);
    m2k_init(1);
    if (!strcmp(mode, "--web-store")) {
        memset(pat[0].name, 0, 8); memcpy(pat[0].name, "WEB16", 5);
        memset(snd[5].name, 0, 8); memcpy(snd[5].name, "WEBSND", 6);
        int n = m2k_store_pack_as(64, 1);
        FILE *f = fopen(path, "wb");
        if (!f || n <= 0 || b[8] != 64 || b[9] != 1) { printf("FAIL a store for the FM-1\n"); if (f) fclose(f); return 1; }
        fwrite(b, 1, (size_t)n, f);
        fclose(f);
        printf("ok   a store for the FM-1: %d B, 64 patterns, 1 project\n", n);
        return 0;
    }
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return 1; }
    int n = (int)fread(b, 1, (size_t)max, f);
    fclose(f);
    int ok = b[8] == 64 && b[9] == 1 && m2k_store_unpack(n) && !strcmp(pat[0].name, "LINKA") && !strcmp(snd[5].name, "SNDA") &&
             !strcmp(pat[16].name, "INIT");
    printf("%s the FM-1's store read in the browser: its 64 patterns, its sounds\n", ok ? "ok  " : "FAIL");
    return !ok;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--selftest")) return selftest();
    if (argc > 1 && !strcmp(argv[1], "--text-sweep")) return text_sweep();
    if (argc > 1 && !strcmp(argv[1], "--bank-hash")) {        /* as host/m2k_link_test.c prints it, through the FM-1's cache */
        m2k_init(3);
        uint32_t h = 2166136261u;
        for (int i = 0; i < M2K_PATTERNS; i++) { int n = pat_pack(PAT(i), blob); for (int k = 0; k < n; k++) h = (h ^ blob[k]) * 16777619u; }
        const uint8_t *sb = (const uint8_t *)snd;
        for (unsigned k = 0; k < sizeof snd; k++) h = (h ^ sb[k]) * 16777619u;
        printf("     bank hash %08x\n", h);
        return 0;
    }
    if (argc > 2 && (!strcmp(argv[1], "--web-store") || !strcmp(argv[1], "--fm1-store"))) return link_store(argv[1], argv[2]);
    const char *out = argc > 1 ? argv[1] : "build/host";
    char path[512];
    if (argc > 2 && !strcmp(argv[2], "--analyze")) return analyze(out);
    if (argc > 2 && !strcmp(argv[2], "--pitch")) {           /* [FIXTURE]: a bank kept with --export-bank */
        if (argc > 3) {
            FILE *f = fopen(argv[3], "rb");
            if (!f) { perror(argv[3]); return 1; }
            bank_len = (int)fread(bank_fix, 1, sizeof bank_fix, f);
            fclose(f);
        }
        return pitch_dump(out);
    }
    if (argc > 2 && !strcmp(argv[2], "--export-bank")) return bank_export(out);
    if (argc > 2 && !strcmp(argv[2], "--bus")) return bus_measure(out);
    if (argc > 2 && !strcmp(argv[2], "--demo")) return demo_render(out, argc, argv);     /* [SECONDS] [SLOT ...] */
    if (argc > 2 && !strcmp(argv[2], "--demo-dry")) { demo_dry = 1; return demo_render(out, argc, argv); }
    /* 1: the three factory patterns, 12 s each */
    for (int p = 0; p < 3; p++) {
        m2k_init(2000);
        cur_pat = p;
        sq.queued = (uint8_t)p;
        lockmap_build();
        for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[p].kit[t], P_COUNT);
        seq_play(1);
        int frames = M2K_SR * 12 / 128 * 128;                    /* whole blocks */
        int16_t *buf = malloc((size_t)frames * 4);
        clock_t c0 = clock();
        for (int f = 0; f < frames; f += 128) m2k_render(buf + 2 * f, 128);
        double sec = (double)(clock() - c0) / CLOCKS_PER_SEC;
        int peak = 0;
        double rms = 0;
        for (int i = 0; i < frames * 2; i++) { int a = abs(buf[i]); if (a > peak) peak = a; rms += (double)buf[i] * buf[i]; }
        rms = sqrt(rms / (frames * 2));
        snprintf(path, sizeof path, "%s/a%02d.wav", out, p + 1);
        wav_write(path, buf, frames);
        printf("A%02d %-8s peak %5d (%.1f dBFS) rms %.1f dBFS  render %.3f s for 12 s (%.1f%% of real time here)\n", p + 1,
               pat[p].name, peak, 20 * log10(peak / 32767.0 + 1e-9), 20 * log10(rms / 32767.0 + 1e-9), sec, sec / 12 * 100);
        free(buf);
    }
    /* 2: the tour, with screenshots; then the manual's scenes */
    m2k_init(2000);
    run_events(TOUR, out);
    manual_scene();
    run_events(MANUAL, out);
    printf("tour done%s\n", ta_found ? ": TEXT TOUCHES OR IS CUT (above)" : ", every text clear");
    return ta_found != 0;
}
