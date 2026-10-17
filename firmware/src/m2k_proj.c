/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K's projects, the modern Elektron way: every pattern carries its own sounds. A project is 4 banks (A-D)
 * of 16 patterns. The working patterns live unpacked in RAM (pat[]); the saved projects are packed images: each
 * pattern as a blob of its sounds, its trigs only and its locks only, so a typical one is 0.5-1 KB.
 *   SAVE          the current pattern's reload point (into the open project's image)
 *   GLO + SAVE    reload: the current pattern back to that point
 *   hold SAVE     the whole project
 * The images live in RAM on the computer (host, browser); on the FM-1 they will be its flash, through the same
 * get / put. */

#define IMG_DATA (160u * 1024u)
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

/* a pattern in the layout of a version (9, today's; the self-test writes the older ones, 3..8; 3..7 kits 70 wide) */
static int pat_pack_v(const m2k_pattern_t *p, uint8_t *o, int ver)
{
    int n = 0, stride = ver >= 8 ? P_COUNT : P_WBNK;
    o[n++] = 'P';
    o[n++] = (uint8_t)ver;             /* 2: amp envelope on the Monomachine's times; 3: MOD; 4: SWAV-SAW's new page;
                                        * 5: the track chain on the Monomachine's; 6: GND-SIN's TUNE in 3.125 c;
                                        * 7: SWAV-ENS's page; 8: the wave bank's choice (kits 71 wide); 9: the bus's
                                        * controls on their new laws */
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

static int pat_pack(const m2k_pattern_t *p, uint8_t *o) { return pat_pack_v(p, o, 9); }

static uint8_t tune_v5(int v)
{
    v = 64 + (v - 64) * 32;
    return (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
}

static int pat_unpack(m2k_pattern_t *p, const uint8_t *o, int len)
{
    int n = 0;
    if (len < 2 || o[0] != 'P' || o[1] < 1 || o[1] > 9) return 0;
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

/* all 64 working patterns into an image; 0 when it does not fit (the image is then left as it was) */
static int img_pack_all(m2k_img_t *dst, const char *name)
{
    img_clear(&work_img, name);
    for (int i = 0; i < M2K_PATTERNS; i++) {
        int n = pat_pack(&pat[i], blob);
        if (!img_put(&work_img, i, blob, n)) return 0;
    }
    m_cpy(dst, &work_img, (uint32_t)(sizeof *dst - IMG_DATA + work_img.used));
    return 1;
}

static void pattern_init_blank(m2k_pattern_t *p)
{
    pat_init(p, "INIT");
    static const uint8_t M[M2K_TRACKS] = {M_GSIN, M_GNOI, M_GNOI, M_SID, M_FMS, M_ENS};
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
    for (int i = 16; i < M2K_PATTERNS; i++) pattern_init_blank(&pat[i]);
    img_pack_all(&proj[0], "DEMO");
    /* the blank projects: one blank pattern, 64 times */
    static m2k_pattern_t blank POOL;
    pattern_init_blank(&blank);
    int n = pat_pack(&blank, blob);
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

#define STORE_MAX (16 + S_COUNT + (M2K_PROJECTS + 1) * (12 + 2 * M2K_PATTERNS + IMG_DATA))
static uint8_t store_buf[STORE_MAX] POOL;

static int ser_img(uint8_t *o, const m2k_img_t *im)
{
    int n = 0;
    m_cpy(o + n, im->name, 8); n += 8;
    for (int k = 0; k < 4; k++) o[n++] = (uint8_t)(im->used >> (8 * k));
    for (int i = 0; i < M2K_PATTERNS; i++) { o[n++] = (uint8_t)im->len[i]; o[n++] = (uint8_t)(im->len[i] >> 8); }
    m_cpy(o + n, im->data, im->used);
    return n + (int)im->used;
}

static int deser_img(m2k_img_t *im, const uint8_t *o, int len)
{
    if (len < 12 + 2 * M2K_PATTERNS) return -1;
    m2k_img_t *t = &work_img;
    int n = 0;
    m_cpy(t->name, o, 8); n += 8;
    t->name[7] = 0;
    t->used = (uint32_t)o[n] | ((uint32_t)o[n + 1] << 8) | ((uint32_t)o[n + 2] << 16) | ((uint32_t)o[n + 3] << 24);
    n += 4;
    uint32_t sum = 0;
    for (int i = 0; i < M2K_PATTERNS; i++) { t->len[i] = (uint16_t)(o[n] | (o[n + 1] << 8)); sum += t->len[i]; n += 2; }
    if (sum != t->used || t->used > IMG_DATA || n + (int)t->used > len) return -1;
    m_cpy(t->data, o + n, t->used);
    m_cpy(im, t, (uint32_t)(sizeof *im - IMG_DATA + t->used));
    return n + (int)t->used;
}

uint8_t *m2k_store_buf(int *max)
{
    *max = STORE_MAX;
    return store_buf;
}

/* the factory's revision: a store saved before it carries the demos it replaced. 1: the bank rewritten on the
 * Monomachine's chain (version 5) */
#define FACTORY_REV 1
static const char *const OLD_DEMO[16] = {"LATEX", "PVC", "SPU RM", "ELECTRO", "GLITCH", "CHROME", "ACID", "2STEP", "GROOVE",
                                         "HYPRPOP", "DUB", "CHIP", "HARDGRV", "IDM", "DUBSTEP", "INIT"};

static int name_is(const char *a, const char *b)
{
    for (int i = 0; i < 8; i++) { if (a[i] != b[i]) return 0; if (!a[i]) return 1; }
    return 1;
}

/* a saved image of the DEMO project: a bank-A slot still holding the demo it shipped with (by its name; A16 only
 * while it is the blank INIT) takes today's demo, built in pat[] by the caller. Anything else is the owner's: kept */
static void refresh_demos(m2k_img_t *im)
{
    static m2k_pattern_t old POOL;
    for (int i = 0; i < 16; i++) {
        if (!im->len[i] || !pat_unpack(&old, im->data + img_off(im, i), im->len[i]) || !name_is(old.name, OLD_DEMO[i])) continue;
        if (i == 15) {
            int trigs = 0;
            for (int t = 0; t < M2K_TRACKS; t++) for (int s = 0; s < M2K_STEPS; s++) trigs += old.step[t][s].flags & ST_TRIG;
            if (trigs) continue;
        }
        int n = pat_pack(&pat[i], blob);
        img_put(im, i, blob, n);
    }
}

int m2k_store_pack(void)
{
    int n = 0;
    store_buf[n++] = 'M'; store_buf[n++] = '2'; store_buf[n++] = 'K'; store_buf[n++] = '2';
    store_buf[n++] = 2;                /* 2: the factory's revision follows the open pattern */
    store_buf[n++] = (uint8_t)cur_proj;
    store_buf[n++] = (uint8_t)cur_pat;
    store_buf[n++] = FACTORY_REV;
    m_cpy(store_buf + n, store_.set, S_COUNT); n += S_COUNT;
    static m2k_img_t w POOL;
    if (!img_pack_all(&w, "WORK")) return 0;
    n += ser_img(store_buf + n, &w);
    for (int k = 0; k < M2K_PROJECTS; k++) n += ser_img(store_buf + n, &proj[k]);
    return n;
}

int m2k_store_unpack(int len)
{
    const uint8_t *o = store_buf;
    if (len < 8 + S_COUNT || o[0] != 'M' || o[1] != '2' || o[2] != 'K' || o[3] != '2' || o[4] < 1 || o[4] > 2) return 0;
    int n = o[4] == 2 ? 8 : 7, rev = o[4] == 2 ? o[7] : 0;
    uint8_t set[S_COUNT];
    m_cpy(set, o + n, S_COUNT); n += S_COUNT;
    static m2k_img_t w POOL;
    int r = deser_img(&w, o + n, len - n);
    if (r < 0) return 0;
    n += r;
    for (int k = 0; k < M2K_PROJECTS; k++) {
        r = deser_img(&proj[k], o + n, len - n);
        if (r < 0) { projects_factory(); return 0; }
        n += r;
    }
    if (rev < FACTORY_REV && name_is(proj[0].name, "DEMO")) {      /* today's demos, where the old ones still are */
        factory();
        refresh_demos(&proj[0]);
        if (o[5] == 0) refresh_demos(&w);
    }
    img_unpack_all(&w);
    m_cpy(store_.set, set, S_COUNT);
    cur_proj = o[5] < M2K_PROJECTS ? o[5] : 0;
    cur_pat = o[6] < M2K_PATTERNS ? o[6] : 0;
    sq.queued = (uint8_t)cur_pat;
    work_reset_current();
    return 1;
}

/* a project as a file: its saved image */
int m2k_proj_export(int slot)
{
    if (slot < 0 || slot >= M2K_PROJECTS) return 0;
    int n = 0;
    store_buf[n++] = 'M'; store_buf[n++] = '2'; store_buf[n++] = 'K'; store_buf[n++] = 'P';
    store_buf[n++] = 1;
    n += ser_img(store_buf + n, &proj[slot]);
    return n;
}

int m2k_proj_import(int slot, int len)
{
    const uint8_t *o = store_buf;
    if (slot < 0 || slot >= M2K_PROJECTS || len < 5 || o[0] != 'M' || o[1] != '2' || o[2] != 'K' || o[3] != 'P' || o[4] != 1) return 0;
    if (deser_img(&proj[slot], o + 5, len - 5) < 0) return 0;
    return 1;
}

void m2k_project(int slot, int save)
{
    if (slot < 0 || slot >= M2K_PROJECTS) return;
    if (save) project_save(slot);
    else project_load(slot);
}

const char *m2k_project_name(int slot) { return proj[slot % M2K_PROJECTS].name; }
int m2k_project_current(void) { return cur_proj; }
