/* SPDX-License-Identifier: GPL-3.0-only */
/* DAURADE's sounds side by side: each sound of each part (and each kit) plays the same phrase through Felucca's
 * engines on the host, its trim at 0, and the tool prints the loudness it reaches and the trim (da_sounds.h
 * das_sound.trim, F_DB steps of half a dB) that brings it to its part's mark. So the SOUND card can change a sound
 * without the mix jumping. With an OUTDIR it also writes each phrase as a WAV, to hear.
 *
 *   tests/da_audition.sh [OUTDIR]
 * The phrases: STABS a three-note chord on the off-beat eighths, BASS eighths on C2 and its octave, LEAD a held
 * quarter-note line round C5, DRUMS a bar of the HOUSE groove. The loudness is the RMS over the phrase, the drums'
 * the same; the marks: STABS -20, BASS -18, LEAD -19, DRUMS -13 dBFS at MIX 100. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "da_groove.h"
#include "da_sounds.h"

static da_groove G;
static const float MARK[DA_PARTS] = {-20.0f, -18.0f, -19.0f, -13.0f};

static void host_own(track_t *t, const preset_t *pr)
{
    static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
    for (int i = 0; i < 8; i++) t->p[P_E0 + i] = pr->e[i];
    t->p[P_ATK] = pr->env[0];
    t->p[P_DEC] = pr->env[1];
    t->p[P_SUS] = pr->env[2];
    t->p[P_REL] = pr->env[3];
    t->p[P_ED_FLT] = pr->fenv;
    t->p[P_VOICE] = pr->mono ? V_LEGATO : V_POLY;
    for (int i = 0; i < 4; i++) t->p[P_DIST + i] = (int16_t)(pr->fx[i] ? pr->fx[i] - 1 : FX_DEF[i]);
}
static void host_fm6(track_t *t, const uint8_t *packed)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK;
    uint8_t v[FP_SIZE + 1u];
    fm6_unpack(packed, v);
    fm6_set_patch(tr, v);
    t->p[P_E7] = FM6_OWN;
    fm6_slot[tr] = FM6_OWN;
}

/* one phrase of part p on its sound (G's settings): its RMS in dBFS */
static float audition(int p, const char *dir, const char *name)
{
    host_tracks_init();
    for (int k = 0; k < DA_PARTS; ++k)
        trk[k].p[P_LEVEL] = 0;
    track_t *t = &trk[p];
    if (p == DA_DRUMS) {
        host_preset(t, das_kit_engine(G.s.snd[3]), 0);
    } else {
        const das_sound *snd = das_of(&G, p);
        host_preset(t, snd->engine, snd->preset);
        if (snd->own) host_own(t, snd->own);
        if (snd->fm6) host_fm6(t, snd->fm6);
    }
    G.s.lvl[p] = 100;
    das_track(t, &G, p);
    t->p[P_REV] = 0;                              /* (the dry sound: the sends would measure the room) */
    t->p[P_DLY] = 0;
    int lvl_no_trim = 112;                         /* MIX 100, trim 0 */
    t->p[P_LEVEL] = (int16_t)lvl_no_trim;
    das_song(&G.s);
    FILE *w = 0;
    char path[512];
    if (dir) {
        snprintf(path, sizeof path, "%s/audition_%s.wav", dir, name);
        w = fopen(path, "wb");
        if (w) wav_hdr(w, 0);
    }
    float bpm = 120.0f, sixteenth = (float)FS * 60.0f / bpm / 4.0f;
    long frames = (long)(sixteenth * 32.0f), f = 0;
    double sum = 0.0;
    long n = 0;
    int last = -1;
    static const int LEAD[8] = {72, 74, 76, 79, 76, 74, 72, 67};
    while (f < frames + FS / 2) {
        int st = (int)((float)f / sixteenth);
        if (st != last && f < frames) {
            last = st;
            int i = st % 16;
            if (p == DA_STABS) {
                if (i % 4 == 2) {
                    trk_note_on(t, 60, 100); trk_note_on(t, 64, 100); trk_note_on(t, 67, 100);
                } else if (i % 4 == 3) {
                    trk_note_off(t, 60); trk_note_off(t, 64); trk_note_off(t, 67);
                }
            } else if (p == DA_BASS) {
                if (i % 2 == 0) trk_note_on(t, i % 4 ? 48 : 36, 100);
                else trk_note_off(t, i % 4 == 1 ? 36 : 48);
            } else if (p == DA_LEAD) {
                if (i % 4 == 0) trk_note_on(t, (uint32_t)LEAD[(st / 4) % 8], 100);
                else if (i % 4 == 3) trk_note_off(t, (uint32_t)LEAD[(st / 4) % 8]);
            } else {
                static const uint8_t KICK[16] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
                if (KICK[i]) trk_note_on(t, (uint32_t)das_drum_note(&G.s, DA_DR_KICK), 120);
                if (i == 4 || i == 12) trk_note_on(t, (uint32_t)das_drum_note(&G.s, DA_DR_CLAP), 115);
                if (i % 4 == 2) trk_note_on(t, (uint32_t)das_drum_note(&G.s, DA_DR_OPEN), 95);
                else trk_note_on(t, (uint32_t)das_drum_note(&G.s, DA_DR_HAT), i % 2 ? 40 : 70);
            }
        }
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        for (int i = 0; i < CTL; ++i) {
            double v = (o[2 * i] + o[2 * i + 1]) * 0.5 / 32768.0;
            if (f < frames) { sum += v * v; n++; }
            if (w) wav_put(w, o[2 * i], o[2 * i + 1]);
        }
        f += CTL;
    }
    if (w) { fseek(w, 0, SEEK_SET); wav_hdr(w, (uint32_t)f); fclose(w); }
    return (float)(10.0 * log10(sum / (double)(n ? n : 1) + 1e-12));
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : 0;
    static const char *const PART[DA_PARTS] = {"STABS", "BASS", "LEAD", "DRUMS"};
    da_init(&G, (float)FS);
    G.s.bpm = 120;
    G.s.reso = 30;
    G.s.drive = 0;
    G.s.phaser = 0;
    G.s.crush = 0;
    for (int p = 0; p < DA_PARTS; ++p) {
        printf("%s (mark %.0f dBFS)\n", PART[p], (double)MARK[p]);
        int n = p == DA_DRUMS ? DA_KIT_COUNT : DA_SOUNDS;
        for (int i = 0; i < n; ++i) {
            G.s.snd[p] = (uint8_t)i;
            char nm[40];
            snprintf(nm, sizeof nm, "%s_%s", PART[p], das_name(p, i));
            for (char *c = nm; *c; ++c) if (*c == ' ' || *c == '.') *c = '_';
            float db = audition(p, dir, nm);
            int trim = (int)((MARK[p] - db) * 2.0f + (MARK[p] - db >= 0 ? 0.5f : -0.5f));
            printf("  %-9s %6.1f dBFS   trim %+d\n", das_name(p, i), (double)db, trim);
        }
        G.s.snd[p] = 0;
    }
    return 0;
}
