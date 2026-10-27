/* SPDX-License-Identifier: GPL-3.0-only */
/* DAURADE's sounds on Felucca's engines, and how the cards set a track: shared by the firmware (da_fw.c) and the
 * host render (tests/da_felucca.c), so both play the same. Needs core.h (track_t, P_*, the song) and the matrix's
 * MS_* / MD_* (mod.c) first.
 *
 * Tracks: 1 STABS, 2 BASS, 3 LEAD, 4 DRUMS. Each music part has a list of ten sounds (the SOUND page): an engine
 * and its factory preset, or a sound of Daurade's own on that engine (das_own), or one of Tuna's FM6 patches
 * (da_fm6.inc). The drums are a kit: CRISP, Daurade's own one-shots (the SAMPLE engine, set DRUMS: tools/da_drums.py,
 * 44.1 kHz), or the DRUM engine's TIGHT or 80 kit.
 *
 * Two of the matrix's slots carry the groove's controls, set every block by the glue:
 *   slot 1  MODW -> the sound's brightness (ANALOG and TRIO CUT, FM6 MLVL, PHYS BRIT, ...) by -32: the SWEEP (t->mw is
 *           127 - the groove's cut, so an open filter leaves the sound as its preset has it, and a closed one takes
 *           half the range off: ANALOG's CUT from 104 (5 kHz) down to 40 (220 Hz))
 *   slot 2  EXPR -> AMP by 64: the PUMP (t->ex_off is the groove's duck: the gain is 1 - duck)
 *   slot 4  VEL -> AMP: the drums' velocities on the SAMPLE engine (it has none of its own) */
#ifndef DA_SOUNDS_H
#define DA_SOUNDS_H

#include "da_groove.h"

enum { DAS_ANALOG = 0, DAS_PHASE = 2, DAS_LOFI = 3, DAS_SAMPLE = 4, DAS_VOICE = 5, DAS_TRIO = 6, DAS_WHEEL = 7,
       DAS_PHYS = 9, DAS_DRUM = 10, DAS_FM6 = 12 };
enum { DAS_KEEP = -1 };                   /* dist, glide, voice: as the preset leaves them */
enum { DAS_POLY = 0, DAS_MONO = 1, DAS_LEGATO = 2 };

typedef struct {
    const char *name;
    uint8_t engine, preset;
    int8_t dist, glide, voice;
    const preset_t *own;
    const uint8_t *fm6;
    int8_t bright;                        /* the E parameter the SWEEP takes down (0..7), -1: none */
    int8_t trim;                          /* added to the level (F_DB steps), to even the sounds out: each part's
                                           * sounds at one loudness (tests/da_audition.sh measures them) */
} das_sound;

/* ---- Daurade's own sounds: Felucca's preset layout (core.h preset_t): {name, E0..E7, ATK DEC SUS REL, ENV->FLT,
 * mono, FX(dist, chorus, delay, reverb)}. The ANALOG E values: WAVE DTN MIX NOIS CUT RES DRV KTR; VOICE: VOWL VOWL2
 * TALK SHIFT BUZZ BRTH Q RAND. CLAV, E.GUITAR, SAW LEAD and UPRIGHT are Tuna's (tuna_sounds_own.inc) ---- */
/* the stab of the filtered loop: two detuned saws, a resonant low-pass with a short bite of envelope */
static const preset_t DAS_CHROME = {"CHROME", {0, 16, 64, 0, 104, 55, 30, 64}, {0, 62, 70, 18}, 24, 0, FX(10, 30, 25, 20)};
/* the robot: the VOICE engine at full buzz and a high Q, so it sings like a vocoder; the vowels and the talk are
 * the VOICE page's */
static const preset_t DAS_ROBOT = {"ROBOT", {0, 64, 40, 0, 127, 0, 110, 0}, {2, 70, 110, 30}, 0, 1, FX(0, 15, 30, 25)};
/* the octave disco bass: a saw with its sub-octave, plucked by the filter envelope */
static const preset_t DAS_DISCO = {"DISCO BASS", {6, 0, 40, 0, 62, 35, 20, 64}, {0, 52, 60, 14}, 30, 1, FX(5, 0, 5, 10)};
static const preset_t DAS_CLAV = {"CLAV", {1, 40, 120, 70, 8, 115, 0, 30}, {0, 100, 127, 25}, 0, 0, FX(10, 20, 15, 25)};
static const preset_t DAS_EGUITAR = {"E.GUITAR", {1, 36, 85, 80, 20, 100, 0, 10}, {0, 100, 127, 40}, 0, 0,
                                     FX(0, 40, 25, 30)};
