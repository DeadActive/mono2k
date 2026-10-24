/* SPDX-License-Identifier: GPL-3.0-only */
#define GEN_NO_MAIN
#include "gen.c"
#define SA (P_SYN + 0)
#define SB (P_SYN + 1)
#define SC (P_SYN + 2)
#define SD (P_SYN + 3)
#define SE (P_SYN + 4)
#define SF (P_SYN + 5)
#define SG (P_SYN + 6)
#define SH (P_SYN + 7)
#include "mus.h"
/* Picked sounds on new notes: a small score read from a file, one command a line.
 *   bpm N | len STEPS | secs S | seed N
 *   track T CAND [p=v ..]     the candidate's kit on track T (its LFOs on T), its edits; the first track's bus
 *   mtrack T CAT ID [p=v ..]  the same for sheet 3's instruments (mus.h): CAT 0 pad 1 stab 2 keys 3 lead
 *   ptrack T M                a plain track: machine M at its init (a sub, a bed), on the first track's bus
 *   orig T CAND [p=v ..]      sheet 1's candidate with its own two bars, exactly, twice over (64 steps)
 *   notes T A B N             every trig of T from step A to B-1 on note N
 *   cond T STEP C             a trig's condition (C_*)
 *   speed T I                 a track's speed: 0 1/8x 1 1/4x 2 1/2x 3 3/4x 4 1x 5 3/2x 6 2x
 *   untrig T STEP             no trig there
 *   solo T                    only T heard (the next out)
 *   clear T                   T's trigs and locks gone (its kit kept, its motion still drawable)
 *   glob I V                  a pattern (global) parameter: G_SWNG 1 ...
 *   dump T PATH               track T's machine and kit, to a file
 *   sync                      everything on the tempo: each delay time (and its locks) on the nearest musical value
 *                             (1/16 3/32 1/8 3/16 1/4 3/8 1/2 of a note), each LFO's speed on a power of two (its
 *                             cycle a whole number of beats, or a beat cut in two, four...)
 *   kit T P V                 a kit parameter
 *   trig T STEP NOTE [a|s|as] a trig: accent, slide
 *   motion T                  each trig of T its candidate's motion, drawn afresh
 *   squeeze T P LO HI         every lock of P on T into LO..HI
 *   lock T STEP P V           a lock
 *   ens T STEP I2 I3 I4       a SWAV-ENS chord: the three intervals, in semitones
 *   trim PCT                  every track's LEVL scaled
 *   out PATH                  render to PATH
 *   cdump N NAME BPM PATH     the pattern as bank A's slot N in the factory's code (after the sync), to PATH */
static const char *const PN_[P_COUNT] = {
    "SA", "SB", "SC", "SD", "SE", "SF", "SG", "SH", "P_ATK", "P_HOLD", "P_DEC", "P_REL", "P_DIST", "P_VOL", "P_PAN", "P_PORT",
    "P_BASE", "P_WDTH", "P_HPQ", "P_LPQ", "P_FATK", "P_FDEC", "P_BOF", "P_WOF", "P_EQF", "P_EQG", "P_SRR", "P_DTIM",
    "P_DSND", "P_DFB", "P_DBAS", "P_DWID", "P_BITS", "P_FOLD", "P_XOR", "P_SPU", "P_PTCH", "P_LEVL", "P_AMOD", "P_ASPD",
    "P_ARNG", "P_GATE",
    "L1 + L_TRCK", "L1 + L_DEST", "L1 + L_WAVE", "L1 + L_TRIG", "L1 + L_MULT", "L1 + L_SPD", "L1 + L_PHAS", "L1 + L_DPTH",
    "L2 + L_TRCK", "L2 + L_DEST", "L2 + L_WAVE", "L2 + L_TRIG", "L2 + L_MULT", "L2 + L_SPD", "L2 + L_PHAS", "L2 + L_DPTH",
    "L3 + L_TRCK", "L3 + L_DEST", "L3 + L_WAVE", "L3 + L_TRIG", "L3 + L_MULT", "L3 + L_SPD", "L3 + L_PHAS", "L3 + L_DPTH",
    "P_MODT", "P_MODR", "P_MODD", "P_MODM", "P_WBNK"};
