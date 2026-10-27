/* SPDX-License-Identifier: GPL-3.0-only */
/* DAURADE groove engine (see da_groove.h). The styles, the chord loops, the parts' rhythm families, the hook, the
 * arc; in time, sixteenth by sixteenth, with swing and a human feel; the sweep and the pump every block. */
#include "da_groove.h"

#include "tuna_sys.h"   /* memcpy and memset: the FM-1 has no C library (TUNA_FREESTANDING) */

/* ---- small helpers ---------------------------------------------------------------------------------------- */
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static uint32_t xs(uint32_t *s)
{
    uint32_t x = *s ? *s : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}
static int rnd_n(uint32_t *s, int n) { return n > 0 ? (int)(xs(s) % (uint32_t)n) : 0; }
static float rnd_f(uint32_t *s) { return (float)(xs(s) >> 8) * (1.0f / 16777216.0f); }   /* 0..1 */

static void cpy(char *d, const char *s, int n)
{
    int i = 0;
    for (; s[i] && i < n - 1; ++i)
        d[i] = s[i];
    d[i] = 0;
}

/* ---- names ------------------------------------------------------------------------------------------------ */
static const char *const NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
const char *da_note_name(int pc) { return NOTE_NAMES[((pc % 12) + 12) % 12]; }
static const char *const SCALE_NAMES[DA_SCALES] = {"DOR", "MIN", "MAJ", "MIX", "LYD", "HARM"};
const char *da_scale_name(int i) { return SCALE_NAMES[clampi(i, 0, DA_SCALES - 1)]; }
static const char *const ARC_NAMES[DA_ARC_COUNT] = {"INTRO", "MAIN", "BUILD", "DROP", "SOLO", "OUT"};
const char *da_arc_name(int p) { return ARC_NAMES[clampi(p, 0, DA_ARC_COUNT - 1)]; }

/* the scales, as semitones from the key */
static const uint8_t SCALE[DA_SCALES][7] = {
    {0, 2, 3, 5, 7, 9, 10},   /* DORIAN */
    {0, 2, 3, 5, 7, 8, 10},   /* MINOR (aeolian) */
    {0, 2, 4, 5, 7, 9, 11},   /* MAJOR */
    {0, 2, 4, 5, 7, 9, 10},   /* MIXOLYDIAN */
    {0, 2, 4, 6, 7, 9, 11},   /* LYDIAN */
    {0, 2, 3, 5, 7, 8, 11},   /* HARMONIC MINOR */
};
/* scale step k (any integer, 7 an octave up) of scale sc from MIDI note base (the key's C-relative tonic) */
static int step_note(int sc, int base, int k)
{
    int o = k >= 0 ? k / 7 : -((-k + 6) / 7);
    int d = k - o * 7;
    return base + o * 12 + SCALE[sc][d];
}

int da_snap(const da_settings *s, int note)
{
    int sc = clampi(s->scale, 0, DA_SCALES - 1), pc = ((note - s->key) % 12 + 12) % 12, best = 0, bd = 99;
    for (int i = 0; i < 7; ++i) {
        int d = SCALE[sc][i] - pc;                /* ties go down: the step below first */
        int ad = d < 0 ? -d : d;
        if (ad < bd || (ad == bd && d < 0)) {
            bd = ad;
            best = d;
        }
    }
    return note + best;
}

/* ---- the chord loops: scale degrees (0 = the key's chord), a flavour (3 triads, 7 sevenths, 9 the minor and
 * major sevenths with their ninth) and whether the fifth degree takes a major third (a dominant in minor) ---- */
typedef struct { uint8_t n, deg[4], flavour, dom; } da_loopdef;
static const da_loopdef DA_LOOP[DA_LOOPS] = {
    {4, {0, 1, 4, 3}, 9, 0},   /* 1  i9 ii7 v7 IVmaj7 (in Dorian): the filtered disco loop */
    {2, {0, 3, 0, 3}, 7, 0},   /* 2  i7 IV7: the two-chord vamp */
    {4, {5, 3, 0, 4}, 7, 0},   /* 3  vi IV I V */
    {4, {0, 5, 2, 6}, 7, 0},   /* 4  i VI III VII */
    {4, {1, 4, 0, 5}, 9, 0},   /* 5  ii7 V7 Imaj7 vi7: the jazz-disco turn */
    {4, {0, 2, 5, 3}, 9, 0},   /* 6  Imaj7 iii7 vi7 IVmaj7 */
    {4, {0, 4, 3, 0}, 7, 1},   /* 7  i V iv i */
    {4, {3, 4, 2, 5}, 9, 0},   /* 8  IVmaj7 V7 iii7 vi7 */
    {4, {0, 6, 5, 4}, 3, 1},   /* 9  i VII VI V: the descent */
    {4, {0, 4, 5, 3}, 3, 0},   /* 10 I V vi IV */
    {2, {0, 3, 0, 3}, 9, 0},   /* 11 i9 IV9: the funk vamp */
    {4, {0, 5, 6, 0}, 7, 0},   /* 12 i VI VII i */
};
void da_loop_name(const da_settings *s, int loop, char *out, int max)
{
    (void)s;
    char b[12] = "LOOP ";
    int v = clampi(loop, 0, DA_LOOPS - 1) + 1, k = 5;
    if (v >= 10) b[k++] = (char)('0' + v / 10);
    b[k++] = (char)('0' + v % 10);
    b[k] = 0;
    cpy(out, b, max);
}

/* ---- the styles ------------------------------------------------------------------------------------------- */
/* the rhythm families */
enum { STAB_LOOP, STAB_COMP, STAB_PAD, STAB_SCRATCH, STAB_CLAV, STAB_HITS, STAB_BALLAD };
enum { BASS_HOUSE, BASS_OCTAVE, BASS_WALK, BASS_SEQ, BASS_SLAP, BASS_SUB, BASS_BALLAD, BASS_DRIVE, BASS_SLOWJAM };
enum { LEAD_ROBOT, LEAD_RIFF, LEAD_SWELL, LEAD_HITS, LEAD_ARP };
enum { DRUMS_HOUSE, DRUMS_DISCO, DRUMS_FUNK, DRUMS_BALLAD, DRUMS_DRIVE, DRUMS_SLOWJAM };
#define P(x) (1u << (x))

/* name side bpm key scale | sweep pump voice energy | snd[4] solo | loops | bars swing human hats fills |
 * kick snare | stab bass lead drums | sweep parts | reverb delay dtime | reso drive phaser.
 * Sounds (da_sounds.h): STABS 0 CHROME 1 EP 2 REED EP 3 STRINGS 4 BRASS 5 CLAV 6 GUITAR 7 ORGAN 8 RAVE 9 PAD;
 * BASS 0 FM 1 SQUARE 2 ACID 3 SUB 4 SLAP 5 FRETLESS 6 FAT 7 WOW 8 DISCO 9 UPRIGHT; LEAD 0 ROBOT 1 TALKBOX 2 SAW
 * 3 SYNC 4 GUITAR 5 STRINGS 6 BRASS 7 CHIP 8 SINE 9 SYN BRASS; the kit 0 CRISP 1 SESSION 2 MACHINE */
