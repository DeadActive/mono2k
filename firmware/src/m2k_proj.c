/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's projects, the modern Elektron way: every pattern carries its own sounds. A project is 4 banks (A-D)
 * of 16 patterns. The working patterns live unpacked in RAM (pat[]); the saved projects are packed images: each
 * pattern as a blob of its sounds, its trigs only and its locks only, so a typical one is 0.5-1 KB.
 *   SAVE          the current pattern's reload point (into the open project's image)
 *   GLO + SAVE    reload: the current pattern back to that point
 *   hold SAVE     the whole project
 * The images live in RAM; the store (them, the working patterns, the settings) is one stream, into a buffer on the
 * computer (host, browser) and into the flash on the FM-1 (m2k_fw_store_put / _get), never whole in its RAM. */

#ifndef M2K_IMG_DATA
#define M2K_IMG_DATA (160u * 1024u)        /* a project's packed patterns (the FM-1's first build: 28 KB, 16 of them) */
#endif
#define IMG_DATA M2K_IMG_DATA
#define BLOB_MAX (32 + M2K_TRACKS * P_COUNT + M2K_TRACKS * M2K_STEPS / 8 + M2K_TRACKS * M2K_STEPS * 4 + 2 + M2K_LOCKS * 4)

typedef struct {
    char name[8];
    uint32_t used;
    uint16_t len[M2K_PATTERNS];
    uint8_t data[IMG_DATA];
} m2k_img_t;

static m2k_img_t proj[M2K_PROJECTS] POOL;
static m2k_img_t work_img POOL;              /* scratch: packing a whole project, the store */
static uint8_t blob[BLOB_MAX];
static int cur_proj;

/* -------------------------------------------------------------------------------------- one pattern, packed */

/* a pattern in the layout of a version (14, today's; the self-test writes the older ones, 3..13; 3..7 kits 70 wide) */
static int pat_pack_v(const m2k_pattern_t *p, uint8_t *o, int ver)
{
    int n = 0, stride = ver >= 8 ? P_COUNT : P_WBNK;
    o[n++] = 'P';
    o[n++] = (uint8_t)ver;             /* 2: amp envelope on the Monomachine's times; 3: MOD; 4: SWAV-SAW's new page;
                                        * 5: the track chain on the Monomachine's; 6: GND-SIN's TUNE in 3.125 c;
                                        * 7: SWAV-ENS's page; 8: the wave bank's choice (kits 71 wide); 9: the bus's
                                        * controls on their new laws; 10: FM+STAT's and FM+DYN's pages; 11: SID's;
                                        * 12: GND-SIN's (TUNE on its eighth knob); 13: HOLD in beats; 14: VO-6's page */
    m_cpy(o + n, p->name, 8); n += 8;
    m_cpy(o + n, p->machine, M2K_TRACKS); n += M2K_TRACKS;
    m_cpy(o + n, p->glob, G_COUNT); n += G_COUNT;
    o[n++] = p->mute;
    for (int t = 0; t < M2K_TRACKS; t++) { m_cpy(o + n, p->kit[t], stride); n += stride; }
    uint8_t *mask = o + n;
    m_set(mask, 0, M2K_TRACKS * M2K_STEPS / 8);
    n += M2K_TRACKS * M2K_STEPS / 8;
    for (int t = 0; t < M2K_TRACKS; t++)
        for (int s = 0; s < M2K_STEPS; s++) {
            const m2k_step_t *st = &p->step[t][s];
            if (!(st->flags & ST_TRIG)) continue;
            int b = t * M2K_STEPS + s;
            mask[b >> 3] |= (uint8_t)(1u << (b & 7));
            o[n++] = st->note; o[n++] = st->flags; o[n++] = st->cond; o[n++] = st->rtrg;
        }
    int nl = 0, at = n;
    n += 2;
    for (int i = 0; i < M2K_LOCKS; i++) {
        const m2k_lock_t *l = &p->lock[i];
        if (l->trk >= M2K_TRACKS) continue;
        o[n++] = l->trk; o[n++] = l->step; o[n++] = l->param; o[n++] = l->val;
        nl++;
    }
    o[at] = (uint8_t)(nl & 0xFF);
    o[at + 1] = (uint8_t)(nl >> 8);
    return n;
}