static const preset_t DAS_SAWLEAD = {"SAW LEAD", {0, 0, 12, 12, 0, 96, 25, 64}, {2, 70, 100, 35}, 20, 1,
                                     FX(0, 10, 35, 25)};
static const preset_t DAS_UPRIGHT = {"UPRIGHT", {1, 40, 35, 75, 0, 127, 0, 10}, {0, 100, 127, 40}, 0, 0, FX(0, 0, 10, 35)};

#include "da_fm6.inc"

#define S(n, e, p, b, tr) {(n), (uint8_t)(e), (uint8_t)(p), DAS_KEEP, DAS_KEEP, DAS_KEEP, 0, 0, (int8_t)(b), (int8_t)(tr)}
#define SX(n, e, p, d, gl, v, b, tr) {(n), (uint8_t)(e), (uint8_t)(p), (int8_t)(d), (int8_t)(gl), (int8_t)(v), 0, 0, (int8_t)(b), (int8_t)(tr)}
#define SO(n, e, p, own, d, gl, v, b, tr) {(n), (uint8_t)(e), (uint8_t)(p), (int8_t)(d), (int8_t)(gl), (int8_t)(v), &(own), 0, (int8_t)(b), (int8_t)(tr)}
#define SF(n, patch, gl, v, tr) {(n), (uint8_t)DAS_FM6, 0, DAS_KEEP, (int8_t)(gl), (int8_t)(v), 0, (patch), 2, (int8_t)(tr)}

/* the three music parts' lists, in the SOUND card's order */
static const das_sound DAS_SOUND[3][DA_SOUNDS] = {
    {   /* STABS */
        SO("CHROME", DAS_ANALOG, 0, DAS_CHROME, DAS_KEEP, DAS_KEEP, DAS_POLY, 4, 5),
        S("EP", DAS_FM6, 0, 2, 1),                       /* TINE EP */
        SF("REED EP", DA_FM6_REEDEP, DAS_KEEP, DAS_POLY, -2),
        S("STRINGS", DAS_ANALOG, 11, 4, -6),
        S("BRASS", DAS_FM6, 3, 2, 2),
        SO("CLAV", DAS_PHYS, 2, DAS_CLAV, DAS_KEEP, DAS_KEEP, DAS_POLY, 2, 24),
        SO("GUITAR", DAS_PHYS, 2, DAS_EGUITAR, 14, DAS_KEEP, DAS_POLY, 2, 22),
        S("ORGAN", DAS_WHEEL, 2, 3, 1),                  /* GOSPEL */
        S("RAVE", DAS_ANALOG, 6, 4, -5),
        S("PAD", DAS_FM6, 4, 2, 16),
    },
    {   /* BASS */
        S("FM BASS", DAS_FM6, 2, 2, -4),
        S("SQUARE", DAS_ANALOG, 2, 4, -1),
        SX("ACID", DAS_ANALOG, 4, DAS_KEEP, 18, DAS_LEGATO, 4, 6),
        S("SUB", DAS_ANALOG, 7, 4, -13),
        SF("SLAP", DA_FM6_SLAPBASS, DAS_KEEP, DAS_POLY, 0),
        SF("FRETLESS", DA_FM6_FRETLESS, 22, DAS_LEGATO, -5),
        S("FAT", DAS_TRIO, 0, 5, 2),
        SX("WOW", DAS_VOICE, 2, DAS_KEEP, 24, DAS_LEGATO, -1, -3),
        SO("DISCO", DAS_ANALOG, 0, DAS_DISCO, DAS_KEEP, DAS_KEEP, DAS_MONO, 4, -1),
        SO("UPRIGHT", DAS_PHYS, 2, DAS_UPRIGHT, DAS_KEEP, DAS_KEEP, DAS_POLY, 2, 16),
    },
    {   /* LEAD */
        SO("ROBOT", DAS_VOICE, 1, DAS_ROBOT, DAS_KEEP, DAS_KEEP, DAS_LEGATO, -1, -10),
        S("TALKBOX", DAS_VOICE, 1, -1, -12),                /* VOX LEAD */
        SO("SAW", DAS_TRIO, 0, DAS_SAWLEAD, DAS_KEEP, 10, DAS_KEEP, 5, -8),
        S("SYNC", DAS_TRIO, 2, 5, -6),
        SO("GUITAR", DAS_PHYS, 2, DAS_EGUITAR, 100, 12, DAS_LEGATO, 2, -3),   /* through the distortion: the solo */
        S("STRINGS", DAS_ANALOG, 11, 4, -10),
        SF("BRASS", DA_FM6_SYNBRASS, DAS_KEEP, DAS_POLY, -1),
        S("CHIP", DAS_LOFI, 0, 7, -12),
        S("SINE", DAS_ANALOG, 5, 4, -3),
        S("ORGAN", DAS_WHEEL, 4, 3, -13),                   /* ROCK DRIVE */
    },
};
#undef S
#undef SX
#undef SO
#undef SF