const da_style DA_STYLE[DA_STYLES] = {
    {"FILTER HOUSE", DA_SIDE_CHROME, 124, 6, DA_SC_DORIAN, 62, 70, 40, 75, {0, 8, 0, 0}, 4, {0, 1, 10, 6}, 2,
     0, 0, DA_HATS_OFFBEAT, DA_FILLS_8, DA_KICK_PUNCH, DA_SNARE_CLAP, STAB_LOOP, BASS_HOUSE, LEAD_ROBOT, DRUMS_HOUSE,
     P(DA_STABS), 20, 30, DA_DTIME_3_16, 60, 35, 20},
    {"ROBOT POP", DA_SIDE_CHROME, 112, 7, DA_SC_MAJOR, 75, 45, 85, 60, {1, 0, 0, 0}, 4, {5, 9, 4, 7}, 2,
     8, 10, DA_HATS_8THS, DA_FILLS_8, DA_KICK_PUNCH, DA_SNARE_CLAP, STAB_COMP, BASS_OCTAVE, LEAD_ROBOT, DRUMS_HOUSE,
     P(DA_STABS), 25, 25, DA_DTIME_8, 30, 20, 0},
    {"SHRED", DA_SIDE_CHROME, 123, 4, DA_SC_MINOR, 70, 55, 30, 90, {9, 1, 4, 0}, 4, {3, 8, 6, 3}, 1,
     0, 0, DA_HATS_16THS, DA_FILLS_4, DA_KICK_PUNCH, DA_SNARE_BOTH, STAB_HITS, BASS_OCTAVE, LEAD_RIFF, DRUMS_DRIVE,
     P(DA_STABS), 15, 20, DA_DTIME_8, 40, 60, 0},
    {"NIGHT DRIVE", DA_SIDE_CHROME, 105, 2, DA_SC_MINOR, 55, 50, 35, 55, {3, 3, 3, 0}, 2, {6, 3, 8, 11}, 2,
     0, 0, DA_HATS_16THS, DA_FILLS_8, DA_KICK_ROUND, DA_SNARE_SNARE, STAB_PAD, BASS_DRIVE, LEAD_ARP, DRUMS_DRIVE,
     P(DA_STABS), 35, 35, DA_DTIME_3_16, 50, 25, 30},
    {"SLOW JAM", DA_SIDE_CHROME, 98, 8, DA_SC_MAJOR, 50, 25, 55, 40, {1, 5, 0, 0}, 0, {4, 7, 5, 4}, 2,
     20, 35, DA_HATS_8THS, DA_FILLS_8, DA_KICK_ROUND, DA_SNARE_CLAP, STAB_COMP, BASS_SLOWJAM, LEAD_ROBOT, DRUMS_SLOWJAM,
     P(DA_STABS), 35, 30, DA_DTIME_3_16, 20, 10, 40},
    {"DISCO", DA_SIDE_SESSION, 116, 9, DA_SC_DORIAN, 85, 35, 30, 75, {6, 8, 5, 0}, 9, {7, 4, 9, 2}, 1,
     10, 30, DA_HATS_OFFBEAT, DA_FILLS_4, DA_KICK_PUNCH, DA_SNARE_SNARE, STAB_SCRATCH, BASS_OCTAVE, LEAD_SWELL,
     DRUMS_DISCO, P(DA_STABS), 25, 15, DA_DTIME_8, 20, 15, 25},
    {"SEQUENCER", DA_SIDE_SESSION, 112, 2, DA_SC_MINOR, 45, 40, 30, 65, {3, 2, 2, 0}, 3, {6, 1, 11, 3}, 4,
     0, 10, DA_HATS_16THS, DA_FILLS_8, DA_KICK_ROUND, DA_SNARE_SNARE, STAB_PAD, BASS_SEQ, LEAD_SWELL, DRUMS_DRIVE,
     P(DA_STABS) | P(DA_BASS), 30, 35, DA_DTIME_3_16, 70, 20, 0},
    {"FUNK", DA_SIDE_SESSION, 108, 4, DA_SC_DORIAN, 70, 30, 50, 70, {5, 4, 1, 0}, 4, {10, 1, 10, 4}, 2,
     15, 30, DA_HATS_16THS, DA_FILLS_4, DA_KICK_PUNCH, DA_SNARE_SNARE, STAB_CLAV, BASS_SLAP, LEAD_HITS, DRUMS_FUNK,
     P(DA_STABS), 15, 15, DA_DTIME_8, 30, 25, 20},
    {"BALLAD", DA_SIDE_SESSION, 88, 5, DA_SC_MAJOR, 90, 0, 50, 35, {2, 5, 5, 0}, 9, {5, 7, 9, 4}, 2,
     0, 40, DA_HATS_8THS, DA_FILLS_OFF, DA_KICK_ROUND, DA_SNARE_SNARE, STAB_BALLAD, BASS_BALLAD, LEAD_SWELL,
     DRUMS_BALLAD, P(DA_STABS), 45, 25, DA_DTIME_4, 10, 0, 0},
    {"FINALE", DA_SIDE_SESSION, 120, 2, DA_SC_MINOR, 80, 30, 45, 85, {3, 0, 6, 0}, 4, {8, 3, 11, 6}, 1,
     0, 15, DA_HATS_16THS, DA_FILLS_4, DA_KICK_PUNCH, DA_SNARE_BOTH, STAB_HITS, BASS_OCTAVE, LEAD_HITS, DRUMS_DISCO,
     P(DA_STABS), 40, 20, DA_DTIME_8, 20, 20, 0},
};
const char *da_style_name(int i) { return DA_STYLE[clampi(i, 0, DA_STYLES - 1)].name; }

int da_bars_per_chord(const da_settings *s) { return s->bars >= 4 ? 4 : s->bars >= 2 ? 2 : 1; }

/* ---- settings --------------------------------------------------------------------------------------------- */
void da_style_apply(da_settings *s, int style)
{
    const da_style *st = &DA_STYLE[clampi(style, 0, DA_STYLES - 1)];
    s->style = (uint8_t)clampi(style, 0, DA_STYLES - 1);
    s->sweep = st->sweep;
    s->pump = st->pump;
    s->voice = st->voice;
    s->energy = st->energy;
    for (int i = 0; i < 4; ++i)
        s->snd[i] = st->snd[i];
    s->reso = st->reso;
    s->drive = st->drive;
    s->phaser = st->phaser;
    s->crush = 0;
    s->vowels = DA_VOWELS_AH_EE;
    s->talk = DA_TALK_8;
    s->glide = 25;
    s->shift = 0;
    s->key = st->key;
    s->scale = st->scale;
    s->chords = st->loops[0];
    s->bars = st->bars;
    s->swing = st->swing;
    s->human = st->human;
    s->hats = st->hats;
    s->fills = st->fills;
    s->kick = st->kick;
    s->snare = st->snare;
    s->snap = 80;
    s->reverb = st->reverb;
    s->delay = st->delay;
    s->dtime = st->dtime;
    s->width = 70;
    s->bpm = st->bpm;
    s->song = 0;
}

void da_defaults(da_settings *s)
{
    memset(s, 0, sizeof *s);
    da_style_apply(s, DA_ST_FILTER_HOUSE);
    s->lvl[DA_STABS] = 85;                         /* (the sounds are evened out: da_sounds.h trims) */
    s->lvl[DA_BASS] = 95;
    s->lvl[DA_LEAD] = 85;
    s->lvl[DA_DRUMS] = 85;
    s->arc = 1;
    s->build = 8;
    s->drop = 1;
    s->solo = 1;
    s->speed = DA_SPEED_16;
    s->bend = 40;
    s->range = 2;
    s->solo_bars = 4;
    s->palette = 10;                               /* NEBULA (ui_palettes.h UI_NEBULA_INDEX) */
    s->look = 0;
}

void da_settings_fix(da_settings *s)
{
    s->style = (uint8_t)clampi(s->style, 0, DA_STYLES - 1);
    s->sweep = (uint8_t)clampi(s->sweep, 0, 100);
    s->pump = (uint8_t)clampi(s->pump, 0, 100);
    s->voice = (uint8_t)clampi(s->voice, 0, 100);
    s->energy = (uint8_t)clampi(s->energy, 0, 100);
    for (int i = 0; i < 3; ++i)
        s->snd[i] = (uint8_t)clampi(s->snd[i], 0, DA_SOUNDS - 1);
    s->snd[3] = (uint8_t)clampi(s->snd[3], 0, DA_KIT_COUNT - 1);
    s->reso = (uint8_t)clampi(s->reso, 0, 100);
    s->drive = (uint8_t)clampi(s->drive, 0, 100);
    s->phaser = (uint8_t)clampi(s->phaser, 0, 100);
    s->crush = (uint8_t)clampi(s->crush, 0, 100);
    s->vowels = (uint8_t)clampi(s->vowels, 0, DA_VOWELS_COUNT - 1);
    s->talk = (uint8_t)clampi(s->talk, 0, DA_TALK_COUNT - 1);
    s->glide = (uint8_t)clampi(s->glide, 0, 100);
    s->shift = (int8_t)clampi(s->shift, -12, 12);
    s->key = (uint8_t)clampi(s->key, 0, 11);
    s->scale = (uint8_t)clampi(s->scale, 0, DA_SCALES - 1);
    s->chords = (uint8_t)clampi(s->chords, 0, DA_LOOPS - 1);
    s->bars = (uint8_t)(s->bars >= 4 ? 4 : s->bars >= 2 ? 2 : 1);
    s->swing = (uint8_t)clampi(s->swing, 0, 60);
    s->human = (uint8_t)clampi(s->human, 0, 100);
    s->hats = (uint8_t)clampi(s->hats, 0, DA_HATS_COUNT - 1);
    s->fills = (uint8_t)clampi(s->fills, 0, DA_FILLS_COUNT - 1);
    s->kick = (uint8_t)clampi(s->kick, 0, DA_KICK_COUNT - 1);
    s->snare = (uint8_t)clampi(s->snare, 0, DA_SNARE_COUNT - 1);
    s->snap = (uint8_t)clampi(s->snap, 0, 100);
    for (int i = 0; i < 4; ++i)
        s->lvl[i] = (uint8_t)clampi(s->lvl[i], 0, 100);
    s->reverb = (uint8_t)clampi(s->reverb, 0, 100);
    s->delay = (uint8_t)clampi(s->delay, 0, 100);
    s->dtime = (uint8_t)clampi(s->dtime, 0, DA_DTIME_COUNT - 1);
    s->width = (uint8_t)clampi(s->width, 0, 100);
    s->arc = (uint8_t)(s->arc ? 1 : 0);
    s->build = (uint8_t)(s->build >= 16 ? 16 : s->build >= 8 ? 8 : 4);
    s->drop = (uint8_t)(s->drop ? 1 : 0);
    s->solo = (uint8_t)(s->solo ? 1 : 0);
    s->speed = (uint8_t)clampi(s->speed, 0, DA_SPEED_COUNT - 1);
    s->bend = (uint8_t)clampi(s->bend, 0, 100);
    s->range = (uint8_t)clampi(s->range, 1, 3);
    s->solo_bars = (uint8_t)(s->solo_bars >= 8 ? 8 : s->solo_bars >= 4 ? 4 : 2);
    s->bpm = (uint8_t)clampi(s->bpm ? s->bpm : 124, 60, 200);
    s->look = (uint8_t)(s->look ? 1 : 0);
    if (s->song > 999)
        s->song = 0;
}