static int pat_pack(const m2k_pattern_t *p, uint8_t *o) { return pat_pack_v(p, o, 14); }

static uint8_t tune_v5(int v)
{
    v = 64 + (v - 64) * 32;
    return (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
}

static int pat_unpack(m2k_pattern_t *p, const uint8_t *o, int len)
{
    int n = 0;
    if (len < 2 || o[0] != 'P' || o[1] < 1 || o[1] > 14) return 0;
    int ver = o[1], stride = ver >= 8 ? P_COUNT : ver >= 3 ? P_WBNK : P_MODT;   /* before 3 a kit ended before the MOD
                                                                               * effect, before 8 before the bank */
    const int head = 2 + 8 + M2K_TRACKS + G_COUNT + 1 + M2K_TRACKS * stride + M2K_TRACKS * M2K_STEPS / 8 + 2;
    if (len < head) return 0;
    m_set(p, 0, sizeof *p);
    n = 2;
    m_cpy(p->name, o + n, 8); n += 8;
    p->name[7] = 0;
    m_cpy(p->machine, o + n, M2K_TRACKS); n += M2K_TRACKS;
    m_cpy(p->glob, o + n, G_COUNT); n += G_COUNT;
    p->mute = o[n++];
    for (int t = 0; t < M2K_TRACKS; t++) {
        m_cpy(p->kit[t], o + n, stride); n += stride;
        if (stride <= P_MODT) { p->kit[t][P_MODT] = 0; p->kit[t][P_MODR] = 40; p->kit[t][P_MODD] = 64; p->kit[t][P_MODM] = 0; }
    }
    const uint8_t *mask = o + n;
    n += M2K_TRACKS * M2K_STEPS / 8;
    for (int t = 0; t < M2K_TRACKS; t++) {
        if (p->machine[t] >= M_COUNT) p->machine[t] = M_GSIN;
        for (int s = 0; s < M2K_STEPS; s++) {
            m2k_step_t *st = &p->step[t][s];
            st->note = 60;
            int b = t * M2K_STEPS + s;
            if (!((mask[b >> 3] >> (b & 7)) & 1)) continue;
            if (n + 4 > len) return 0;
            st->note = o[n]; st->flags = o[n + 1]; st->cond = o[n + 2] < C_COUNT ? o[n + 2] : C_ALWAYS; st->rtrg = o[n + 3];
            n += 4;
        }
    }
    if (n + 2 > len) return 0;
    int nl = o[n] | (o[n + 1] << 8);
    n += 2;
    for (int i = 0; i < M2K_LOCKS; i++) p->lock[i].trk = 0xFF;
    for (int i = 0; i < nl && i < M2K_LOCKS; i++) {
        if (n + 4 > len) return 0;
        p->lock[i] = (m2k_lock_t){o[n], o[n + 1], o[n + 2], o[n + 3]};
        if (p->lock[i].trk >= M2K_TRACKS || p->lock[i].step >= M2K_STEPS || p->lock[i].param >= P_COUNT) p->lock[i].trk = 0xFF;
        n += 4;
    }
    if (ver == 1) amp_v1(p);
    if (ver < 4)                       /* SWAV-SAW took the Monomachine's page: carry the old one over as near as it goes */
        for (int t = 0; t < M2K_TRACKS; t++) {
            if (p->machine[t] != M_SAW) continue;
            uint8_t *k = p->kit[t], o8[8];
            m_cpy(o8, k, 8);                       /* DTUN VOIC MIX SUB SHPE DRFT OCTV STRT */
            int voices = 1 + ((o8[1] * 7) >> 7);
            k[0] = voices > 1 ? o8[2] : 0;         /* UNIL from the side mix */
            k[1] = (uint8_t)(o8[0] * 2 > 127 ? 127 : o8[0] * 2);   /* UNIW from the detune */
            k[2] = voices >= 5 ? 100 : 0;          /* UNIX when there were five voices or more */
            k[3] = o8[5];                          /* DRFT */
            k[4] = o8[3];                          /* SUBX: the old sub was a square */
            k[5] = 0; k[6] = 0; k[7] = 64;         /* SUB1, SUB2, TUNE centred */
            for (int i = 0; i < M2K_LOCKS; i++)
                if (p->lock[i].trk == t && p->lock[i].param < 8) p->lock[i].trk = 0xFF;
        }
    if (ver < 3) {                     /* the reverb's return came up 12 dB: half the sends keep the same space */
        for (int t = 0; t < M2K_TRACKS; t++) p->kit[t][P_SPU] >>= 1;
        for (int i = 0; i < M2K_LOCKS; i++) if (p->lock[i].trk != 0xFF && p->lock[i].param == P_SPU) p->lock[i].val >>= 1;
    }
    if (ver < 5) chain_v4(p);          /* the chain became the Monomachine's */
    if (ver < 6)                       /* GND-SIN's TUNE was a semitone a step: the Monomachine's +-2 semitones hold +-2 */
        for (int t = 0; t < M2K_TRACKS; t++) {
            if (p->machine[t] != M_GSIN) continue;
            p->kit[t][P_SYN] = tune_v5(p->kit[t][P_SYN]);
            for (int i = 0; i < M2K_LOCKS; i++)
                if (p->lock[i].trk == t && p->lock[i].param == P_SYN) p->lock[i].val = tune_v5(p->lock[i].val);
        }
    if (ver < 7) ens_v6(p);           /* SWAV-ENS took the Monomachine's page */
    if (ver < 9) bus_v9(p);           /* the bus's controls took new laws */
    if (ver < 10) fm_v9(p);           /* FM+STAT and FM+DYN took the Monomachine's pages */
    if (ver < 11) sid_v10(p);         /* and SID SID-6581's */
    if (ver < 12) gsin_v11(p);        /* and GND-SIN's: TUNE on its eighth knob */
    if (ver < 13) hold_v12(p);        /* HOLD went from 14.6 ms a step to 1 / 32 beat */
    if (ver < 14) vox_v13(p);         /* VOX-6 took VO-6's page */
    return 1;
}

/* ------------------------------------------------------------------------------------------------ an image */

static uint32_t img_off(const m2k_img_t *im, int i)
{
    uint32_t o = 0;
    for (int k = 0; k < i; k++) o += im->len[k];
    return o;
}

static int img_put(m2k_img_t *im, int i, const uint8_t *b, int n)
{
    uint32_t off = img_off(im, i), old = im->len[i];
    if (im->used - old + (uint32_t)n > IMG_DATA) return 0;
    uint32_t tail = im->used - off - old;
    /* move the tail to its new place; regions overlap: copy in the safe direction */
    uint8_t *src = im->data + off + old, *dst = im->data + off + (uint32_t)n;
    if (dst < src) for (uint32_t k = 0; k < tail; k++) dst[k] = src[k];
    else for (uint32_t k = tail; k > 0; k--) dst[k - 1] = src[k - 1];
    m_cpy(im->data + off, b, (uint32_t)n);
    im->len[i] = (uint16_t)n;
    im->used = im->used - old + (uint32_t)n;
    return 1;
}

static const uint8_t *img_get(const m2k_img_t *im, int i, int *n)
{
    *n = im->len[i];
    return im->data + img_off(im, i);
}

static void img_clear(m2k_img_t *im, const char *name)
{
    m_set(im->name, 0, sizeof im->name);
    for (int i = 0; i < 7 && name[i]; i++) im->name[i] = name[i];
    im->used = 0;
    m_set(im->len, 0, sizeof im->len);
}

/* every working pattern into the scratch image; 0 when they do not fit */
static int img_pack_work(const char *name)
{
    img_clear(&work_img, name);
    for (int i = 0; i < M2K_PATTERNS; i++) {
        int n = pat_pack(&pat[i], blob);
        if (!img_put(&work_img, i, blob, n)) return 0;
    }
    return 1;
}

/* every working pattern into an image; 0 when they do not fit (the image is then left as it was) */
static int img_pack_all(m2k_img_t *dst, const char *name)
{
    if (!img_pack_work(name)) return 0;
    m_cpy(dst, &work_img, (uint32_t)(sizeof *dst - IMG_DATA + work_img.used));
    return 1;
}

static m2k_pattern_t pat_tmp POOL;           /* scratch: the factory's blank, an old demo read back */

/* the sound library: four banks of 16 sounds (S01..S64), each a track's machine and its parameters, kept with the
 * store; an empty slot has no name (M2K_SOUNDS slots, m2k.h). The factory fills it from bank A's tracks */
typedef struct { char name[8]; uint8_t machine, kit[P_COUNT]; } m2k_sound_t;
static m2k_sound_t snd[M2K_SOUNDS];                /* (RAM: the FM-1's pool is full) */

static void sound_name(char *o, const m2k_pattern_t *p, int t)   /* "MAMM T3": the pattern's name, the track */
{
    int i = 0;
    for (; i < 4 && p->name[i] && p->name[i] != ' '; i++) o[i] = p->name[i];
    o[i++] = 'T';
    o[i++] = (char)('1' + t);
    o[i] = 0;
}

static void sound_save(int slot, const m2k_pattern_t *p, int t)
{
    m2k_sound_t *d = &snd[slot % M2K_SOUNDS];
    m_set(d, 0, sizeof *d);
    sound_name(d->name, p, t);
    d->machine = p->machine[t];
    m_cpy(d->kit, p->kit[t], P_COUNT);
}

static void sounds_factory(void)              /* bank A's tracks, A01 T1 first */
{
    m_set(snd, 0, sizeof snd);
    for (int i = 0; i < M2K_SOUNDS && i / M2K_TRACKS < (M2K_PATTERNS < 16 ? M2K_PATTERNS : 16); i++)
        sound_save(i, &pat[i / M2K_TRACKS], i % M2K_TRACKS);
}

static void pattern_init_blank(m2k_pattern_t *p)
{
    pat_init(p, "INIT");
    static const uint8_t M[M2K_TRACKS] = {M_GSIN, M_GPRC, M_GPRC, M_SID, M_FMS, M_ENS};
    for (int t = 0; t < M2K_TRACKS; t++) kit_default(p, t, M[t]);
    p->glob[G_TMPO] = 120 - 30;
    p->glob[G_VERB] = 40;
}

/* the working patterns from an image: empty or broken slots become blank patterns */
static void img_unpack_all(const m2k_img_t *im)
{
    for (int i = 0; i < M2K_PATTERNS; i++) {
        int n;
        const uint8_t *b = img_get(im, i, &n);
        if (!n || !pat_unpack(&pat[i], b, n)) pattern_init_blank(&pat[i]);
    }
}

/* ---------------------------------------------------------------------------------------------- the actions */

static void work_reset_current(void)
{
    lockmap_build();
    for (int t = 0; t < M2K_TRACKS; t++) m_cpy(trk[t].eff, pat[cur_pat].kit[t], P_COUNT);
}

static int pattern_save(void)
{
    int n = pat_pack(&pat[cur_pat], blob);
    return img_put(&proj[cur_proj], cur_pat, blob, n);
}

static int pattern_reload(void)
{
    int n;
    const uint8_t *b = img_get(&proj[cur_proj], cur_pat, &n);
    if (n) { if (!pat_unpack(&pat[cur_pat], b, n)) return 0; }
    else pattern_init_blank(&pat[cur_pat]);
    work_reset_current();
    return 1;
}

static int project_save(int slot)
{
    char name[8];
    m_cpy(name, proj[slot].name, 8);
    if (!name[0]) { name[0] = 'P'; name[1] = (char)('1' + slot); name[2] = 0; }
    if (!img_pack_all(&proj[slot], name)) return 0;
    cur_proj = slot;
    return 1;
}

static void seq_play(int on);

static void project_load(int slot)
{
    seq_play(0);
    img_unpack_all(&proj[slot]);
    cur_proj = slot;
    cur_pat = 0;
    sq.queued = 0;
    work_reset_current();
}

/* the factory: project 1 is DEMO (bank A the demos, B-D blank), projects 2-4 blank */
static void projects_factory(void)
{
    factory();
    sounds_factory();
    for (int i = 16; i < M2K_PATTERNS; i++) pattern_init_blank(&pat[i]);
    img_pack_all(&proj[0], "DEMO");
    /* the blank projects: one blank pattern, 64 times */
    pattern_init_blank(&pat_tmp);
    int n = pat_pack(&pat_tmp, blob);
    for (int k = 1; k < M2K_PROJECTS; k++) {
        char nm[8] = {'P', (char)('1' + k), 0};
        img_clear(&proj[k], nm);
        for (int i = 0; i < M2K_PATTERNS; i++) img_put(&proj[k], i, blob, n);
    }
    cur_proj = 0;
}

/* ---------------------------------------------------------------------------------------------- the store */
/* everything the instrument keeps, as one blob for the platform to save: the settings, the working patterns,
 * the four projects. "M2K2", version, open project; then images as (name, used, len[], data). */

#define STORE_MAX (16 + S_COUNT + (M2K_PROJECTS + 1) * (12 + 2 * M2K_PATTERNS + IMG_DATA) + 2 + 80 * 64)
#ifndef M2K_FW
static uint8_t store_buf[STORE_MAX] POOL;
#endif

/* the stream: st_put writes at st_at and moves on, st_get reads so (0 past the end, or when the flash fails) */
static uint32_t st_at;
static int st_bad;

static void st_put(const void *p, uint32_t n)
{
#ifdef M2K_FW
    if (!st_bad && m2k_fw_store_put(st_at, (const uint8_t *)p, n)) st_bad = 1;
#else
    if (st_at + n <= STORE_MAX) m_cpy(store_buf + st_at, p, n);
    else st_bad = 1;
#endif
    st_at += n;
}

static int st_get(void *p, uint32_t n, uint32_t len)
{
    if (st_at + n > len) return 0;
#ifdef M2K_FW
    if (m2k_fw_store_get(st_at, (uint8_t *)p, n)) return 0;
#else
    m_cpy(p, store_buf + st_at, n);
#endif
    st_at += n;
    return 1;
}

/* an image as the stream has it: its name, its length, the lengths of its first n patterns (n may differ from this
 * build's: the FM-1 keeps 16 a project, the browser 64), their data */
static void ser_img(const m2k_img_t *im, int n)
{
    uint8_t h[12], l[2];
    uint32_t used = 0;
    for (int i = 0; i < n && i < M2K_PATTERNS; i++) used += im->len[i];
    m_cpy(h, im->name, 8);
    for (int k = 0; k < 4; k++) h[8 + k] = (uint8_t)(used >> (8 * k));
    st_put(h, 12);
    for (int i = 0; i < n; i++) {
        uint16_t v = i < M2K_PATTERNS ? im->len[i] : 0;
        l[0] = (uint8_t)v; l[1] = (uint8_t)(v >> 8);
        st_put(l, 2);
    }
    st_put(im->data, used);
}

/* an image from the stream, of n patterns: the first this build keeps (those past it skipped, those short of it blank);
 * through the scratch image (an image refused is left as it was), or straight into im. im 0: skipped whole */
static int deser_img(m2k_img_t *im, uint32_t len, int direct, int n)
{
    m2k_img_t *t = direct && im ? im : &work_img;
    uint8_t h[12], l[2];
    uint32_t used, keep = 0, sum = 0;
    if (!st_get(h, 12, len)) return 0;
    used = (uint32_t)h[8] | ((uint32_t)h[9] << 8) | ((uint32_t)h[10] << 16) | ((uint32_t)h[11] << 24);
    if (!im) {                                          /* not kept: past it */
        if (st_at + 2u * (uint32_t)n + used > len) return 0;
        st_at += 2u * (uint32_t)n + used;
        return 1;
    }
    m_cpy(t->name, h, 8);
    t->name[7] = 0;
    m_set(t->len, 0, sizeof t->len);
    for (int i = 0; i < n; i++) {
        if (!st_get(l, 2, len)) return 0;
        uint16_t v = (uint16_t)(l[0] | (l[1] << 8));
        sum += v;
        if (i < M2K_PATTERNS) { t->len[i] = v; keep += v; }
    }
    if (sum != used || keep > IMG_DATA || !st_get(t->data, keep, len)) return 0;
    if (st_at + (used - keep) > len) return 0;
    st_at += used - keep;
    t->used = keep;
    if (!direct) m_cpy(im, t, (uint32_t)(sizeof *im - IMG_DATA + t->used));
    return 1;
}

#ifndef M2K_FW
uint8_t *m2k_store_buf(int *max)
{
    *max = STORE_MAX;
    return store_buf;
}
#endif

/* the factory's revision: a store saved before it carries the demos it replaced, by revision. 1: the bank rewritten
 * on the Monomachine's chain (version 5); 2: bank A's first four parts after the brief (docs/bank-a.md: HAZE,
 * SPLICE, RIBBON, FACETS); 3: the next four (SMOKE, TORQUE, TUNNEL, MOTEL); 4: brief v3's first four (PYLONS,
 * RIVETS, ORCHARD, SPLINTR), and HAZE, SPLICE and RIBBON set to sound on the per-machine DIST as they did; 5: A04
 * TENFOUR and A16 FOUNDRY, after Radiohead and Boards of Canada; 6: A06 CAROM, A07 VESPER, A12 CUTUP, A13 GRISTLE;
 * 7: A09 MAMMOTH (direction v5) for PYLONS; 8: the bank reordered by energy, MAMMOTH first; 9: CUTUP to A02,
 * RIVETS to A11 */
#define FACTORY_REV 9
static const char *const OLD_DEMO[FACTORY_REV][16] = {
    {"LATEX", "PVC", "SPU RM", "ELECTRO", "GLITCH", "CHROME", "ACID", "2STEP", "GROOVE", "HYPRPOP", "DUB", "CHIP",
     "HARDGRV", "IDM", "DUBSTEP", "INIT"},
    {"DEEP", "JUNGLE", "DAYDRM", "ELECTRO", "GLITCH", "PRISM", "FOG", "LIQUID", "DREXC", "STUTTR", "SYRUP", "SPU",
     "GROOVE", "ROBOT", "BRAIN", "SHARDS"},
    {"HAZE", "SPLICE", "RIBBON", "FACETS", "GLITCH", "PRISM", "FOG", "LIQUID", "DREXC", "STUTTR", "SYRUP", "SPU",
     "GROOVE", "ROBOT", "BRAIN", "SHARDS"},
    {"HAZE", "SPLICE", "RIBBON", "FACETS", "SMOKE", "TORQUE", "TUNNEL", "MOTEL", "DREXC", "STUTTR", "SYRUP", "SPU",
     "GROOVE", "ROBOT", "BRAIN", "SHARDS"},
    {"HAZE", "SPLICE", "RIBBON", "FACETS", "SMOKE", "TORQUE", "TUNNEL", "MOTEL", "PYLONS", "RIVETS", "SYRUP", "SPU",
     "GROOVE", "ORCHARD", "SPLINTR", "SHARDS"},
    {"HAZE", "SPLICE", "RIBBON", "TENFOUR", "SMOKE", "TORQUE", "TUNNEL", "MOTEL", "PYLONS", "RIVETS", "SYRUP", "SPU",
     "GROOVE", "ORCHARD", "SPLINTR", "FOUNDRY"},
    {"HAZE", "SPLICE", "RIBBON", "TENFOUR", "SMOKE", "CAROM", "VESPER", "MOTEL", "PYLONS", "RIVETS", "SYRUP", "CUTUP",
     "GRISTLE", "ORCHARD", "SPLINTR", "FOUNDRY"},
    {"HAZE", "SPLICE", "RIBBON", "TENFOUR", "SMOKE", "CAROM", "VESPER", "MOTEL", "MAMMOTH", "RIVETS", "SYRUP", "CUTUP",
     "GRISTLE", "ORCHARD", "SPLINTR", "FOUNDRY"},
    {"MAMMOTH", "RIVETS", "HAZE", "GRISTLE", "CAROM", "TENFOUR", "VESPER", "ORCHARD", "SPLINTR", "SMOKE", "CUTUP", "SYRUP",
     "RIBBON", "MOTEL", "FOUNDRY", "SPLICE"},
};

static int name_is(const char *a, const char *b)
{
    for (int i = 0; i < 8; i++) { if (a[i] != b[i]) return 0; if (!a[i]) return 1; }
    return 1;
}

/* a saved image of the DEMO project: a bank-A slot still holding the demo the store's revision shipped (by its name;
 * an INIT only while it is blank) takes today's demo, built in pat[] by the caller. Anything else is the owner's: kept */
static void refresh_demos(m2k_img_t *im, int rev)
{
    m2k_pattern_t *const o = &pat_tmp;
    for (int i = 0; i < 16; i++) {
        if (!im->len[i] || !pat_unpack(o, im->data + img_off(im, i), im->len[i]) || !name_is(o->name, OLD_DEMO[rev][i])) continue;
        if (name_is(OLD_DEMO[rev][i], "INIT")) {
            int trigs = 0;
            for (int t = 0; t < M2K_TRACKS; t++) for (int s = 0; s < M2K_STEPS; s++) trigs += o->step[t][s].flags & ST_TRIG;
            if (trigs) continue;
        }
        int n = pat_pack(&pat[i], blob);
        img_put(im, i, blob, n);
    }
}

/* the store, version 3: "M2K2" 3, the open project and pattern, the factory's revision, how many patterns a project and
 * how many projects it holds; the settings; the working patterns; the projects; the sound library ('S', its count,
 * each slot). n and k as this build keeps them, or fewer (the browser writing a store for the FM-1): an open project
 * past k goes first, as the open one */
static int store_pack_nk(int n, int k)
{
    uint8_t h[10] = {'M', '2', 'K', '2', 3, (uint8_t)cur_proj, (uint8_t)cur_pat, FACTORY_REV, (uint8_t)n, (uint8_t)k};
    if (cur_proj >= k) h[5] = 0;
    if (cur_pat >= n) h[6] = 0;
    st_at = 0;
    st_bad = 0;
    st_put(h, 10);
    st_put(store_.set, S_COUNT);
    if (!img_pack_work("WORK")) return 0;
    ser_img(&work_img, n);
    for (int i = 0; i < k; i++) ser_img(&proj[i == 0 && cur_proj >= k ? cur_proj : i < M2K_PROJECTS ? i : 0], n);
    uint8_t sh[2] = {'S', M2K_SOUNDS};
    st_put(sh, 2);
    st_put(snd, sizeof snd);
    return st_bad ? 0 : (int)st_at;
}

int m2k_store_pack(void) { return store_pack_nk(M2K_PATTERNS, M2K_PROJECTS); }

int m2k_store_pack_as(int patterns, int projects)
{
    if (patterns < 1 || patterns > M2K_PATTERNS || projects < 1 || projects > M2K_PROJECTS) return 0;
    return store_pack_nk(patterns, projects);
}

int m2k_store_unpack(int len)
{
    uint8_t o[10], set[S_COUNT];
    uint32_t n = (uint32_t)len;
    st_at = 0;
    if (!st_get(o, 5, n) || o[0] != 'M' || o[1] != '2' || o[2] != 'K' || o[3] != '2' || o[4] < 1 || o[4] > 3) return 0;
    int ver = o[4];
    if (!st_get(o + 5, ver == 1 ? 2 : ver == 2 ? 3 : 5, n)) return 0;
    int rev = ver >= 2 ? o[7] : 0;
    int np = ver >= 3 ? o[8] : M2K_PATTERNS, nk = ver >= 3 ? o[9] : M2K_PROJECTS;   /* before 3: this build's own */
    if (np < 1 || np > 64 || nk < 1 || nk > 16) return 0;
    if (!st_get(set, S_COUNT, n)) return 0;
    if (!deser_img(&work_img, n, 1, np)) return 0;          /* the working patterns, in the scratch image */
    for (int i = 0; i < nk; i++)
        if (!deser_img(i < M2K_PROJECTS ? &proj[i] : 0, n, 1, np)) { projects_factory(); return 0; }
    for (int i = nk; i < M2K_PROJECTS; i++) {               /* projects the store has not: blank ones */
        char nm[8] = {'P', (char)('1' + i), 0};
        pattern_init_blank(&pat_tmp);
        int bn = pat_pack(&pat_tmp, blob);
        img_clear(&proj[i], nm);
        for (int j = 0; j < M2K_PATTERNS; j++) img_put(&proj[i], j, blob, bn);
    }
    uint8_t sh[2];
    if (ver >= 3 && st_get(sh, 2, n) && sh[0] == 'S') {   /* the sound library: the slots this build keeps */
        m_set(snd, 0, sizeof snd);
        for (int i = 0; i < sh[1]; i++) {
            m2k_sound_t tmp;
            if (!st_get(&tmp, sizeof tmp, n)) break;
            if (i < M2K_SOUNDS) { snd[i] = tmp; snd[i].name[7] = 0; if (snd[i].machine >= M_COUNT) snd[i].name[0] = 0; }
        }
    }
    if (rev < FACTORY_REV && name_is(proj[0].name, "DEMO")) {      /* today's demos, where the old ones still are */
        factory();
        refresh_demos(&proj[0], rev);
        if (o[5] == 0) refresh_demos(&work_img, rev);
    }
    img_unpack_all(&work_img);
    m_cpy(store_.set, set, S_COUNT);
    cur_proj = o[5] < M2K_PROJECTS ? o[5] : 0;
    cur_pat = o[6] < M2K_PATTERNS ? o[6] : 0;
    sq.queued = (uint8_t)cur_pat;
    work_reset_current();
    return 1;
}

#ifndef M2K_FW
/* a project as a file: its saved image */
int m2k_proj_export(int slot)
{
    if (slot < 0 || slot >= M2K_PROJECTS) return 0;
    static const uint8_t h[6] = {'M', '2', 'K', 'P', 2, M2K_PATTERNS};   /* 2: the count of patterns follows */
    st_at = 0;
    st_bad = 0;
    st_put(h, 6);
    ser_img(&proj[slot], M2K_PATTERNS);
    return st_bad ? 0 : (int)st_at;
}

int m2k_proj_import(int slot, int len)
{
    const uint8_t *o = store_buf;
    if (slot < 0 || slot >= M2K_PROJECTS || len < 6 || o[0] != 'M' || o[1] != '2' || o[2] != 'K' || o[3] != 'P' || o[4] < 1 || o[4] > 2) return 0;
    st_at = o[4] == 1 ? 5 : 6;
    return deser_img(&proj[slot], (uint32_t)len, 0, o[4] == 1 ? 64 : o[5]);   /* 1: 64 a project, as the browser's */
}
#endif

void m2k_project(int slot, int save)
{
    if (slot < 0 || slot >= M2K_PROJECTS) return;
    if (save) project_save(slot);
    else project_load(slot);
}

const char *m2k_project_name(int slot) { return proj[slot % M2K_PROJECTS].name; }
int m2k_project_current(void) { return cur_proj; }