static const char *const MN_[M_COUNT] = {"M_GSIN", "M_GPRC", "M_SID", "M_FMS", "M_FMD", "M_SAW", "M_ENS", "M_WAV", "M_VOX",
    "M_FMP", "M_PUL", "M_FXC", "M_FXR", "M_FXD", "M_GNOI", "M_DKIK", "M_DSNR", "M_DHAT", "M_DCYM"};
static const char *const GN_[G_COUNT] = {"G_TMPO", "G_SWNG", "G_LEN", "G_ACNT", "G_TAPE", "G_ADPC", "G_GLCH", "G_GSIZ",
    "G_VERB", "G_DAMP", "G_HISS", "G_DRIV", "G_SPD1 + 0", "G_SPD1 + 1", "G_SPD1 + 2", "G_SPD1 + 3", "G_SPD1 + 4", "G_SPD1 + 5"};
static m2k_pattern_t ref_;
static int16_t mbuf[2 * 44100 * 64];
int main(int argc, char **argv)
{
    if (argc < 2) return 1;
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 1; }
    m2k_init(7);
    m2k_pattern_t *q = PAT(cur_pat);
    steps_clear(q, -1);
    q->mute = 0x3F;
    m_cpy(q->glob, GLOB0, G_COUNT);
    for (int t = 0; t < M2K_TRACKS; t++) q->glob[G_SPD1 + t] = SPD_1X;
    cand_t cd[M2K_TRACKS];
    int used = 0, have_bus = 0;
    double secs = 9.4;
    uint32_t seed = 1;
    int do_sync = 0;
    char ln[512];
    while (fgets(ln, sizeof ln, f)) {
        char cmd[16]; int t, a, b, c, d, n;
        if (sscanf(ln, "%15s%n", cmd, &n) != 1) continue;
        char *r = ln + n;
        if (!strcmp(cmd, "bpm") && sscanf(r, "%d", &a) == 1) q->glob[G_TMPO] = (uint8_t)(a - 30);
        else if (!strcmp(cmd, "len") && sscanf(r, "%d", &a) == 1) q->glob[G_LEN] = (uint8_t)a;
        else if (!strcmp(cmd, "secs")) secs = atof(r);
        else if (!strcmp(cmd, "seed") && sscanf(r, "%d", &a) == 1) seed = (uint32_t)a;
        else if ((!strcmp(cmd, "track") && sscanf(r, "%d %d%n", &t, &a, &n) == 2) ||
                 (!strcmp(cmd, "mtrack") && sscanf(r, "%d %d %d%n", &t, &b, &a, &n) == 3)) {
            if (cmd[0] == 'm') draw_mus(a, b, &cd[t]); else draw(a, &cd[t]);
            kit_default(q, t, cd[t].m);
            m_cpy(q->kit[t], cd[t].kit, P_COUNT);
            for (int l = 0; l < 3; l++) q->kit[t][P_LFO + 8 * l + L_TRCK] = enc(t, M2K_TRACKS);
            if (!have_bus) {
                q->glob[G_VERB] = cd[t].glob[G_VERB]; q->glob[G_DAMP] = cd[t].glob[G_DAMP];
                q->glob[G_TAPE] = cd[t].glob[G_TAPE]; q->glob[G_ADPC] = cd[t].glob[G_ADPC]; have_bus = 1;
            }
            q->mute &= (uint8_t)~(1u << t); used |= 1 << t;
            char *e = r + n; int p, v, k;
            while (sscanf(e, " %d=%d%n", &p, &v, &k) == 2) { if (p >= 0 && p < P_COUNT) q->kit[t][p] = (uint8_t)v; e += k; }
        } else if (!strcmp(cmd, "orig") && sscanf(r, "%d %d%n", &t, &a, &n) == 2) {
            draw(a, &cd[t]);
            kit_default(q, t, cd[t].m);
            m_cpy(q->kit[t], cd[t].kit, P_COUNT);
            for (int l = 0; l < 3; l++) q->kit[t][P_LFO + 8 * l + L_TRCK] = enc(t, M2K_TRACKS);
            if (!have_bus) {
                q->glob[G_VERB] = cd[t].glob[G_VERB]; q->glob[G_DAMP] = cd[t].glob[G_DAMP];
                q->glob[G_TAPE] = cd[t].glob[G_TAPE]; q->glob[G_ADPC] = cd[t].glob[G_ADPC]; have_bus = 1;
            }
            q->mute &= (uint8_t)~(1u << t); used |= 1 << t;
            int hit[32] = {0};
            lay(&cd[t], q, t, hit);
            for (int s2 = 0; s2 < 32; s2++) q->step[t][s2 + 32] = q->step[t][s2];
            for (int i = 0; i < M2K_LOCKS; i++)
                if (q->lock[i].trk == t && q->lock[i].step < 32) lock_set(t, q->lock[i].step + 32, q->lock[i].param, q->lock[i].val);
            char *e = r + n; int p, v, k;
            while (sscanf(e, " %d=%d%n", &p, &v, &k) == 2) { if (p >= 0 && p < P_COUNT) q->kit[t][p] = (uint8_t)v; e += k; }
        } else if (!strcmp(cmd, "notes") && sscanf(r, "%d %d %d %d", &t, &a, &b, &c) == 4) {
            for (int s2 = a; s2 < b && s2 < M2K_STEPS; s2++) if (q->step[t][s2].flags & ST_TRIG) q->step[t][s2].note = (uint8_t)c;
        } else if (!strcmp(cmd, "cond") && sscanf(r, "%d %d %d", &t, &a, &b) == 3) q->step[t][a].cond = (uint8_t)b;
        else if (!strcmp(cmd, "speed") && sscanf(r, "%d %d", &t, &a) == 2) q->glob[G_SPD1 + t] = enc(a, 7);
        else if (!strcmp(cmd, "untrig") && sscanf(r, "%d %d", &t, &a) == 2) {
            q->step[t][a].flags = 0; q->step[t][a].rtrg = 0;
            for (int i = 0; i < M2K_LOCKS; i++) if (q->lock[i].trk == t && q->lock[i].step == a) q->lock[i].trk = 0xFF;
        } else if (!strcmp(cmd, "glob") && sscanf(r, "%d %d", &a, &b) == 2 && a >= 0 && a < G_COUNT) {
            q->glob[a] = (uint8_t)b;
        } else if (!strcmp(cmd, "dump") && sscanf(r, "%d", &t) == 1) {
            char path[400]; sscanf(r, "%*d %399s", path);
            FILE *o = fopen(path, "w"); fprintf(o, "%d", q->machine[t]);
            for (int i = 0; i < P_COUNT; i++) fprintf(o, " %d", q->kit[t][i]);
            fprintf(o, "\n"); fclose(o);
        } else if (!strcmp(cmd, "sync")) {
            do_sync = 1;
        } else if (!strcmp(cmd, "clear") && sscanf(r, "%d", &t) == 1) {
            steps_clear(q, t);
        } else if (!strcmp(cmd, "solo") && sscanf(r, "%d", &t) == 1) {
            q->mute = (uint8_t)(0x3F & ~(1u << t));
        } else if (!strcmp(cmd, "ptrack") && sscanf(r, "%d %d", &t, &a) == 2) {
            kit_default(q, t, a); q->kit[t][P_LEVL] = 80;
            q->mute &= (uint8_t)~(1u << t); used |= 1 << t; cd[t].nmov = 0;
        } else if (!strcmp(cmd, "kit") && sscanf(r, "%d %d %d", &t, &a, &b) == 3) q->kit[t][a] = (uint8_t)b;
        else if (!strcmp(cmd, "trig") && sscanf(r, "%d %d %d", &t, &a, &b) == 3) {
            char fl[8] = ""; sscanf(r, "%*d %*d %*d %7s", fl);
            m2k_step_t *st = &q->step[t][a];
            st->flags = ST_TRIG | (strchr(fl, 'a') ? ST_ACC : 0) | (strchr(fl, 's') ? ST_SLD : 0);
            st->note = (uint8_t)b;
        } else if (!strcmp(cmd, "motion") && sscanf(r, "%d", &t) == 1) {
            rs = seed * 2654435761u + (uint32_t)t * 97u + 7u; for (int i = 0; i < 8; i++) rnd();
            for (int s = 0; s < M2K_STEPS; s++) {
                if (!(q->step[t][s].flags & ST_TRIG)) continue;
                for (int i = 0; i < cd[t].nmov; i++)
                    lock_set(t, s, cd[t].mv[i], cd[t].spr[i] == 127 ? ri(0, 127) : clampi(cd[t].base[i] + ri(-cd[t].spr[i], cd[t].spr[i])));
            }
        } else if (!strcmp(cmd, "squeeze") && sscanf(r, "%d %d %d %d", &t, &a, &b, &c) == 4) {
            for (int i = 0; i < M2K_LOCKS; i++)
                if (q->lock[i].trk == t && q->lock[i].param == a) q->lock[i].val = (uint8_t)(b + q->lock[i].val * (c - b) / 127);
        } else if (!strcmp(cmd, "lock") && sscanf(r, "%d %d %d %d", &t, &a, &b, &c) == 4) lock_set(t, a, b, c);
        else if (!strcmp(cmd, "ens") && sscanf(r, "%d %d %d %d %d", &t, &a, &b, &c, &d) == 5) {
            lock_set(t, a, P_SYN + 0, PCH(b)); lock_set(t, a, P_SYN + 1, PCH(c)); lock_set(t, a, P_SYN + 2, PCH(d));
        } else if (!strcmp(cmd, "trim") && sscanf(r, "%d", &a) == 1) {
            for (int tt = 0; tt < M2K_TRACKS; tt++) if (used >> tt & 1) q->kit[tt][P_LEVL] = (uint8_t)clampi(q->kit[tt][P_LEVL] * a / 100);
        } else if (!strcmp(cmd, "probe")) {             /* the machines drawn so far, to a file */
            char path[400]; sscanf(r, "%399s", path);
            FILE *o = fopen(path, "w"); for (int tt = 0; tt < M2K_TRACKS; tt++) fprintf(o, "%d ", q->machine[tt]); fclose(o);
        } else if (!strcmp(cmd, "out") || !strcmp(cmd, "cdump")) {
            char path[400], name[16] = ""; int slot = 0, bpm = 120;
            if (cmd[0] == 'o') sscanf(r, "%399s", path); else sscanf(r, "%d %15s %d %399s", &slot, name, &bpm, path);
            if (do_sync) {
                static const int DT[] = {16, 24, 32, 48, 64, 96, 127};
                #define SNAP_DT(v) do { int b_ = DT[0]; for (unsigned j_ = 0; j_ < 7; j_++) if (abs(DT[j_] - (v)) < abs(b_ - (v))) b_ = DT[j_]; (v) = (uint8_t)b_; } while (0)
                #define SNAP_P2(v) do { int b_ = 1; for (int p_ = 1; p_ <= 64; p_ <<= 1) if (abs(p_ - (v)) < abs(b_ - (v))) b_ = p_; (v) = (uint8_t)b_; } while (0)
                for (int tt = 0; tt < M2K_TRACKS; tt++) {
                    if (!(used >> tt & 1)) continue;
                    SNAP_DT(q->kit[tt][P_DTIM]);
                    for (int l = 0; l < 3; l++) if (q->kit[tt][P_LFO + 8 * l + L_DEST]) SNAP_P2(q->kit[tt][P_LFO + 8 * l + L_SPD]);
                }
                for (int i = 0; i < M2K_LOCKS; i++) {
                    if (q->lock[i].trk >= M2K_TRACKS) continue;
                    if (q->lock[i].param == P_DTIM) SNAP_DT(q->lock[i].val);
                    for (int l = 0; l < 3; l++) if (q->lock[i].param == P_LFO + 8 * l + L_SPD) SNAP_P2(q->lock[i].val);
                }
            }
            if (cmd[0] == 'c') {
                /* the factory's code: part() as bank A has it, then what differs from it */
                pat_init(&ref_, name);
                for (int tt = 0; tt < M2K_TRACKS; tt++) kit_default(&ref_, tt, q->machine[tt]);
                ref_.glob[G_TMPO] = (uint8_t)(bpm - 30); ref_.glob[G_LEN] = 64; ref_.glob[G_VERB] = 50; ref_.glob[G_DRIV] = BANK_DRIVE;
                FILE *o = fopen(path, "w");
                fprintf(o, "    p = part(%d, \"%s\", %d, MACHINES(", slot, name, bpm);
                for (int tt = 0; tt < M2K_TRACKS; tt++) fprintf(o, "%s%s", MN_[q->machine[tt]], tt < 5 ? ", " : "));\n");
                for (int tt = 0; tt < M2K_TRACKS; tt++) {
                    int col = fprintf(o, "    SET(p, %d", tt), any = 0;
                    for (int i = 0; i < P_COUNT; i++) {
                        if (q->kit[tt][i] == ref_.kit[tt][i]) continue;
                        char item[48]; int n2 = snprintf(item, sizeof item, ", %s, %d", PN_[i], q->kit[tt][i]);
                        if (col + n2 > 118) { fprintf(o, ",\n       "); col = 7; n2 = snprintf(item, sizeof item, " %s, %d", PN_[i], q->kit[tt][i]); }
                        fputs(item, o); col += n2; any = 1;
                    }
                    fprintf(o, any ? ");\n" : ");  /* its init */\n");
                }
                for (int i = 0; i < G_COUNT; i++)
                    if (q->glob[i] != ref_.glob[i]) fprintf(o, "    p->glob[%s] = %d;\n", GN_[i], q->glob[i]);
                if (q->mute) fprintf(o, "    p->mute = %d;\n", q->mute);
                fprintf(o, "    static const uint8_t TRIGS[] = {   /* track, step, note, flags, condition, retrigs */\n");
                for (int tt = 0; tt < M2K_TRACKS; tt++) {
                    int col = fprintf(o, "       ");
                    for (int st = 0; st < M2K_STEPS; st++) {
                        const m2k_step_t *x = &q->step[tt][st];
                        if (!(x->flags & ST_TRIG)) continue;
                        char item[40]; int n2 = snprintf(item, sizeof item, " %d,%d,%d,%d,%d,%d,", tt, st, x->note, x->flags, x->cond, x->rtrg);
                        if (col + n2 > 118) { fprintf(o, "\n       "); col = 7; }
                        fputs(item, o); col += n2;
                    }
                    fprintf(o, "\n");
                }
                fprintf(o, "    };\n    trigs(p, TRIGS, sizeof TRIGS);\n");
                int nl = 0; for (int i = 0; i < M2K_LOCKS; i++) nl += q->lock[i].trk < M2K_TRACKS;
                if (nl) {
                    fprintf(o, "    static const uint8_t LOCKS[] = {   /* track, step, parameter, value */\n");
                    int col = fprintf(o, "       ");
                    for (int tt = 0; tt < M2K_TRACKS; tt++)
                        for (int i = 0; i < M2K_LOCKS; i++) {
                            if (q->lock[i].trk != tt) continue;
                            char item[40]; int n2 = snprintf(item, sizeof item, " %d,%d,%d,%d,", tt, q->lock[i].step, q->lock[i].param, q->lock[i].val);
                            if (col + n2 > 118) { fprintf(o, "\n       "); col = 7; }
                            fputs(item, o); col += n2;
                        }
                    fprintf(o, "\n    };\n    locks(p, LOCKS, sizeof LOCKS);\n");
                }
                fclose(o);
                continue;
            }
            lockmap_build();
            for (int tt = 0; tt < M2K_TRACKS; tt++) m_cpy(trk[tt].eff, q->kit[tt], P_COUNT);
            int nf = (int)(secs * 44100) & ~127;
            if (nf > 44100 * 64) nf = 44100 * 64;
            seq_play(1);
            for (int fr = 0; fr < nf; fr += 128) m2k_render(mbuf + 2 * fr, 128);
            seq_play(0);
            wav_write(path, mbuf, nf);
        }
    }
    fclose(f);
    return 0;
}