/* ---- the song: the chords, the patterns, the hook --------------------------------------------------------- */
static void chord_name(da_chord *c, int third, int fifth, int seventh, bool ninth)
{
    const char *q;
    if (third == 3 && fifth == 6)
        q = seventh == 9 ? "DIM7" : seventh ? "M7B5" : "DIM";
    else if (third == 3)
        q = !seventh ? "MIN" : ninth ? "MIN9" : "MIN7";
    else if (fifth == 8)
        q = "AUG";
    else if (seventh == 10)
        q = ninth ? "9" : "7";
    else if (seventh == 11)
        q = ninth ? "MAJ9" : "MAJ7";
    else
        q = "";
    char b[10];
    cpy(b, NOTE_NAMES[c->root % 12], sizeof b);
    int k = 0;
    while (b[k]) k++;
    for (int i = 0; q[i] && k < 7; ++i)
        b[k++] = q[i];
    b[k] = 0;
    cpy(c->name, b, sizeof c->name);
}

/* the chord on scale degree deg: its tones and a voicing near prev_center (MIDI), the voice leading of a player's
 * hand: of the chord's inversions, the one whose middle is closest */
static void build_chord(da_chord *c, const da_settings *s, int deg, int flavour, bool dom, int *center)
{
    int sc = clampi(s->scale, 0, DA_SCALES - 1);
    int r = SCALE[sc][deg % 7];
    int iv[5];
    for (int k = 0; k < 5; ++k) {                  /* root, third, fifth, seventh, ninth: stacked thirds */
        int d = deg + 2 * k;
        iv[k] = SCALE[sc][d % 7] + 12 * (d / 7) - r;
    }
    if (dom && deg % 7 == 4) {                     /* the dominant in minor: a major third, a minor seventh */
        iv[1] = 4;
        iv[3] = 10;
    }
    int third = iv[1], fifth = iv[2], seventh = flavour >= 7 ? iv[3] : 0;
    bool ninth = flavour == 9 && (iv[4] == 14) && seventh;   /* only a major ninth: a minor one sounds wrong here */
    c->root = (uint8_t)((s->key + r) % 12);
    c->tone[0] = c->root;
    c->tone[1] = (uint8_t)((c->root + third) % 12);
    c->tone[2] = (uint8_t)((c->root + fifth) % 12);
    c->tone[3] = (uint8_t)((c->root + (seventh ? seventh : 12)) % 12);
    chord_name(c, third, fifth, seventh, ninth);
    /* the voicing: four notes, close; with a ninth it takes the root's place (the bass has the root) */
    int pcs[4] = {ninth ? (c->root + 14) % 12 : c->root, c->tone[1], c->tone[2], seventh ? c->tone[3] : (c->root + 12) % 12};
    int best_d = 1 << 20, n = 4;
    int bestv[4] = {60, 64, 67, 71};
    for (int inv = 0; inv < 4; ++inv) {            /* each inversion: its lowest note, the others above in order */
        for (int base = 48; base <= 66; ++base) {
            if ((base % 12) != pcs[inv])
                continue;
            int v[4], p = base;
            v[0] = p;
            for (int j = 1; j < 4; ++j) {
                int pc = pcs[(inv + j) % 4];
                int x = p + 1;
                while (x % 12 != pc)
                    ++x;
                v[j] = x;
                p = x;
            }
            int mid = (v[0] + v[3]) / 2;
            int d = (mid - *center) * (mid - *center) + (v[3] > 74 ? 400 : 0);
            if (d < best_d) {
                best_d = d;
                memcpy(bestv, v, sizeof bestv);
            }
        }
    }
    for (int j = 0; j < n; ++j)
        c->note[j] = (uint8_t)bestv[j];
    c->note[4] = 0;
    c->n = (uint8_t)n;
    *center = (bestv[0] + bestv[3]) / 2;
}

/* the hook: two bars of sixteenths, as scale steps from the key (0 the tonic; -128 a rest) and lengths */
static void make_hook(da_groove *g, uint32_t seed)
{
    const da_style *st = &DA_STYLE[g->s.style];
    uint32_t r = seed * 2654435761u + 0x51ED27u;
    for (int i = 0; i < DA_HOOK; ++i) {
        g->hook[i] = -128;
        g->hook_len[i] = 0;
    }
    int note = 4 + rnd_n(&r, 3);                   /* starts round the fifth above the tonic */
    int pos = 0;
    while (pos < DA_HOOK) {
        int len, gap = 0;
        switch (st->lead) {
        case LEAD_RIFF:                            /* sixteenths and eighths, busy */
            len = rnd_n(&r, 4) == 0 ? 2 : 1;
            gap = rnd_n(&r, 5) == 0 ? 1 : 0;
            break;
        case LEAD_SWELL:                           /* long notes */
            len = rnd_n(&r, 2) ? 8 : 6;
            gap = rnd_n(&r, 3) == 0 ? 2 : 0;
            break;
        case LEAD_HITS:                            /* short stabs on the off-beats */
            len = 1 + rnd_n(&r, 2);
            gap = 1 + rnd_n(&r, 3);
            break;
        default:                                   /* ROBOT: eighths, a few sixteenths, a held note at the end */
            len = rnd_n(&r, 5) == 0 ? 1 : rnd_n(&r, 4) == 0 ? 3 : 2;
            gap = rnd_n(&r, 4) == 0 ? 2 : 0;
            break;
        }
        if (pos + len > DA_HOOK)
            len = DA_HOOK - pos;
        if (pos >= 28 && st->lead == LEAD_ROBOT)   /* the phrase breathes at its end */
            break;
        g->hook[pos] = (int8_t)note;
        g->hook_len[pos] = (uint8_t)len;
        pos += len + gap;
        int mv = rnd_n(&r, 7) - 3;                 /* steps, mostly small; leaps rarer */
        if (mv == 3 || mv == -3)
            mv = mv > 0 ? 4 : -4;
        note = clampi(note + mv, 0, 11);
    }
    /* the second bar answers the first: its start repeats the first bar's, its end lands on the tonic or fifth */
    for (int i = 16; i < 24; ++i) {
        g->hook[i] = g->hook[i - 16];
        g->hook_len[i] = g->hook_len[i - 16];
    }
    for (int i = 31; i >= 24; --i)
        if (g->hook[i] != -128) {
            g->hook[i] = (int8_t)(rnd_n(&r, 2) ? 7 : 4);
            break;
        }
}

void da_rebuild(da_groove *g)
{
    da_settings *s = &g->s;
    const da_loopdef *L = &DA_LOOP[clampi(s->chords, 0, DA_LOOPS - 1)];
    int center = 60;
    g->nloop = L->n;
    for (int i = 0; i < L->n; ++i)
        build_chord(&g->loop[i], s, L->deg[i], L->flavour, L->dom != 0, &center);
    uint32_t seed = (uint32_t)s->style * 7919u + (uint32_t)s->song * 104729u + 17u;
    uint32_t r = seed;
    g->stab_pat = (uint8_t)rnd_n(&r, 4);
    g->bass_pat = (uint8_t)rnd_n(&r, 2);
    g->drum_var = (uint8_t)rnd_n(&r, 4);
    make_hook(g, seed + (uint32_t)g->hook_var * 31u);
}