static const char *const DAS_KIT_NAMES[DA_KIT_COUNT] = {"CRISP", "SESSION", "MACHINE"};
static const int8_t DAS_KIT_TRIM[DA_KIT_COUNT] = {-14, 0, -1};   /* (tests/da_audition.sh, as the sounds' trims) */
static inline const char *das_name(int part, int i)
{
    if (part == DA_DRUMS)
        return DAS_KIT_NAMES[(unsigned)i % DA_KIT_COUNT];
    return DAS_SOUND[part % 3][(unsigned)i % DA_SOUNDS].name;
}

/* the sound a part plays now: its card's, the LEAD's solo sound while a solo plays */
static inline const das_sound *das_of(const da_groove *g, int part)
{
    int i = g->s.snd[part];
    if (part == DA_LEAD && da_soloing(g))
        i = DA_STYLE[g->s.style].solo_snd;
    return &DAS_SOUND[part % 3][(unsigned)i % DA_SOUNDS];
}

/* ---- the drums: the kit's engine and each drum's note ---- */
#define DAS_KIT_BASE 53                   /* the DRUMS set's first key (tools/gen_samples.py KIT_BASE) */
static inline uint32_t das_kit_engine(int kit) { return kit == DA_KIT_CRISP ? DAS_SAMPLE : DAS_DRUM; }
static inline int das_drum_note(const da_settings *s, int drum)
{
    if (s->snd[3] == DA_KIT_CRISP) {      /* the DRUMS set: KICK PUNCH, KICK ROUND, SNARE, CLAP, HAT, OPEN HAT, CRASH */
        static const uint8_t K[DA_DR_COUNT] = {0, 2, 3, 4, 5, 6};
        int k = K[drum % DA_DR_COUNT];
        if (drum == DA_DR_KICK && s->kick == DA_KICK_ROUND)
            k = 1;
        return DAS_KIT_BASE + k;
    }
    static const uint8_t GM[DA_DR_COUNT] = {36, 38, 39, 42, 46, 49};   /* General MIDI */
    return GM[drum % DA_DR_COUNT];
}
/* the DRUM engine's eight (E0 KIT, E1 TUNE, E2 TONE, E3 DECY, E4 SNAP, E5 ACC, E6 KICK, E7 DRV) for a kit */
static inline void das_kit_params(const da_settings *s, int16_t *e)
{
    e[0] = s->snd[3] == DA_KIT_SESSION ? 10 : 4;    /* TIGHT, 80 (eng_drum.c DK_*) */
    e[1] = 64;
    e[2] = 64;
    e[3] = 60;
    e[4] = (int16_t)(30 + s->snap * 97 / 100);
    e[5] = 100;
    e[6] = (int16_t)(s->kick == DA_KICK_ROUND ? 1 : 0);
    e[7] = 10;
}

static inline int16_t das_clamp(int v, int lo, int hi) { return (int16_t)(v < lo ? lo : v > hi ? hi : v); }

/* the DTIME card in Felucca's delay times (params.c N_DTIME): 1/16, 1/8, 1/8D, 1/4 */
static const uint8_t DAS_DTIME[DA_DTIME_COUNT] = {2, 1, 10, 0};
static const char *const DAS_DTIME_NAMES[DA_DTIME_COUNT] = {"1/16", "1/8", "3/16", "1/4"};

/* the TALK card: the vowel glide in a note value of the tempo, as the VOICE engine's TALK time (gen_tables.py: a
 * value v is 10000^(v/127) ms) */
