/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's profile: every pattern of bank A played SECONDS (default 8) and the render's time by part and by machine,
 * as a share of real time on this computer (the FM-1's test build shows the same parts on its screen).
 *   cc -O2 -Ifirmware/src -Ifirmware/gen -DM2K_PROF -o build/host/m2k_prof host/m2k_prof.c && build/host/m2k_prof [SECONDS] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "m2k_core.c"

uint32_t m2k_prof_now(void) { return (uint32_t)(clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 100u); }   /* 0.1 us */

int main(int argc, char **argv)
{
    int sec = argc > 1 ? atoi(argv[1]) : 8, frames = M2K_SR * sec / 128 * 128;
    static const char *const PART[5] = {"seq+lfo", "machines", "chain", "delays", "bus"};
    static const char *const MN[M_COUNT] = {"GND-SIN", "GND-PERC", "SID", "FM+STAT", "FM+DYN", "SWAV-SAW", "SWAV-ENS",
                                            "DIGI-WAV", "VOX-6", "FM+PAR", "SWAV-PUL", "FX-CHORS", "FX-RING", "FX-DYNMX", "GND-NOIS",
                                            "DRM-KICK", "DRM-SNAR", "DRM-HATS", "DRM-CYMB"};
    static int16_t buf[256];
    uint64_t tot[5] = {0}, mt[M_COUNT] = {0}, real = 0;
    int mn[M_COUNT] = {0};
    printf("pattern   total  seq+lfo machines  chain delays   bus   (%% of real time here)\n");
    for (int p = 0; p < 16; p++) {
        m2k_init(2000);
        cur_pat = p; sq.queued = (uint8_t)p;
        lockmap_build();
        for (int t = 0; t < M2K_TRACKS; t++) { m_cpy(trk[t].eff, pat[p].kit[t], P_COUNT); mn[pat[p].machine[t]]++; }
        seq_play(1);
        memset(m2k_prof, 0, sizeof m2k_prof); memset(m2k_prof_m, 0, sizeof m2k_prof_m);
        for (int f = 0; f < frames; f += 128) m2k_render(buf, 128);
        double us = sec * 1e7, sum = 0;                          /* the real time, in 0.1 us */
        for (int k = 0; k < 5; k++) { sum += m2k_prof[k]; tot[k] += m2k_prof[k]; }
        for (int m = 0; m < M_COUNT; m++) mt[m] += m2k_prof_m[m];
        real += (uint64_t)us;
        printf("A%02d %-7s %5.2f  %6.2f  %7.2f  %5.2f  %5.2f  %5.2f\n", p + 1, pat[p].name, 100 * sum / us,
               100 * m2k_prof[0] / us, 100 * m2k_prof[1] / us, 100 * m2k_prof[2] / us, 100 * m2k_prof[3] / us, 100 * m2k_prof[4] / us);
    }
    double s = 0;
    for (int k = 0; k < 5; k++) s += tot[k];
    printf("all         %5.2f ", 100 * s / real);
    for (int k = 0; k < 5; k++) printf(" %s %.2f", PART[k], 100.0 * tot[k] / real);
    printf("\nmachines, per track playing it (%% of real time):\n");
    for (int m = 0; m < M_COUNT; m++) if (mn[m]) printf("  %-9s %5.3f  (%d tracks)\n", MN[m], 100.0 * mt[m] / real * 16 / mn[m], mn[m]);
    return 0;
}
