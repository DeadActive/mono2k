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
    {18500, 'x', 0, 0, 0},
};

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
        CHECK(bl[1] == 12, "format: patterns are written as version 12");
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
                               pat[d].machine[t] == M_SID || pat[d].machine[t] == M_GSIN;
                    moved += c.kit[t][P_PTCH] != pat[d].kit[t][P_PTCH] || (!conv && memcmp(c.kit[t], pat[d].kit[t], 8) != 0);
                    for (int s = 0; s < M2K_STEPS; s++)
                        moved += c.step[t][s].note != pat[d].step[t][s].note || c.step[t][s].flags != pat[d].step[t][s].flags;
                }
                for (int i = 0; i < M2K_LOCKS; i++) {
                    int m = pat[d].lock[i].trk != 0xFF ? pat[d].machine[pat[d].lock[i].trk] : M_ENS;
                    if (m != M_ENS && m != M_FMS && m != M_FMD && m != M_SID && m != M_GSIN && (pat[d].lock[i].param < 8 || pat[d].lock[i].param == P_PTCH))
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
            double r1 = T_FMR24[(s2[0] * 24) >> 7] / 65536.0 * (192 + s2[1]) / 256;
            ok = ok && fabs(r1 / 1.41 - 1) < 0.01 && s2[1] != 64 && (s2[4] * 24) >> 7 == 19 &&
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
                if (l->trk == 1 && l->step == 3) lr = l->param == P_SYN + 0 && (l->val * 24) >> 7 == 21;
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
            for (int t = 0; t < M2K_TRACKS; t++)             /* (a version-10 SID or GND-SIN page is the old one: none) */
                if (w2.machine[t] == M_GSIN || w2.machine[t] == M_SID) w2.machine[t] = M_GPRC;
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
            double ring[2];
            for (int mx = 0; mx < 2; mx++) {
                FXPAT(M_FXR, M_SAW, 0);
                pat[fp].kit[1][P_SYN + 3] = (uint8_t)(mx ? 127 : 0);
                fx_measure(fp, 0x3F, 1.0, kb, &pk, &rm);
                ring[mx] = rm;
            }
            double dry;
            FXPAT(M_GSIN, M_SAW, 0);
            pat[fp].kit[0][P_VOL] = 64;
            pat[fp].mute = 0x02;
            fx_measure(fp, 0x01, 1.0, kb, &pk, &dry);
            CHECK(fabs(ring[0] - dry) < 1.5 && fabs(ring[1] - ring[0]) > 1.0, "FX-RING: its input as it is at MIX 0, rung at 127");
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
            int three = nsub(PG_SYN) == 3 && knob_param(PG_SYN, 2, 0) == P_WBNK;
            ui.trk = 0;                                               /* T1: GND-SIN */
            int two = nsub(PG_SYN) == 2;
            CHECK(wt >= 0 && three && two && !strcmp(v1, "M2K") && !strcmp(v2, "M64") && !strcmp(n2, "OS06") && strcmp(n1, n2),
                  "ui: a wave machine's SYN has a BANK page (M2K / M64), its WAVE named from the bank");
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
    /* the clipboard, a sound: track 6's copied with GLO + REC on its page, pasted onto track 1 with GLO + PLAY */
    {
        m2k_init(7);
        uint32_t now = 100000;
        #define TAP_B(bb) do { m2k_button(bb, 1); m2k_frame(now += 20); m2k_button(bb, 0); m2k_frame(now += 20); } while (0)
        #define GLO_B(bb) do { m2k_button(B_GLO, 1); m2k_frame(now += 20); TAP_B(bb); m2k_button(B_GLO, 0); m2k_frame(now += 20); } while (0)
        cur_pat = 0;
        m2k_track(5);
        TAP_B(B_EDIT);                                           /* a track's page: the sound, not the pattern */
        GLO_B(B_REC);
        CHECK(!strcmp(ui.toast, "COPY") && !strcmp(ui.toast2, "T6 SOUND"), "clipboard: track 6's sound copied (T6 SOUND)");
        m2k_track(0);
        TAP_B(B_EDIT);
        GLO_B(B_PLAY);
        CHECK(!strcmp(ui.toast, "PASTE") && !strcmp(ui.toast2, "T1 SOUND") && pat[0].machine[0] == pat[0].machine[5] &&
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
        m2k_button(B_GLO, 1); m2k_button(B_OCTU, 1); m2k_button(B_OCTU, 0); m2k_button(B_GLO, 0);
        m2k_key(kw1, 1); m2k_key(kw1, 0);
        m2k_step_t *sp = pat[16].step[ui.trk];
        CHECK(sp[c0].note == 53 + kw0 && sp[c0 + 1].note == 53 + kw2 && !(sp[c0 + 2].flags & ST_TRIG) &&
              sp[c0 + 3].note == 53 + kw1 && (sp[c0 + 3].flags & ST_TRIG), "keys: step entry writes notes, GLO + OCT+ a rest");
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
    CHECK(!strcmp(pat[1].name, "RIVETS"), "load DEMO again");
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
     * owner's stay. Revision 0 (before the store kept one: A16 was INIT, the owner's once written in), 1 to 7 (7: the
     * bank before its reorder, every part in another slot) */
    for (int rev = 0; rev < 8; rev++) {
        static const char *const OLDN[8] = {"SPU RM", "DAYDRM", "GLITCH", "DREXC", "FACETS", "TORQUE", "PYLONS", "HAZE"};
        int sl = rev == 2 ? 4 : rev == 3 || rev == 6 ? 8 : rev == 4 ? 3 : rev == 5 ? 5 : rev == 7 ? 0 : 2;   /* a slot the next revision replaced */
        m2k_init(11);
        m2k_pattern_t keep2 = pat[sl], keep15 = pat[15], keep0 = pat[0], keep6 = pat[6];
        s_cpy(pat[sl].name, OLDN[rev]);                      /* an old demo, as shipped */
        if (rev == 3) { s_cpy(pat[0].name, "HAZE"); pat[0].kit[5][P_DIST] = 74; }   /* A01 as revision 3 shipped it: HAZE's lead at DIST 74 */
        if (rev == 7) pat[6].kit[5][P_DIST] = 74;            /* A07 VESPER, in its slot before and after the reorder */
        s_cpy(pat[7].name, "MYSONG");                        /* the owner's */
        if (rev) {
            s_cpy(pat[15].name, rev >= 5 ? "FOUNDRY" : "SHARDS");   /* the A16 its revision shipped: a demo */
        } else {
            pattern_init_blank(&pat[15]);
            pat[15].step[0][0].flags = ST_TRIG;              /* A16 INIT, but written in: the owner's */
        }
        project_save(0);
        int n = m2k_store_pack();
        if (rev) {
            store_buf[7] = (uint8_t)rev;                     /* the factory's revision it was saved at */
        } else {
            memmove(store_buf + 7, store_buf + 8, (size_t)n - 8);  /* as a store before the factory's revision was kept */
            store_buf[4] = 1;
            n--;
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
    printf("%s (%d failed)\n", fails ? "SELFTEST FAILED" : "selftest passed", fails);
    return fails != 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--selftest")) return selftest();
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
    /* 2: the tour, with screenshots */
    m2k_init(2000);
    int16_t blkbuf[256];
    int ms = 0, ei = 0;
    for (int s = 0; ; s += 128) {
        int now = (int)((int64_t)s * 1000 / M2K_SR);
        while (TOUR[ei].ms <= now) {
            const ev_t *e = &TOUR[ei++];
            if (e->kind == 'x') { printf("tour done\n"); return 0; }
            if (e->kind == 'b') m2k_button(e->a, e->b);
            if (e->kind == 'e') m2k_encoder(e->a, e->b);
            if (e->kind == 'k') m2k_key(e->a, e->b);
            if (e->kind == 's') {
                snprintf(path, sizeof path, "%s/%s.pbm", out, e->shot);
                pbm_write(path, m2k_frame((uint32_t)now));
            }
        }
        m2k_render(blkbuf, 128);
        if (now - ms >= 16) { ms = now; m2k_frame((uint32_t)now); }
    }
}