static inline int16_t das_talk(const da_settings *s)
{
    static const float DIV[DA_TALK_COUNT] = {0.25f, 0.5f, 1.0f, 2.0f};   /* beats */
    float ms = 60000.0f / (float)(s->bpm ? s->bpm : 120) * DIV[s->talk % DA_TALK_COUNT];
    /* v = 127 log10(ms) / 4, log10 without libm: by the powers of ten and a few steps of the series */
    int dec = 0;
    float x = ms;
    while (x >= 10.0f) { x *= 0.1f; dec++; }
    float y = (x - 1.0f) / (x + 1.0f), y2 = y * y;   /* ln(x) = 2 atanh(y) */
    float ln = 2.0f * y * (1.0f + y2 / 3.0f + y2 * y2 / 5.0f + y2 * y2 * y2 / 7.0f);
    float v = 127.0f * ((float)dec + ln * 0.4342945f) / 4.0f;
    return das_clamp((int)(v + 0.5f), 0, 127);
}
/* the VOWELS card: VOWL and VOWL2 (A 0, E 32, I 64, O 95, U 127: eng_formant.c's five) */
static const uint8_t DAS_VOWELS[DA_VOWELS_COUNT][2] = {{0, 64}, {95, 0}, {127, 64}, {64, 0}, {0, 95}};
static const char *const DAS_VOWEL_NAMES[DA_VOWELS_COUNT] = {"AH-EE", "OH-AH", "OO-EE", "EE-AH", "AH-OH"};
static const char *const DAS_TALK_NAMES[DA_TALK_COUNT] = {"1/16", "1/8", "1/4", "1/2"};

/* a part's track from the cards, over its sound's preset: the level, the place, the sends, TONE on the parts the
 * sweep filters, the VOICE page on a VOICE sound, and the matrix's slots for the groove (above). Main loop, the
 * audio held off (or the host) */
