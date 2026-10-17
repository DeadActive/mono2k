/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K: the unity build of the core. Every unit is included here, so everything can stay static. */
#include "m2k.h"
#include "m2k_tables.h"
#include "m2k_waves64.h"
#include "m2k_fonts.h"
#include "m2k_icons.h"

#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))

static void m_set(void *d, int v, uint32_t n) { uint8_t *p = d; while (n--) *p++ = (uint8_t)v; }
static void m_cpy(void *d, const void *s, uint32_t n) { uint8_t *p = d; const uint8_t *q = s; while (n--) *p++ = *q++; }

static uint32_t rng = 0x2000u;
static inline uint32_t rnd32(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

static inline int32_t sat16(int32_t x) { return x > 32767 ? 32767 : x < -32767 ? -32767 : x; }

static inline int32_t sinp(uint32_t ph)
{
    uint32_t i = ph >> 22, f = (ph >> 6) & 0xFFFFu;
    int32_t a = T_SIN[i], b = T_SIN[i + 1];
    return a + (((b - a) * (int32_t)f) >> 16);
}

/* pitch in 1/64 semitones (MIDI note * 64) to a phase increment per sample */
static inline uint32_t inc_of(int32_t pq6)
{
    if (pq6 < 0) pq6 = 0;
    if (pq6 > 140 * 64) pq6 = 140 * 64;
    int32_t oct = pq6 / 768, r = pq6 - oct * 768;
    int32_t sh = 10 - oct;
    if (sh >= 0) return T_INC_TOP[r] >> sh;
    uint32_t v = T_INC_TOP[r];
    return v > (0x7FFFFFFFu >> (-sh - 1)) ? 0x7FFFFFFFu : v << -sh;
}

static inline uint32_t mulu_hi(uint32_t a, uint32_t b, int sh) { return (uint32_t)(((uint64_t)a * b) >> sh); }

/* soft saturation: -131072..131072 in, full scale out */
static inline int32_t sat_soft(int32_t x)
{
    if (x >= 131071) return 32767;
    if (x <= -131071) return -32767;
    int32_t u = x + 131072, i = u >> 10, f = u & 1023;
    return T_SAT[i] + (((T_SAT[i + 1] - T_SAT[i]) * f) >> 10);
}

/* the bus limiter: straight to 3/4 of full scale, then a knee; -131072..131072 in */
static inline int32_t lim_soft(int32_t x)
{
    if (x >= 131071) return 32767;
    if (x <= -131071) return -32767;
    int32_t u = x + 131072, i = u >> 10, f = u & 1023;
    return T_LIM[i] + (((T_LIM[i + 1] - T_LIM[i]) * f) >> 10);
}

/* the state the screen reads from the audio side */
static struct {
    int32_t peak[M2K_TRACKS];          /* 0..32767, decays in the UI */
    uint8_t trig[M2K_TRACKS];          /* counts up on every trig: the UI flashes on change */
    int32_t aenv[M2K_TRACKS];          /* Q15 */
    uint8_t astage[M2K_TRACKS];
    int32_t cut_hp[M2K_TRACKS], cut_lp[M2K_TRACKS];   /* q3 cutoff index, live */
    int32_t lfo[M2K_TRACKS][3];        /* -32767..32767 */
    int16_t scope[256];                /* the master, every 8th sample */
    uint8_t scope_w;
    uint8_t glitch;                    /* 1 while the bus repeats */
    int32_t dly[M2K_TRACKS];           /* delay return level */
} mon;

#include "m2k_dsp.c"
#include "m2k_fx.c"
#include "m2k_seq.c"
#include "m2k_gfx.c"
#include "m2k_ui.c"