/* ---- the parts' rhythms ------------------------------------------------------------------------------------ */
/* a pattern: 16 steps, the length in sixteenths of the note starting there (0 none) */
static const uint8_t STAB_PATS[7][4][16] = {
    [STAB_LOOP] = {{0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0},
                   {2, 0, 0, 1, 0, 0, 2, 0, 0, 0, 1, 0, 2, 0, 1, 0},
                   {1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0},
                   {0, 1, 0, 1, 0, 0, 2, 0, 0, 1, 0, 1, 0, 0, 2, 0}},
    [STAB_COMP] = {{3, 0, 0, 2, 0, 0, 4, 0, 0, 0, 2, 0, 0, 0, 2, 0},
                   {0, 0, 2, 0, 0, 0, 3, 0, 0, 0, 0, 2, 0, 0, 2, 0},
                   {6, 0, 0, 0, 0, 0, 0, 2, 0, 0, 4, 0, 0, 0, 0, 1},
                   {2, 0, 0, 2, 0, 0, 2, 0, 4, 0, 0, 0, 0, 0, 2, 0}},
    [STAB_PAD] = {{16}, {16}, {16}, {16}},
    [STAB_SCRATCH] = {{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                      {1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1},
                      {0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1},
                      {1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1}},
    [STAB_CLAV] = {{1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0},
                   {1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0},
                   {2, 0, 0, 1, 0, 1, 0, 1, 2, 0, 0, 1, 0, 1, 1, 0},
                   {1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1}},
    [STAB_HITS] = {{2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 2, 0, 0, 0, 1, 0},
                   {3, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 2, 0, 1, 0},
                   {1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 4, 0, 0, 0, 0, 0},
                   {2, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 2, 0, 0, 1, 0}},
    [STAB_BALLAD] = {{8, 0, 0, 0, 0, 0, 0, 0, 8}, {12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4},
                     {16}, {6, 0, 0, 0, 0, 0, 2, 0, 8}},
};
/* the bass: per step a tone (0 none, 1 root, 2 octave, 3 fifth, 4 seventh, 5 approach to the next chord's root,
 * 6 the scale step above the root) and its length */
enum { BR_NONE, BR_ROOT, BR_OCT, BR_FIFTH, BR_SEV, BR_APP, BR_PASS };
typedef struct { uint8_t tone[16], len[16]; } bass_pat_t;
static const bass_pat_t BASS_PATS[9][2] = {
    [BASS_HOUSE] = {{{0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 2, 0}, {0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0}},
                    {{1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 2, 0, 1, 0, 3, 0}, {2, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 2, 0, 1, 0}}},
    [BASS_OCTAVE] = {{{1, 0, 2, 0, 1, 0, 2, 0, 1, 0, 2, 0, 1, 0, 2, 0}, {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0}},
                     {{1, 0, 2, 1, 0, 2, 1, 0, 2, 0, 1, 2, 0, 1, 2, 5}, {1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1}}},
    [BASS_WALK] = {{{1, 0, 0, 0, 3, 0, 0, 0, 2, 0, 0, 0, 5, 0, 0, 0}, {3, 0, 0, 0, 3, 0, 0, 0, 3, 0, 0, 0, 3, 0, 0, 0}},
                   {{1, 0, 0, 6, 3, 0, 0, 0, 2, 0, 4, 0, 5, 0, 0, 0}, {2, 0, 0, 1, 3, 0, 0, 0, 2, 0, 2, 0, 3, 0, 0, 0}}},
    [BASS_SEQ] = {{{1, 1, 2, 1, 3, 1, 2, 1, 1, 1, 2, 1, 3, 1, 2, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
                  {{1, 2, 1, 3, 1, 2, 1, 4, 1, 2, 1, 3, 1, 2, 3, 2}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}},
    [BASS_SLAP] = {{{1, 0, 0, 2, 0, 0, 1, 2, 0, 0, 3, 0, 1, 0, 5, 0}, {2, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0}},
                   {{1, 0, 1, 2, 0, 1, 0, 2, 1, 0, 0, 2, 0, 3, 2, 5}, {1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1}}},
    [BASS_SUB] = {{{1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 5, 0}, {6, 0, 0, 0, 0, 0, 2, 0, 0, 0, 4, 0, 0, 0, 2, 0}},
                  {{1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0}, {3, 0, 0, 5, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0}}},
    [BASS_BALLAD] = {{{1, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 5, 0}, {8, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 2, 0}},
                     {{1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0}, {8, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0}}},
    [BASS_DRIVE] = {{{1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 2, 0}, {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0}},
                    {{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}},
    [BASS_SLOWJAM] = {{{1, 0, 0, 0, 0, 0, 3, 0, 2, 0, 1, 0, 0, 0, 0, 5}, {4, 0, 0, 0, 0, 0, 2, 0, 2, 0, 4, 0, 0, 0, 0, 1}},
                      {{1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 3, 0, 0, 0, 5, 0}, {6, 0, 0, 0, 0, 0, 0, 2, 0, 0, 4, 0, 0, 0, 2, 0}}},
};
/* the drums: per drum the velocity at each step (0 none); KICK SNARE CLAP HAT OPEN */
typedef struct { uint8_t v[5][16]; } drum_pat_t;
static const drum_pat_t DRUM_PATS[6] = {
    [DRUMS_HOUSE] = {{{120, 0, 0, 0, 120, 0, 0, 0, 120, 0, 0, 0, 120, 0, 0, 0},
                      {0},
                      {0, 0, 0, 0, 115, 0, 0, 0, 0, 0, 0, 0, 115, 0, 0, 0},
                      {70, 40, 0, 45, 70, 40, 0, 45, 70, 40, 0, 45, 70, 40, 0, 45},
                      {0, 0, 95, 0, 0, 0, 95, 0, 0, 0, 95, 0, 0, 0, 95, 0}}},
    [DRUMS_DISCO] = {{{120, 0, 0, 0, 120, 0, 0, 0, 120, 0, 0, 0, 120, 0, 0, 0},
                      {0, 0, 0, 0, 118, 0, 0, 0, 0, 0, 0, 0, 118, 0, 0, 0},
                      {0},
                      {80, 45, 0, 50, 80, 45, 0, 50, 80, 45, 0, 50, 80, 45, 0, 50},
                      {0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100, 0}}},
    [DRUMS_FUNK] = {{{120, 0, 0, 0, 0, 0, 100, 0, 0, 0, 110, 0, 0, 0, 0, 0},
                     {0, 0, 0, 0, 118, 0, 0, 35, 0, 30, 0, 0, 118, 0, 0, 40},
                     {0},
                     {85, 50, 70, 50, 85, 50, 70, 50, 85, 50, 70, 50, 85, 50, 70, 50},
                     {0}}},
    [DRUMS_BALLAD] = {{{110, 0, 0, 0, 0, 0, 0, 0, 100, 0, 70, 0, 0, 0, 0, 0},
                       {0, 0, 0, 0, 95, 0, 0, 0, 0, 0, 0, 0, 95, 0, 0, 0},
                       {0},
                       {65, 0, 45, 0, 65, 0, 45, 0, 65, 0, 45, 0, 65, 0, 45, 0},
                       {0}}},
    [DRUMS_DRIVE] = {{{120, 0, 0, 0, 120, 0, 0, 0, 120, 0, 0, 0, 120, 0, 0, 0},
                      {0, 0, 0, 0, 115, 0, 0, 0, 0, 0, 0, 0, 115, 0, 0, 0},
                      {0},
                      {75, 45, 60, 45, 75, 45, 60, 45, 75, 45, 60, 45, 75, 45, 60, 45},
                      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 90, 0}}},
    [DRUMS_SLOWJAM] = {{{115, 0, 0, 0, 0, 0, 0, 90, 0, 0, 105, 0, 0, 0, 0, 0},
                        {0},
                        {0, 0, 0, 0, 110, 0, 0, 0, 0, 0, 0, 0, 110, 0, 0, 0},
                        {70, 0, 45, 0, 70, 0, 45, 0, 70, 0, 45, 0, 70, 0, 45, 0},
                        {0}}},
};

/* ---- the engine ------------------------------------------------------------------------------------------- */
void da_init(da_groove *g, float sample_rate)
{
    memset(g, 0, sizeof *g);
    g->sr = sample_rate;
    da_defaults(&g->s);
    g->step = -1;
    g->next_style = g->next_song = -1;
    g->solo_wait = -1;
    for (int k = 0; k < 27; ++k)
        g->key_note[k] = -1;
    for (int p = 0; p < DA_PARTS; ++p)
        g->cut[p] = 127;
    da_rebuild(g);
}

void da_set_sound(da_groove *g, void (*fn)(void *ctx, const da_event *e), void *ctx)
{
    g->sound = fn;
    g->sound_ctx = ctx;
}

static void emit(da_groove *g, int on, int part, int note, int vel)
{
    if (!g->sound)
        return;
    da_event e = {(uint8_t)on, (uint8_t)part, (uint8_t)clampi(note, 0, 127), (uint8_t)clampi(vel, 1, 127)};
    g->sound(g->sound_ctx, &e);
}

static void feed(da_groove *g, int part, int note, int vel)
{
    uint8_t w = (uint8_t)((g->feed_w + 1u) % DA_FEED);
    if (w == g->feed_r)
        return;
    da_seen *n = &g->feed[g->feed_w];
    n->part = (uint8_t)part;
    n->note = (uint8_t)note;
    n->vel = (uint8_t)vel;
    g->feed_w = w;
}

bool da_feed_pop(da_groove *g, da_seen *n)
{
    if (g->feed_r == g->feed_w)
        return false;
    *n = g->feed[g->feed_r];
    g->feed_r = (uint8_t)((g->feed_r + 1u) % DA_FEED);
    return true;
}

static void msg(da_groove *g, const char *m)
{
    cpy(g->msg, m, sizeof g->msg);
    g->msg_new = 1;
}

static void note_off_now(da_groove *g, int i)
{
    emit(g, 0, g->held[i].part, g->held[i].note, 0);
    g->held[i] = g->held[--g->nheld];
}

/* a note now, its note-off after len frames; a note of the part still sounding at that pitch is cut first */
static void note(da_groove *g, int part, int n, int vel, float len_frames)
{
    if (part == DA_DRUMS) {                        /* a drum rings for its own length: then its voice is free again */
        static const float RING[DA_DR_COUNT] = {0.80f, 0.24f, 0.30f, 0.09f, 0.45f, 1.0f};
        len_frames = RING[n % DA_DR_COUNT] * g->sr;
    }
    {
        for (int i = 0; i < g->nheld; ++i)
            if (g->held[i].part == part && g->held[i].note == n) {
                note_off_now(g, i);
                break;
            }
        if (g->nheld >= DA_HELD) {                 /* full: the oldest note goes */
            int o = 0;
            for (int i = 1; i < g->nheld; ++i)
                if ((int32_t)(g->held[i].off - g->held[o].off) < 0)
                    o = i;
            note_off_now(g, o);
        }
    }
    emit(g, 1, part, n, vel);
    feed(g, part, n, vel);
    if (g->step >= 0) {
        if (part == DA_DRUMS)
            g->drum_hits[n % DA_DR_COUNT] |= (uint16_t)(1u << g->step);
        else
            g->hits[part] |= (uint16_t)(1u << g->step);
    }
    if (part == DA_LEAD)
        g->lead_on_clock = g->clock;
    {
        g->held[g->nheld].note = (uint8_t)n;
        g->held[g->nheld].part = (uint8_t)part;
        g->held[g->nheld].off = g->clock + (uint32_t)(len_frames > 1.0f ? len_frames : 1.0f);
        g->nheld++;
    }
}

void da_all_off(da_groove *g)
{
    while (g->nheld > 0)
        note_off_now(g, g->nheld - 1);
}

static void part_off(da_groove *g, int part)
{
    for (int i = g->nheld - 1; i >= 0; --i)
        if (g->held[i].part == part)
            note_off_now(g, i);
}

/* the pending notes: a swung or late sixteenth's notes, played when the clock reaches them */
#define DA_PEND 48
static struct { uint32_t at; uint8_t part, note, vel, pad; float len; } pend[DA_PEND];
static int npend;
static void schedule(da_groove *g, uint32_t delay, int part, int n, int vel, float len)
{
    if (delay == 0 || npend >= DA_PEND) {
        if (part == DA_DRUMS && (n & 0x80))       /* a choke: the open hat's note-off */
            emit(g, 0, DA_DRUMS, n & 0x7F, 0);
        else
            note(g, part, n, vel, len);
        return;
    }
    pend[npend].at = g->clock + delay;
    pend[npend].part = (uint8_t)part;
    pend[npend].note = (uint8_t)n;
    pend[npend].vel = (uint8_t)vel;
    pend[npend].len = len;
    npend++;
}

static float step_frames(const da_groove *g) { return g->sr * 60.0f / (float)clampi(g->s.bpm, 60, 200) / 4.0f; }

/* the sweep's 16-bar cycle: up over fourteen bars, back down over two */
static float sweep_cycle(const da_groove *g)
{
    if (!g->playing)
        return 0.75f;
    float pos = (float)(g->bar % 16) + ((float)(g->step < 0 ? 0 : g->step) + 1.0f - clampf(g->wait / step_frames(g), 0.0f, 1.0f)) / 16.0f;
    float c = pos / 16.0f;
    float u = c < 0.875f ? c / 0.875f : 1.0f - (c - 0.875f) / 0.125f;
    return 0.45f + 0.55f * u * u * (3.0f - 2.0f * u);
}

/* ---- the arc ---------------------------------------------------------------------------------------------- */
static int arc_len(const da_groove *g, int phase)
{
    switch (phase) {
    case DA_ARC_INTRO: return 8;
    case DA_ARC_BUILD: return g->s.build;
    case DA_ARC_DROP: return 2;
    case DA_ARC_OUT: return 8;
    default: return 16;
    }
}
static int arc_next(const da_groove *g, int phase)
{
    switch (phase) {
    case DA_ARC_INTRO: return DA_ARC_MAIN;
    case DA_ARC_MAIN: return DA_ARC_BUILD;
    case DA_ARC_BUILD: return g->s.drop ? DA_ARC_DROP : DA_ARC_SOLO;
    case DA_ARC_DROP: return DA_ARC_SOLO;
    case DA_ARC_SOLO: return DA_ARC_OUT;
    default: return DA_ARC_MAIN;
    }
}

/* a new bar: what waited for the bar line comes in, the arc moves on */
static void bar_line(da_groove *g)
{
    for (int p = 0; p < DA_PARTS; ++p) {
        g->hits_last[p] = g->hits[p];
        g->hits[p] = 0;
    }
    for (int d = 0; d < DA_DR_COUNT; ++d) {
        g->drum_last[d] = g->drum_hits[d];
        g->drum_hits[d] = 0;
    }
    if (g->next_style >= 0) {
        int keep_song = g->next_song;
        da_settings old = g->s;
        da_style_apply(&g->s, g->next_style);
        for (int i = 0; i < 4; ++i)                /* MIX ARC SOLO MENU stay the player's */
            g->s.lvl[i] = old.lvl[i];
        g->s.arc = old.arc;
        g->s.build = old.build;
        g->s.drop = old.drop;
        g->s.solo = old.solo;
        g->s.speed = old.speed;
        g->s.bend = old.bend;
        g->s.range = old.range;
        g->s.solo_bars = old.solo_bars;
        g->s.palette = old.palette;
        g->s.look = old.look;
        if (keep_song >= 0)
            g->s.song = (uint16_t)keep_song;
        g->next_style = -1;
        g->next_song = -1;
        g->sound_sig++;
        da_all_off(g);
        da_rebuild(g);
    } else if (g->next_song >= 0) {
        g->s.song = (uint16_t)g->next_song;
        uint32_t r = (uint32_t)g->s.song * 2246822519u + (uint32_t)g->s.style;
        g->s.chords = DA_STYLE[g->s.style].loops[rnd_n(&r, 4)];
        g->next_song = -1;
        da_rebuild(g);
    }
    if (g->drop && !g->drop_held && g->bar >= g->drop_end) {
        g->drop = false;
        g->crash_next = true;
    }
    if (g->s.arc) {
        if (++g->arc_bar >= arc_len(g, g->arc_phase)) {
            int nx = arc_next(g, g->arc_phase);
            if (g->arc_phase == DA_ARC_OUT) {      /* round again: a new hook */
                g->arc_round++;
                g->hook_var++;
                da_rebuild(g);
            }
            if (nx == DA_ARC_MAIN || nx == DA_ARC_SOLO)
                g->crash_next = true;
            g->arc_phase = nx;
            g->arc_bar = 0;
        }
        if (g->arc_phase == DA_ARC_SOLO && g->arc_bar == 4 && g->s.solo && g->solo_left == 0 && g->solo_wait < 0) {
            g->solo_wait = 0;                      /* the auto solo: bars 5 .. of the phase */
            msg(g, "SOLO");
        }
    }
}

/* is part p out (the arc, a drop, its level 0) */
static bool part_out(const da_groove *g, int p)
{
    if (g->s.lvl[p] == 0)
        return true;
    if (g->drop || (g->s.arc && g->arc_phase == DA_ARC_DROP))
        return p != DA_STABS;
    if (!g->s.arc)
        return false;
    if (g->arc_phase == DA_ARC_INTRO) {            /* the parts come in one by one: drums, bass, the loop, the lead */
        static const uint8_t IN_AT[DA_PARTS] = {4, 2, 8, 0};
        return g->arc_bar < IN_AT[p];
    }
    if (g->arc_phase == DA_ARC_OUT)                /* the breakdown: the loop and the lead, the bass back halfway */
        return p == DA_DRUMS ? false : p == DA_BASS ? g->arc_bar < 4 : false;
    return false;
}

/* the drums of step i, with ENERGY, HATS, the fills, the arc */
static void play_drums(da_groove *g, int i, uint32_t delay, float hum)
{
    const da_style *st = &DA_STYLE[g->s.style];
    const drum_pat_t *dp = &DRUM_PATS[st->drums];
    float e = (float)g->s.energy / 100.0f;
    int bar_in = g->bar;
    bool fill = false;
    static const uint8_t EVERY[DA_FILLS_COUNT] = {0, 8, 4, 2};
    int ev = EVERY[g->s.fills];
    if (ev && (bar_in % ev) == ev - 1 && i >= 12)
        fill = true;
    bool build = g->s.arc && g->arc_phase == DA_ARC_BUILD;
    bool outro = g->s.arc && g->arc_phase == DA_ARC_OUT;
    bool intro_light = g->s.arc && g->arc_phase == DA_ARC_INTRO && g->arc_bar < 2;
    int vel[DA_DR_COUNT] = {0};
    vel[DA_DR_KICK] = dp->v[0][i];
    vel[DA_DR_SNARE] = dp->v[1][i];
    vel[DA_DR_CLAP] = dp->v[2][i];
    /* the backbeat as DRUMS > SNARE has it */
    int back = dp->v[1][i] > dp->v[2][i] ? dp->v[1][i] : dp->v[2][i];
    if (back >= 90) {
        vel[DA_DR_SNARE] = g->s.snare == DA_SNARE_CLAP ? 0 : back;
        vel[DA_DR_CLAP] = g->s.snare == DA_SNARE_SNARE ? 0 : back;
    }
    /* the hats */
    int h = dp->v[3][i], o = dp->v[4][i];
    switch (g->s.hats) {
    case DA_HATS_8THS: h = (i % 2 == 0) ? (i % 4 == 0 ? 75 : 60) : 0; o = 0; break;
    case DA_HATS_16THS: h = i % 4 == 0 ? 80 : i % 2 == 0 ? 60 : 42; o = i == 14 && (g->drum_var & 1) ? 85 : 0; break;
    case DA_HATS_OFFBEAT: h = i % 4 == 2 ? 0 : (i % 2 ? 40 : 60); o = i % 4 == 2 ? 95 : 0; break;
    default: h = i % 2 ? 45 : 0; o = i % 2 == 0 ? (i % 4 == 2 ? 100 : 70) : 0; break;
    }
    if (e < 0.35f && (i % 2))                      /* low ENERGY: no sixteenth hats */
        h = 0;
    if (e < 0.2f)
        o = 0;
    if (e > 0.8f && st->drums != DRUMS_BALLAD && i == 15 && (g->drum_var & 2))   /* high: a pickup kick */
        vel[DA_DR_KICK] = vel[DA_DR_KICK] ? vel[DA_DR_KICK] : 85;
    if (e > 0.85f && (i == 7 || i == 15) && !vel[DA_DR_SNARE] && st->drums != DRUMS_HOUSE)   /* ghost notes */
        vel[DA_DR_SNARE] = 30;
    vel[DA_DR_HAT] = h;
    vel[DA_DR_OPEN] = o;
    if (fill) {                                    /* a fill: the backbeat drum on the sixteenths, rising */
        int d = g->s.snare == DA_SNARE_CLAP ? DA_DR_CLAP : DA_DR_SNARE;
        if ((g->drum_var & 2) ? (i % 2 == 0 || i >= 14) : (i == 13 || i == 14 || i == 15))
            vel[d] = 70 + (i - 12) * 12;
        vel[DA_DR_OPEN] = i == 15 ? 100 : 0;
        if (i == 15)
            vel[DA_DR_HAT] = 0;
    }
    if (build) {                                   /* the build: a roll, quarters then eighths then sixteenths */
        int span = g->s.build, b = g->arc_bar;
        int every = b < span / 2 ? 4 : b < span - span / 4 ? 2 : 1;
        int d = g->s.snare == DA_SNARE_CLAP ? DA_DR_CLAP : DA_DR_SNARE;
        if (i % every == 0)
            vel[d] = 60 + (b * 60) / (span > 1 ? span : 1);
        if (b == span - 1 && i >= 12)              /* the last beat before the drop: silence, but the roll */
            vel[DA_DR_KICK] = vel[DA_DR_HAT] = vel[DA_DR_OPEN] = 0;
    }
    if (outro) {                                   /* the breakdown: no kick, the hats thin */
        vel[DA_DR_KICK] = 0;
        vel[DA_DR_SNARE] = vel[DA_DR_CLAP] = 0;
        if (g->arc_bar < 4)
            vel[DA_DR_HAT] = vel[DA_DR_HAT] && i % 4 == 2 ? 50 : 0;
    }
    if (intro_light)                               /* the first bars: the kick alone */
        vel[DA_DR_SNARE] = vel[DA_DR_CLAP] = vel[DA_DR_HAT] = vel[DA_DR_OPEN] = 0;
    if (i == 0 && g->crash_next) {
        vel[DA_DR_CRASH] = 110;
        g->crash_next = false;
    }
    for (int d = 0; d < DA_DR_COUNT; ++d) {
        if (!vel[d])
            continue;
        int v = vel[d];
        if (d != DA_DR_KICK && d != DA_DR_CRASH)
            v = (int)((float)v * (0.75f + 0.25f * e));
        v += (int)(hum * 10.0f * (float)g->s.human / 100.0f);
        if (d == DA_DR_KICK)
            g->beat_ph = 0;                        /* (the pump follows the kick: da_process) */
        if (d == DA_DR_HAT && g->s.hats != DA_HATS_OPEN)   /* a closed hat chokes the open one (HATS OPEN: they ring) */
            schedule(g, delay, DA_DRUMS, 0x80 | DA_DR_OPEN, 1, 0);
        schedule(g, delay, DA_DRUMS, d, clampi(v, 1, 127), 0);
    }
}

/* the chord sounding at bar b */
static const da_chord *chord_at(const da_groove *g, int b)
{
    int bpc = da_bars_per_chord(&g->s);
    return &g->loop[(b / bpc) % (g->nloop > 0 ? g->nloop : 1)];
}
static const da_chord *next_chord(const da_groove *g, int b)
{
    int bpc = da_bars_per_chord(&g->s);
    return &g->loop[((b / bpc) + 1) % (g->nloop > 0 ? g->nloop : 1)];
}
int da_chord_now(const da_groove *g)
{
    int bpc = da_bars_per_chord(&g->s);
    return g->nloop > 0 ? (g->bar / bpc) % g->nloop : 0;
}

static int bass_note(const da_groove *g, int tone, const da_chord *c, const da_chord *nx)
{
    int root = 36 + ((c->root - 36) % 12 + 12) % 12;   /* C2 .. B2 */
    if (root > 43)
        root -= 12;                                     /* (G#1 .. G2: the bass sits low) */
    switch (tone) {
    case BR_OCT: return root + 12;
    case BR_FIFTH: return root + ((c->tone[2] - c->root + 12) % 12);
    case BR_SEV: return root + ((c->tone[3] - c->root + 12) % 12);
    case BR_APP: {
        int nr = 36 + ((nx->root - 36) % 12 + 12) % 12;
        if (nr > 43) nr -= 12;
        return nx == c ? root + 12 : nr - 1;            /* a semitone below the next root */
    }
    case BR_PASS: return da_snap(&g->s, root + 2);
    default: return root;
    }
}

static void play_stabs(da_groove *g, int i, uint32_t delay, float sf, float hum)
{
    const da_style *st = &DA_STYLE[g->s.style];
    const da_chord *c = chord_at(g, g->bar);
    int bpc = da_bars_per_chord(&g->s);
    int len = STAB_PATS[st->stab][g->stab_pat][i];
    float e = (float)g->s.energy / 100.0f;
    if (st->stab == STAB_PAD || st->stab == STAB_BALLAD) {
        if (st->stab == STAB_PAD) {                /* a pad: the chord held for as long as it lasts */
            if (i != 0 || (g->bar % bpc) != 0)
                return;
            len = 16 * bpc;
        } else if (!len) {
            return;
        }
        for (int k = 0; k < c->n; ++k)
            schedule(g, delay + (uint32_t)(k * (st->stab == STAB_BALLAD ? sf * 0.08f : 0.0f)), DA_STABS, c->note[k],
                     clampi(78 + (int)(hum * 8.0f), 1, 127), sf * (float)len - 40.0f);
        return;
    }
    if (!len) {
        if (e > 0.85f && st->stab == STAB_LOOP && (i == 7 || i == 15) && (g->stab_pat & 1))   /* high energy: more chops */
            len = 1;
        else
            return;
    }
    int vel = 100;
    if (st->stab == STAB_SCRATCH)                  /* muted strums: the accents on the off-beat eighths */
        vel = i % 4 == 2 ? 112 : 46;
    else if (st->stab == STAB_CLAV)
        vel = i % 4 == 0 ? 105 : 80;
    else if (st->stab == STAB_LOOP)
        vel = i % 4 == 2 ? 108 : 92;
    vel += (int)(hum * 10.0f);
    float gate = st->stab == STAB_SCRATCH ? 0.35f : st->stab == STAB_CLAV ? 0.6f : 0.85f;
    if (st->stab == STAB_CLAV) {                   /* the clavinet: the voicing's top and bottom halves alternate */
        int half = (i / 2) % 2;
        for (int k = half * 2; k < half * 2 + 2 && k < c->n; ++k)
            schedule(g, delay, DA_STABS, c->note[k], vel, sf * (float)len * gate);
        return;
    }
    /* a chop sounds the voicing's top three: the shared voice budget (eight for all four parts) stays for the
     * drums' tails and the bass */
    int k0 = c->n > 3 ? 1 : 0;
    for (int k = k0; k < c->n; ++k)
        schedule(g, delay, DA_STABS, c->note[k], clampi(vel - (k == c->n - 1 ? 0 : 6), 1, 127), sf * (float)len * gate);
}

static void play_bass(da_groove *g, int i, uint32_t delay, float sf, float hum)
{
    const da_style *st = &DA_STYLE[g->s.style];
    const bass_pat_t *bp = &BASS_PATS[st->bass][g->bass_pat];
    int tone = bp->tone[i], len = bp->len[i];
    if (!tone)
        return;
    const da_chord *c = chord_at(g, g->bar);
    int bpc = da_bars_per_chord(&g->s);
    bool last_bar = (g->bar % bpc) == bpc - 1;
    const da_chord *nx = last_bar ? next_chord(g, g->bar) : c;
    if (tone == BR_APP && !last_bar)               /* an approach only leads into a new chord */
        tone = BR_OCT;
    int n = bass_note(g, tone, c, nx);
    int vel = (i % 4 == 0 ? 110 : 96) + (int)(hum * 8.0f);
    float gate = st->bass == BASS_SEQ || st->bass == BASS_DRIVE ? 0.55f : st->bass == BASS_SLAP ? 0.7f : 0.9f;
    schedule(g, delay, DA_BASS, n, clampi(vel, 1, 127), sf * (float)len * gate);
}

/* the solo: runs through the scale, up and down within RANGE octaves, an accent on the beat */
static void play_solo(da_groove *g, int i, uint32_t delay, float sf)
{
    int per = g->s.speed == DA_SPEED_8 ? 2 : 1;
    int sub = g->s.speed == DA_SPEED_TRIP ? 3 : 2;   /* notes in two sixteenths: triplets 3 */
    if (i % per)
        return;
    int lo = 7, hi = 7 + 7 * g->s.range;
    uint32_t r = g->rnd;
    int count = g->s.speed == DA_SPEED_TRIP ? (i % 2 == 0 ? sub : 0) : 1;
    float spacing = g->s.speed == DA_SPEED_TRIP ? sf * 2.0f / 3.0f : sf * (float)per;
    for (int k = 0; k < count; ++k) {
        if (rnd_n(&r, 9) == 0)
            g->solo_dir = -g->solo_dir;
        if (!g->solo_dir)
            g->solo_dir = 1;
        g->solo_note += g->solo_dir * (rnd_n(&r, 5) == 0 ? 2 : 1);
        if (g->solo_note > hi) { g->solo_note = hi - 1; g->solo_dir = -1; }
        if (g->solo_note < lo) { g->solo_note = lo + 1; g->solo_dir = 1; }
        int n = step_note(g->s.scale, 48 + g->s.key, g->solo_note);
        int vel = (i % 4 == 0 && k == 0) ? 118 : 96;
        /* a bend: the step below, then the note itself a moment later (the glide carries it up) */
        if (g->s.bend > 0 && rnd_n(&r, 100) < g->s.bend / 4 && i % 4 == 0 && k == 0) {
            int below = step_note(g->s.scale, 48 + g->s.key, g->solo_note - 1);
            schedule(g, delay, DA_LEAD, below, vel, spacing * 0.5f);
            schedule(g, delay + (uint32_t)(spacing * 0.35f), DA_LEAD, n, vel, spacing * 1.6f);
        } else {
            schedule(g, delay + (uint32_t)(spacing * (float)k), DA_LEAD, n, vel, spacing * 0.9f);
        }
    }
    g->rnd = r;
}

static void play_lead(da_groove *g, int i, uint32_t delay, float sf, float hum)
{
    const da_style *st = &DA_STYLE[g->s.style];
    if (g->keys)                                   /* the player has the lead */
        return;
    float v = (float)g->s.voice / 100.0f;
    if (v <= 0.0f)
        return;
    const da_chord *c = chord_at(g, g->bar);
    if (st->lead == LEAD_ARP) {                    /* an arpeggiator through the chord, two octaves */
        if (v < 0.5f && (i % 2))
            return;
        static const uint8_t ORD[8] = {0, 1, 2, 3, 4, 3, 2, 1};
        int k = ORD[i % 8], oct = k / 4, t = k % 4;
        int n = 60 + ((c->tone[t] - 60) % 12 + 12) % 12 + 12 * oct;
        if (n > 84)
            n -= 12;
        schedule(g, delay, DA_LEAD, n, clampi(70 + (i % 4 == 0 ? 20 : 0) + (int)(hum * 6.0f), 1, 127), sf * 0.6f);
        return;
    }
    /* the hook: on every phrase with VOICE high, only in the second half of each eight bars with it low */
    if (v < 0.45f && (g->bar % 8) < 4)
        return;
    int hp = (g->bar % 2) * 16 + i;
    if (g->hook[hp] == -128)
        return;
    int len = g->hook_len[hp];
    int n = step_note(g->s.scale, 60 + g->s.key - (g->s.key > 6 ? 12 : 0), g->hook[hp]);
    if (len >= 2 || i % 4 == 0) {                  /* the long and strong notes onto the chord */
        int best = n, bd = 99;
        for (int t = 0; t < 4; ++t)
            for (int o = -1; o <= 1; ++o) {
                int cand = n - ((n % 12) - c->tone[t]) + o * 12;
                int d = cand - n < 0 ? n - cand : cand - n;
                if (d < bd) {
                    bd = d;
                    best = cand;
                }
            }
        n = best;
    }
    int vel = (int)(80.0f + 30.0f * v) + (i % 4 == 0 ? 8 : 0) + (int)(hum * 6.0f);
    float gate = st->lead == LEAD_SWELL ? 1.0f : st->lead == LEAD_HITS ? 0.5f : 0.85f;
    schedule(g, delay, DA_LEAD, n, clampi(vel, 1, 127), sf * (float)len * gate);
}

/* one sixteenth: the parts, swung and humanized */
static void do_step(da_groove *g)
{
    if (++g->step >= 16) {
        g->step = 0;
        g->bar++;
        bar_line(g);
    }
    int i = g->step;
    if (i == 0)
        g->rnd = (uint32_t)g->bar * 2654435761u + (uint32_t)g->s.song * 97u + 1u;
    float sf = step_frames(g);
    uint32_t delay = 0;
    if (i % 2 && g->s.swing)                        /* swing: the off sixteenths late */
        delay = (uint32_t)(sf * (float)g->s.swing / 100.0f * 0.66f);
    for (int p = 0; p < DA_PARTS; ++p) {
        bool out = part_out(g, p);
        if (out != g->muted[p] && out)
            part_off(g, p);
        g->muted[p] = out;
    }
    /* the solo */
    if (g->solo_wait >= 0) {
        if (g->solo_wait == 0 || i % 4 == 0) {
            g->solo_wait = -1;
            g->solo_left = g->s.solo_bars * 16;
            g->solo_note = 10;
            g->solo_dir = 1;
        } else {
            g->solo_wait--;
        }
    }
    for (int p = 0; p < DA_PARTS; ++p) {
        if (g->muted[p])
            continue;
        float hum = (rnd_f(&g->rnd) - 0.5f) * 2.0f * (float)g->s.human / 100.0f;
        uint32_t d = delay + (uint32_t)((hum > 0 ? hum : 0.0f) * g->sr * 0.010f);   /* late only, up to 10 ms */
        switch (p) {
        case DA_STABS: play_stabs(g, i, d, sf, hum); break;
        case DA_BASS: play_bass(g, i, d, sf, hum); break;
        case DA_LEAD:
            if (g->solo_left > 0)
                play_solo(g, i, d, sf);
            else
                play_lead(g, i, d, sf, hum);
            break;
        default: play_drums(g, i, d, hum); break;
        }
    }
    if (g->solo_left > 0)
        --g->solo_left;                            /* (over: the glue gives the LEAD its own sound back) */
}

void da_play(da_groove *g, bool on)
{
    if (on == g->playing)
        return;
    da_all_off(g);
    npend = 0;
    g->playing = on;
    g->clock = 0;
    g->wait = 0;
    g->step = -1;
    g->bar = 0;
    g->drop = false;
    g->drop_held = false;
    g->solo_left = 0;
    g->solo_wait = -1;
    g->crash_next = false;
    g->arc_phase = g->s.arc ? DA_ARC_INTRO : DA_ARC_MAIN;
    g->arc_bar = 0;
    g->arc_round = 0;
    g->hook_var = 0;
    g->beat_ph = 1.0f;
    if (on) {
        if (g->next_style >= 0 || g->next_song >= 0) {   /* (queued while stopped: at once) */
            g->bar = -1;
            g->step = 15;
            bar_line(g);
            g->bar = 0;
            g->step = -1;
        }
        da_rebuild(g);
    }
}

void da_queue_style(da_groove *g, int style)
{
    style = ((style % DA_STYLES) + DA_STYLES) % DA_STYLES;
    g->next_style = style;
    if (!g->playing) {                             /* stopped: at once */
        int b = g->bar;
        g->bar = -1;
        bar_line(g);
        g->bar = b;
    } else {
        char m[24] = "NEXT ";
        cpy(m + 5, DA_STYLE[style].name, (int)sizeof m - 5);
        msg(g, m);
    }
}

void da_queue_song(da_groove *g, int song)
{
    g->next_song = ((song % 1000) + 1000) % 1000;
    if (!g->playing) {
        int b = g->bar;
        g->bar = -1;
        bar_line(g);
        g->bar = b;
    }
}

void da_drop(da_groove *g, bool down)
{
    if (!g->playing)
        return;
    if (down) {
        g->drop = true;
        g->drop_held = true;
        g->drop_end = g->bar + 2;                  /* a tap: back on the bar line two bars on */
        for (int p = 0; p < DA_PARTS; ++p)
            if (p != DA_STABS)
                part_off(g, p);
        msg(g, "DROP");
    } else {
        g->drop_held = false;
        if (g->drop && g->bar >= g->drop_end)      /* held past its two bars: back at the next bar line */
            g->drop_end = g->bar + 1;
    }
}

void da_solo(da_groove *g)
{
    if (!g->playing)
        return;
    if (g->solo_left > 0 || g->solo_wait >= 0) {
        g->solo_left = 0;
        g->solo_wait = -1;
        part_off(g, DA_LEAD);
        msg(g, "SOLO OFF");
    } else {
        g->solo_wait = 4 - (g->step < 0 ? 0 : g->step % 4) - 1;   /* sixteenths to the next beat */
        msg(g, "SOLO");
    }
}

bool da_soloing(const da_groove *g) { return g->solo_left > 0 || g->solo_wait >= 0; }

void da_key(da_groove *g, int k, int note, bool down)
{
    if (k < 0 || k >= 27)
        return;
    if (down) {
        int n = da_snap(&g->s, note);
        if (g->key_note[k] >= 0)
            emit(g, 0, DA_LEAD, g->key_note[k], 0);
        if (!g->keys)                              /* the hook makes way */
            part_off(g, DA_LEAD);
        g->keys |= 1u << k;
        g->key_note[k] = n;
        emit(g, 1, DA_LEAD, n, 104);
        feed(g, DA_LEAD, n, 104);
    } else {
        g->keys &= ~(1u << k);
        if (g->key_note[k] >= 0)
            emit(g, 0, DA_LEAD, g->key_note[k], 0);
        g->key_note[k] = -1;
    }
}

/* the block: notes in time, then the sweep and the pump for the glue */
void da_process(da_groove *g, int frames)
{
    if (g->playing) {
        float sf = step_frames(g);
        int left = frames;
        while (left > 0) {
            if (g->wait <= 0.0f) {
                do_step(g);
                g->wait += sf;
            }
            int chunk = (int)g->wait + 1;
            if (chunk > left)
                chunk = left;
            if (chunk < 1)
                chunk = 1;
            g->clock += (uint32_t)chunk;
            g->wait -= (float)chunk;
            g->beat_ph += (float)chunk / (sf * 4.0f);   /* beats since the last kick */
            left -= chunk;
            for (int k = 0; k < npend;) {          /* the swung and late notes */
                if ((int32_t)(g->clock - pend[k].at) >= 0) {
                    int nn = pend[k].note;
                    if (pend[k].part == DA_DRUMS && (nn & 0x80))
                        emit(g, 0, DA_DRUMS, nn & 0x7F, 0);   /* a choke */
                    else if (!g->muted[pend[k].part])
                        note(g, pend[k].part, nn, pend[k].vel, pend[k].len);
                    pend[k] = pend[--npend];
                } else {
                    ++k;
                }
            }
            for (int k = 0; k < g->nheld;) {       /* the note-offs */
                if ((int32_t)(g->clock - g->held[k].off) >= 0)
                    note_off_now(g, k);
                else
                    ++k;
            }
        }
    }
    /* the sweep: the knob's ceiling, the cycle's breath, the build's opening, the drop's filtered loop */
    float sw = (float)g->s.sweep / 100.0f;
    float open = sweep_cycle(g);
    if (g->playing && g->s.arc) {
        if (g->arc_phase == DA_ARC_BUILD)
            open = 0.45f + 0.55f * ((float)g->arc_bar + (float)(g->step < 0 ? 0 : g->step) / 16.0f) / (float)g->s.build;
        else if (g->arc_phase == DA_ARC_INTRO)
            open = 0.3f + 0.6f * ((float)g->arc_bar + (float)(g->step < 0 ? 0 : g->step) / 16.0f) / 8.0f;
        else if (g->arc_phase == DA_ARC_OUT)
            open = 0.35f + 0.1f * (float)g->arc_bar / 8.0f;
    }
    if (g->drop || (g->s.arc && g->arc_phase == DA_ARC_DROP && g->playing))
        open = 0.32f;
    float cut = clampf(sw * open, 0.0f, 1.0f);
    g->sweep_now = cut;
    uint8_t mask = DA_STYLE[g->s.style].sweep_parts;
    for (int p = 0; p < DA_PARTS; ++p)
        g->cut[p] = (uint8_t)((mask >> p) & 1u ? (int)(cut * 127.0f + 0.5f) : 127);
    /* the pump: a fast duck on the kick (6 ms down), back over 70 % of the beat */
    float depth = g->playing && !g->muted[DA_DRUMS] ? (float)g->s.pump / 100.0f * 0.8f : 0.0f;
    float beat_s = 60.0f / (float)clampi(g->s.bpm, 60, 200);
    float t = g->beat_ph * beat_s;                 /* seconds since the kick (beat_ph counts beats) */
    float d;
    if (t < 0.006f)
        d = depth * t / 0.006f;
    else {
        float u = clampf((t - 0.006f) / (0.7f * beat_s), 0.0f, 1.0f);
        d = depth * (1.0f - u) * (1.0f - u);
    }
    for (int p = 0; p < DA_PARTS; ++p)
        g->duck[p] = (uint8_t)(p == DA_DRUMS ? 0 : (int)(d * 127.0f + 0.5f));
}