static inline void das_track(track_t *t, const da_groove *g, int part)
{
    const da_settings *s = &g->s;
    const da_style *st = &DA_STYLE[s->style];
    const das_sound *snd = part == DA_DRUMS ? 0 : das_of(g, part);
    bool swept = (st->sweep_parts >> part) & 1u;
    /* MIX: 100 is 0 dB, each step down 0.3 dB (MIX 50: -15 dB), 0 off; then the sound's trim (F_DB: half-dB steps) */
    int lvl = 112 - (100 - s->lvl[part]) * 6 / 10 + (snd ? snd->trim : DAS_KIT_TRIM[s->snd[3] % DA_KIT_COUNT]);
    t->p[P_LEVEL] = das_clamp(s->lvl[part] ? lvl : 0, 1, 127);
    /* the place: the stabs a little left and spread, the lead a little right, the bass and drums centred */
    int w = s->width;
    t->p[P_PAN] = (int16_t)(part == DA_STABS ? -w * 18 / 100 : part == DA_LEAD ? w * 18 / 100 : 0);
    t->p[P_SPRD] = das_clamp(part == DA_STABS ? w * 90 / 100 : 0, 0, 127);
    /* the sends */
    static const uint8_t REV[DA_PARTS] = {100, 35, 90, 30}, DLY[DA_PARTS] = {70, 0, 100, 0};
    t->p[P_REV] = das_clamp(s->reverb * 127 / 100 * REV[part] / 100, 0, 127);
    t->p[P_DLY] = das_clamp(s->delay * 127 / 100 * DLY[part] / 100, 0, 127);
    /* the matrix: the sweep and the pump */
    t->p[P_M1SRC] = MS_MODW;
    t->p[P_M1DST] = (int16_t)(snd && snd->bright >= 0 ? MD_E1 + snd->bright : MD_OFF);
    t->p[P_M1AMT] = -32;                         /* (closed: half the range off, ~200 Hz on ANALOG's CUT from 104) */
    t->p[P_M2SRC] = part == DA_DRUMS ? MS_OFF : MS_EXPR;
    t->p[P_M2DST] = part == DA_DRUMS ? MD_OFF : MD_AMP;
    t->p[P_M2AMT] = 64;
    if (part == DA_DRUMS) {
        t->p[P_VOICE] = V_POLY;
        if (s->snd[3] == DA_KIT_CRISP) {          /* the samples: their velocity, SNAP as a low-pass (100: open) */
            t->p[P_M4SRC] = MS_VEL;
            t->p[P_M4DST] = MD_AMP;
            t->p[P_M4AMT] = 64;
            int lpf = 60 + s->snap * 68 / 100;
            if (lpf >= 127) {
                t->p[P_ITYPE] = 0;
            } else {
                t->p[P_ITYPE] = 5;                /* IT_CRUSH at 16 bits and the full rate: its low-pass alone */
                t->p[P_IA] = 127;
                t->p[P_IB] = 127;
                t->p[P_IC] = das_clamp(lpf, 0, 127);
                t->p[P_IMIX] = 127;
            }
        } else {
            t->p[P_M4SRC] = MS_OFF;
            t->p[P_ITYPE] = 0;
            das_kit_params(s, &t->p[P_E0]);
        }
        t->p[P_DIST] = 0;
        return;
    }
    if (snd->glide >= 0)
        t->p[P_GLIDE] = snd->glide;
    if (part == DA_LEAD && da_soloing(g))         /* the solo's bends: a glide between the notes */
        t->p[P_GLIDE] = das_clamp(s->bend * 40 / 100, 0, 127);
    if (snd->voice >= 0)
        t->p[P_VOICE] = (int16_t)(snd->voice == DAS_LEGATO ? V_LEGATO : snd->voice == DAS_MONO ? V_MONO : V_POLY);
    if (snd->engine == DAS_VOICE) {              /* the VOICE page */
        t->p[P_E0] = DAS_VOWELS[s->vowels % DA_VOWELS_COUNT][0];
        t->p[P_E1] = DAS_VOWELS[s->vowels % DA_VOWELS_COUNT][1];
        t->p[P_E2] = das_talk(s);
        t->p[P_E3] = s->shift;
        if (part == DA_LEAD && s->glide)
            t->p[P_GLIDE] = das_clamp(s->glide * 60 / 100, 0, 127);
    }
    /* TONE on the swept parts: RESO, DRIVE, then one insert, CRUSH before PHASER. A swept ANALOG or TRIO sound
     * starts bright (CUT at least 100): the sweep brings the darkness, a dark preset under it would go silent */
    if (swept) {
        if ((snd->engine == DAS_ANALOG || snd->engine == DAS_TRIO) && snd->bright >= 0 && t->p[P_E0 + snd->bright] < 100)
            t->p[P_E0 + snd->bright] = 100;
        if (snd->engine == DAS_ANALOG)
            t->p[P_E5] = das_clamp(s->reso * 120 / 100, 0, 127);
        else if (snd->engine == DAS_TRIO)
            t->p[P_E6] = das_clamp(s->reso * 120 / 100, 0, 127);
        t->p[P_DIST] = das_clamp((snd->dist >= 0 ? snd->dist : 0) + s->drive * 90 / 100, 0, 127);
        if (s->crush) {
            t->p[P_ITYPE] = 5;                    /* IT_CRUSH: the bits fall as CRUSH rises, the rate with them */
            t->p[P_IA] = das_clamp(127 - s->crush * 100 / 100, 16, 127);
            t->p[P_IB] = das_clamp(127 - s->crush * 70 / 100, 0, 127);
            t->p[P_IC] = 110;
            t->p[P_IMIX] = 127;
        } else if (s->phaser) {
            t->p[P_ITYPE] = 6;                    /* IT_PHASER: slow, its depth the card */
            t->p[P_IA] = 30;
            t->p[P_IB] = das_clamp(s->phaser * 127 / 100, 0, 127);
            t->p[P_IC] = 70;
            t->p[P_IMIX] = 127;
        } else {
            t->p[P_ITYPE] = 0;
        }
    } else {
        if (snd->dist >= 0)
            t->p[P_DIST] = snd->dist;
        t->p[P_ITYPE] = 0;
    }
}

/* the song's own: the tempo for the delay and the LFOs, the delay, the reverb's room */
static inline void das_song(const da_settings *s)
{
    song.g[G_BPM] = s->bpm;
    song.g[G_DTIME] = DAS_DTIME[s->dtime % DA_DTIME_COUNT];
    song.g[G_DFDBK] = 45;
    song.g[G_DMIX] = 100;
    song.g[G_RSIZE] = (int16_t)(60 + s->reverb * 30 / 100);
    song.g[G_RDAMP] = 50;
    song.g[G_RTYPE] = 0;                          /* (Felucca's ROOM model) */
}

/* the block's controls into the tracks (the audio ISR, before the mix): the sweep and the pump */
static inline void das_block(const da_groove *g)
{
    for (int p = 0; p < DA_PARTS; ++p) {
        trk[p].mw = (uint8_t)(127 - g->cut[p]);
        trk[p].ex_off = g->duck[p];
    }
}

#endif
