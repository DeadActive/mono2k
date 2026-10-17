/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's side of the comparison: reads the same spec as tools/ref/mm_ref (name, Monomachine machine number, note,
 * velocity, gate, tail) and renders each line through MONO2K's counterpart, from the blank pattern, as a WAV.
 *   cc -O2 -Ifirmware/src -Ifirmware/gen -o build/host/m2k_ref host/m2k_ref.c -lm
 *   build/host/m2k_ref build/ref/m2k < tools/ref/basic.spec */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "m2k_core.c"

/* the Monomachine's machine numbers (as its own screen names them) to MONO2K's machines */
static int machine_of(int mm)
{
    switch (mm) {
    case 1: return M_GSIN;
    case 2: return M_GNOI;
    case 3: return M_SID;
    case 4: return M_SAW;
    case 6: return M_WAV;
    case 8: return M_FMS;
    case 10: return M_FMD;
    case 11: return M_VOX;
    case 14: return M_ENS;
    default: return -1;
    }
}

static void wav(const char *path, const int16_t *s, uint32_t frames)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    uint32_t bytes = frames * 4, v;
    uint16_t h;
    fwrite("RIFF", 1, 4, f); v = 36 + bytes; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); h = 1; fwrite(&h, 2, 1, f); h = 2; fwrite(&h, 2, 1, f);
    v = M2K_SR; fwrite(&v, 4, 1, f); v = M2K_SR * 4; fwrite(&v, 4, 1, f); h = 4; fwrite(&h, 2, 1, f); h = 16; fwrite(&h, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f); fwrite(s, 4, frames, f);
    fclose(f);
}

static void run(int16_t *out, uint32_t frames, uint32_t *ms)
{
    for (uint32_t i = 0; i < frames; i += 128) {
        m2k_render(out + i * 2, 128);
        *ms += 3;                                   /* about 128 frames */
        m2k_frame(*ms);
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "m2k_ref DIR < SPEC\n"); return 2; }
    char line[512];
    while (fgets(line, sizeof line, stdin)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char name[64];
        int mm, note, vel;
        double gate, tail;
        if (sscanf(line, "%63s %d %d %d %lf %lf", name, &mm, &note, &vel, &gate, &tail) != 6) continue;
        int m = machine_of(mm);
        if (m < 0) { fprintf(stderr, "%s: no counterpart for machine %d\n", name, mm); continue; }
        m2k_init(1234);
        uint32_t ms = 0;
        for (int i = 0; i < 200; i++) m2k_frame(ms += 16);   /* past the splash */
        m2k_pattern(16);                                       /* B01, a blank pattern */
        m2k_pattern_t *p = &pat[cur_pat];
        p->machine[0] = (uint8_t)m;
        m_cpy(p->kit[0], MACH_DEF[m], 8);
        m_cpy(trk[0].eff, p->kit[0], P_COUNT);
        /* the line's controllers, through MONO2K's own copy of the Monomachine's CC map */
        const char *c = line;
        for (int f = 0; f < 6 && *c; f++) { while (*c && *c != ' ' && *c != '\t') c++; while (*c == ' ' || *c == '\t') c++; }
        while (*c && *c != '\n') {
            int cc, v, used, skip = 0;
            /* "mm:cc=v" is the Monomachine's alone (a control MONO2K keeps elsewhere); "m2k:cc=v" is MONO2K's alone */
            if (!strncmp(c, "mm:", 3)) { c += 3; skip = 1; }
            else if (!strncmp(c, "m2k:", 4)) c += 4;
            if (*c == 'p') {               /* "m2k:pN=v": MONO2K's kit parameter N as it is (one no controller reaches) */
                if (sscanf(c, "p%d=%d%n", &cc, &v, &used) != 2 || cc < 0 || cc >= P_COUNT) break;
                p->kit[0][cc] = (uint8_t)v;
                trk[0].eff[cc] = (uint8_t)v;
                c += used;
                while (*c == ' ' || *c == '\t') c++;
                continue;
            }
            if (sscanf(c, "%d=%d%n", &cc, &v, &used) != 2) break;
            if (skip) { c += used; while (*c == ' ' || *c == '\t') c++; continue; }
            m2k_midi(0xB0, cc, v);
            c += used;
            while (*c == ' ' || *c == '\t') c++;
        }
        for (int i = 0; i < 4; i++) m2k_frame(ms += 16);
        {   /* let what glides settle (the delay's time, as the reference renderer waits 1.5 s on the Monomachine) */
            static int16_t pre[M2K_SR / 2 * 2];
            run(pre, M2K_SR / 2 & ~127u, &ms);
        }
        uint32_t g = (uint32_t)(gate * M2K_SR) & ~127u, tl = (uint32_t)(tail * M2K_SR) & ~127u;
        int16_t *s = calloc((size_t)(g + tl) * 2, sizeof *s);
        m2k_midi(0x90, note, vel);
        run(s, g, &ms);
        m2k_midi(0x80, note, 0);
        run(s + g * 2, tl, &ms);
        char path[512];
        snprintf(path, sizeof path, "%s/%s.wav", argv[1], name);
        wav(path, s, g + tl);
        free(s);
        puts(name);
    }
    return 0;
}
