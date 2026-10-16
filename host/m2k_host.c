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

/* render seconds of pattern p with the tracks in mask; peak and rms (dB) */
static void measure(int p, int mask, double sec, int16_t *keep, double *pk_db, double *rms_db)
{
    m2k_init(2000);
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
    for (int p = 0; p < M2K_PATTERNS - 1; p++) {
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

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "build/host";
    char path[512];
    if (argc > 2 && !strcmp(argv[2], "--analyze")) return analyze(out);
    /* 1: the three factory patterns, 12 s each */
    for (int p = 0; p < 3; p++) {
        m2k_init(2000);
        cur_pat = p;
        sq.queued = (uint8_t)p;
        lockmap_build();
        for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[p].kit[t], P_COUNT);
        seq_play(1);
        int frames = M2K_SR * 12;
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
