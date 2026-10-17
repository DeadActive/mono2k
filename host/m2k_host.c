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

/* the bus's tape at TAPE 127 on a held 1047 Hz sine: the pitch per cycle (between interpolated rising zero
 * crossings), its slow part (a 159 ms average: one flutter period) and the rest; peak deviation in cents of each */
static void tape_pitch(double *wow_c, double *flut_c)
{
    m2k_init(7);
    cur_pat = 16;                                  /* B01, the blank kit */
    pat[16].machine[0] = M_GSIN;
    pat[16].kit[0][P_SYN + 4] = 0;                 /* no pitch envelope */
    pat[16].kit[0][P_HOLD] = 127;
    pat[16].kit[0][P_DEC] = 127;
    pat[16].glob[G_TAPE] = 127;
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

static int selftest(void)
{
    int fails = 0;
    double wow_c, flut_c;
    tape_pitch(&wow_c, &flut_c);
    printf("     tape at 127: wow +-%.1f c, flutter +-%.1f c\n", wow_c, flut_c);
    CHECK(wow_c > 25 && wow_c < 55 && flut_c < wow_c / 3,
          "tape: the wow drifts (25..55 c at TAPE 127), the flutter stays under a third of it");
    m2k_init(7);
    CHECK(!strcmp(pat[0].name, "DEEP") && !strcmp(pat[16].name, "INIT"), "factory: A01 DEEP, B01 INIT");
    CHECK(!strcmp(m2k_project_name(0), "DEMO") && !strcmp(m2k_project_name(1), "P2"), "projects: DEMO, P2");
    printf("     DEMO packed: %u bytes for 64 patterns (A01 %d bytes, B01 %d)\n", proj[0].used, proj[0].len[0], proj[0].len[16]);
    /* the pattern formats: older ones convert once, on their path; today's loads unchanged */
    {
        /* the demos are written on today's chain: none is converted (A01 T4's DTIM 48 would become 127) */
        CHECK(pat[0].kit[3][P_DTIM] == 48 && pat[0].kit[0][P_DSND] == 64 && pat[0].kit[0][P_DIST] == 64,
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
        CHECK(bl[1] == 7, "format: patterns are written as version 7");
        bl[1] = 4;
        CHECK(pat_unpack(&b, bl, n), "format: a version-4 pattern loads");
        CHECK(b.kit[0][P_DIST] == T_V5_DIST[34] && b.kit[0][P_DTIM] == 32 && b.kit[0][P_DSND] == 64 &&
              b.kit[0][P_DFB] == (64 * 270 * 64 + 16384) >> 15 && b.kit[0][P_VOL] == T_V5_VOL[100],
              "format: version 4 -> 5 converts the chain (DIST, DTIM, DSND, DFB, VOL)");
        CHECK(b.kit[0][P_BASE] == v5_base(T_V5_FPOS[40], 36), "format: the filter is placed against the track's note");
        CHECK(b.kit[0][P_SYN + 2] == 77 && b.kit[0][P_LFO + L_SPD] == 21 && b.kit[0][P_BITS] == 9 &&
              !memcmp(b.kit[1], a.kit[1], 8) && b.step[0][4].note == 36 && b.step[0][4].flags == ST_TRIG,
              "format: machine, LFO and MONO2K's own settings, trigs untouched");
        int lock_ok = 0;
        for (int i = 0; i < M2K_LOCKS; i++) {
            if (b.lock[i].trk == 0 && b.lock[i].param == P_DTIM && b.lock[i].val == 64) lock_ok |= 1;
            if (b.lock[i].trk == 0 && b.lock[i].param == P_SYN + 1 && b.lock[i].val == 99) lock_ok |= 2;
        }
        CHECK(lock_ok == 3, "format: a chain lock converts, a machine lock does not");
        int tl = -1;
        for (int i = 0; i < M2K_LOCKS; i++) if (b.lock[i].trk == 0 && b.lock[i].param == P_SYN) tl = b.lock[i].val;
        CHECK(b.kit[0][P_SYN] == 32 && tl == 96 && b.kit[1][P_SYN] == 63,
              "format: version 5 -> 6 keeps GND-SIN's TUNE in semitones (-1: 32, a lock +1: 96), SWAV-SAW's A as it was");
        CHECK(pat_unpack(&c, bl, n) && !memcmp(&b, &c, sizeof b), "format: the conversion is deterministic");
        int n2 = pat_pack(&b, bl2);
        CHECK(pat_unpack(&c, bl2, n2) && !memcmp(b.kit, c.kit, sizeof b.kit) && !memcmp(b.lock, c.lock, sizeof b.lock),
              "format: version 6 loads unchanged (no second conversion)");
        bl[1] = 5;
        CHECK(pat_unpack(&c, bl, n) && c.kit[0][P_SYN] == 32 && c.kit[0][P_DTIM] == 8,
              "format: version 5 takes only the TUNE");
        bl[1] = 3;
        CHECK(pat_unpack(&c, bl, n) && c.kit[1][0] == 0 && c.kit[1][7] == 64 && c.kit[0][P_DTIM] == 32,
              "format: version 3 takes SWAV-SAW's page and the chain");
        n = pat_pack(&pat[0], bl);
        CHECK(pat_unpack(&c, bl, n) && !memcmp(pat[0].kit, c.kit, sizeof c.kit), "format: a saved demo reloads as it was");
        /* no conversion moves a pitch: every demo saved and reloaded as version 4 and 5 keeps its notes, its PTCH,
         * its machines' pages and the locks on them (GND-SIN's TUNE converts, and no demo moves it) */
        int moved = 0;
        for (int d = 0; d < 16; d++)
            for (int ver = 4; ver <= 5; ver++) {
                n = pat_pack(&pat[d], bl);
                bl[1] = (uint8_t)ver;
                if (!pat_unpack(&c, bl, n)) { moved++; continue; }
                for (int t = 0; t < M2K_TRACKS; t++) {               /* SWAV-ENS's page converts at 7, by design */
                    moved += c.kit[t][P_PTCH] != pat[d].kit[t][P_PTCH] ||
                             (pat[d].machine[t] != M_ENS && memcmp(c.kit[t], pat[d].kit[t], 8) != 0);
                    for (int s = 0; s < M2K_STEPS; s++)
                        moved += c.step[t][s].note != pat[d].step[t][s].note || c.step[t][s].flags != pat[d].step[t][s].flags;
                }
                for (int i = 0; i < M2K_LOCKS; i++)
                    if (pat[d].lock[i].trk != 0xFF && pat[d].machine[pat[d].lock[i].trk] != M_ENS &&
                        (pat[d].lock[i].param < 8 || pat[d].lock[i].param == P_PTCH))
                        moved += c.lock[i].val != pat[d].lock[i].val;
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
    CHECK(!strcmp(pat[1].name, "JUNGLE"), "load DEMO again");
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
    /* a store saved before today's demos: the slots still holding the old ones take the new, the owner's stay */
    {
        m2k_init(11);
        m2k_pattern_t keep2 = pat[2];
        s_cpy(pat[2].name, "SPU RM");                        /* an old demo, as shipped */
        s_cpy(pat[3].name, "MYSONG");                        /* the owner's */
        pattern_init_blank(&pat[15]);
        pat[15].step[0][0].flags = ST_TRIG;                  /* A16 INIT, but written in: the owner's */
        project_save(0);
        int n = m2k_store_pack();
        memmove(store_buf + 7, store_buf + 8, (size_t)n - 8);  /* as a store before the factory's revision was kept */
        store_buf[4] = 1;
        m2k_init(12);
        CHECK(m2k_store_unpack(n - 1) == 1, "store: one saved before today's demos loads");
        CHECK(!strcmp(pat[2].name, keep2.name) && !memcmp(pat[2].kit, keep2.kit, sizeof keep2.kit),
              "store: an old demo left in its slot becomes today's");
        CHECK(!strcmp(pat[3].name, "MYSONG") && !strcmp(pat[15].name, "INIT") && (pat[15].step[0][0].flags & ST_TRIG),
              "store: the owner's patterns are kept");
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
