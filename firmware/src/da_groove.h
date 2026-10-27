/* SPDX-License-Identifier: GPL-3.0-only */
/* DAURADE groove engine: ten styles of filtered, pumping, robotic grooves on four parts (STABS, BASS, LEAD, DRUMS).
 * It runs in time, sixteenth by sixteenth, and hands each note-on and note-off to the sound (Felucca's tracks on
 * the FM-1, a renderer on the host). Besides the notes it computes, every block, what the glue turns into sound:
 *
 *   the SWEEP   how far the loop's filter is open (0..127 per part): the SWEEP knob is its ceiling, and over each
 *               16-bar cycle the filter breathes up to it and falls back; a BUILD opens it all the way, a DROP
 *               closes it to a filtered loop
 *   the PUMP    how far each part ducks under the kick (0..127): a fast duck on every beat, a smooth return over
 *               most of the beat; the drums themselves never duck
 *
 * A song is a style plus a number (the SONG, turned by ALGORITHM): the number picks the chord loop, the parts'
 * patterns and the hook, so the same style and number always play the same song. A new style or song comes in on
 * the next bar line; the knobs act at once.
 *
 * Performance: DROP (everything but the filtered loop out for two bars, then back on the bar; held: until let go),
 * SOLO (a guitar-like solo over the next bars, from the next beat), the keys (the lead live, snapped to the
 * scale). ARC on: the song shapes itself (an intro that brings the parts in one by one, a build, a drop, a solo,
 * a breakdown, round again with a new hook).
 *
 * No libm, no doubles: floats on the FPU (single precision), as Flyfish and Tuna. */
#ifndef DA_GROOVE_H
#define DA_GROOVE_H

#include <stdbool.h>
#include <stdint.h>

enum { DA_STABS, DA_BASS, DA_LEAD, DA_DRUMS, DA_PARTS };
enum { DA_SIDE_CHROME, DA_SIDE_SESSION };
#define DA_STYLES 10
enum { DA_ST_FILTER_HOUSE, DA_ST_ROBOT_POP, DA_ST_SHRED, DA_ST_NIGHT_DRIVE, DA_ST_SLOW_JAM,
       DA_ST_DISCO, DA_ST_SEQUENCER, DA_ST_FUNK, DA_ST_BALLAD, DA_ST_FINALE };
enum { DA_SC_DORIAN, DA_SC_MINOR, DA_SC_MAJOR, DA_SC_MIXO, DA_SC_LYDIAN, DA_SC_HARMONIC, DA_SCALES };
enum { DA_HATS_8THS, DA_HATS_16THS, DA_HATS_OFFBEAT, DA_HATS_OPEN, DA_HATS_COUNT };
enum { DA_FILLS_OFF, DA_FILLS_8, DA_FILLS_4, DA_FILLS_2, DA_FILLS_COUNT };
enum { DA_KIT_CRISP, DA_KIT_SESSION, DA_KIT_MACHINE, DA_KIT_COUNT };
enum { DA_KICK_PUNCH, DA_KICK_ROUND, DA_KICK_COUNT };
enum { DA_SNARE_CLAP, DA_SNARE_SNARE, DA_SNARE_BOTH, DA_SNARE_COUNT };
enum { DA_VOWELS_AH_EE, DA_VOWELS_OH_AH, DA_VOWELS_OO_EE, DA_VOWELS_EE_AH, DA_VOWELS_AH_OH,
       DA_VOWELS_PHRASE, DA_VOWELS_COUNT };   /* PHRASE: a vowel per note, AH EH OH EH AH EH EH EH, each hook again */
#define DA_PHRASE_LEN 8
enum { DA_TALK_16, DA_TALK_8, DA_TALK_4, DA_TALK_2, DA_TALK_COUNT };
enum { DA_DTIME_16, DA_DTIME_8, DA_DTIME_3_16, DA_DTIME_4, DA_DTIME_COUNT };
enum { DA_SPEED_8, DA_SPEED_TRIP, DA_SPEED_16, DA_SPEED_COUNT };
/* the drums as the groove names them; the glue maps them to the kit (da_sounds.h) */
enum { DA_DR_KICK, DA_DR_SNARE, DA_DR_CLAP, DA_DR_HAT, DA_DR_OPEN, DA_DR_CRASH, DA_DR_COUNT };
/* the arc's phases */
enum { DA_ARC_INTRO, DA_ARC_MAIN, DA_ARC_BUILD, DA_ARC_DROP, DA_ARC_SOLO, DA_ARC_OUT, DA_ARC_COUNT };
#define DA_SETS 8
#define DA_SOUNDS 10                               /* each part's SOUND list (da_sounds.h) */
#define DA_LOOPS 12                                /* the chord loops (da_groove.c DA_LOOP) */

/* what the player set: the cards (and the knobs above them). Saved as it is (da_fw.c), so append only */
typedef struct {
    uint8_t style;
    uint8_t sweep, pump, voice, energy;            /* PLAY 0..100 */
    uint8_t snd[4];                                /* SOUND: STABS BASS LEAD (indices into each part's list), DRUMS = kit */
    uint8_t reso, drive, phaser, crush;            /* TONE 0..100 (CRUSH 0: off) */
    uint8_t vowels, talk, glide;                   /* VOICE: the vowels it talks through, how fast, its glide */
    int8_t shift;                                  /* VOICE: formant shift -12..12 */
    uint8_t key, scale, chords, bars;              /* KEY: 0..11 (C..B), DA_SC_*, the loop (0..DA_LOOPS-1), 1 2 4 */
    uint8_t swing, human, hats, fills;             /* GROOVE: swing 0..60 %, human 0..100, DA_HATS_*, DA_FILLS_* */
    uint8_t kick, snare, snap;                     /* DRUMS: DA_KICK_*, DA_SNARE_*, SNAP 0..100 (KIT: snd[3]) */
    uint8_t lvl[4];                                /* MIX 0..100 */
    uint8_t reverb, delay, dtime, width;           /* SPACE */
    uint8_t arc, build, drop, solo;                /* ARC: on, build bars (4 8 16), auto drop, auto solo */
    uint8_t speed, bend, range, solo_bars;         /* SOLO: DA_SPEED_*, bend 0..100, octaves 1..3, bars 2 4 8 */
    uint8_t bpm;                                   /* SELECT: 60..200 */
    uint8_t palette, look;                         /* MENU: COLOR, STYLE (FLAT, LINE) */
    uint16_t song;                                 /* ALGORITHM: the song number in the style, 0..999 */
} da_settings;

typedef struct {
    uint8_t on, part, note, vel;                   /* DRUMS: note is DA_DR_*; vel 1..127 (on) */
} da_event;

/* what the screen hears: each note-on */
typedef struct { uint8_t part, note, vel, pad; } da_seen;

/* a chord as played: its root (pitch class), the notes (MIDI) it voices, its name */
typedef struct {
    uint8_t root, n;
    uint8_t note[5];                               /* stab voicing, low to high */
    uint8_t tone[4];                               /* chord tones as pitch classes: root, third, fifth, seventh */
    char name[8];
} da_chord;

#define DA_HELD 24
#define DA_FEED 32
#define DA_LOOP_MAX 8
#define DA_HOOK 32                                 /* the hook: two bars of sixteenths */

typedef struct {
    da_settings s;
    float sr;
    bool playing;
    uint32_t clock;                                /* frames since PLAY */
    float wait;                                    /* frames until the next sixteenth */
    int step;                                      /* the sixteenth sounding in its bar (0..15; -1 before the first) */
    int bar;                                       /* bars since PLAY (0: the first) */
    float beat_ph;                                 /* 0..1 through the beat (the pump) */
    /* the song: built from the style, the song number, the key and the loop (da_build) */
    da_chord loop[DA_LOOP_MAX];
    int nloop;
    uint8_t stab_pat, bass_pat, drum_var, hook_var;
    int8_t hook[DA_HOOK];                          /* scale steps from the chord's root (-128: rest); length in hook_len */
    uint8_t hook_len[DA_HOOK];
    uint32_t rnd;                                  /* the per-note randomness (human, ghosts): seeded per bar */
    /* what waits for the next bar line */
    int next_style;                                /* -1 none */
    int next_song;                                 /* -1 none */
    /* performance */
    bool drop;                                     /* the parts out but the loop */
    int drop_end;                                  /* the bar it comes back on */
    bool drop_held;
    bool crash_next;                               /* a crash on the next downbeat */
    int solo_left;                                 /* sixteenths of solo still to play (0 none) */
    int solo_wait;                                 /* armed: sixteenths until it starts (-1 none) */
    int solo_note;                                 /* the scale step it is at */
    int solo_dir;
    uint32_t keys;                                 /* the FM-1's keys held (bit note - 53) */
    int key_note[27];                              /* the note each key plays (snapped), -1 none */
    /* the arc */
    int arc_phase, arc_bar;                        /* phase, bars into it */
    int arc_round;
    uint8_t intro_in;                              /* INTRO: the parts in so far (bit per part) */
    /* the block's controls for the glue */
    uint8_t cut[DA_PARTS];                         /* 0..127: how far each part's filter is open */
    uint8_t duck[DA_PARTS];                        /* 0..127: how far each part is ducked now */
    float sweep_now;                               /* 0..1: the filter for the screen */
    bool muted[DA_PARTS];                          /* out (DROP, the arc, a level of 0) */
    /* what each part played, a bit per sixteenth: the bar playing and the bar before (the screen) */
    uint16_t hits[DA_PARTS], hits_last[DA_PARTS];
    uint16_t drum_hits[DA_DR_COUNT], drum_last[DA_DR_COUNT];
    uint32_t lead_on_clock;                        /* the lead's latest note-on (the VOICE page's vowel) */
    uint8_t lead_syl, lead_syl_now;                /* PHRASE: the next vowel of the phrase, the one sounding */
    /* the notes sounding: their note-off at clock off */
    struct { uint8_t note, part; uint32_t off; } held[DA_HELD];
    int nheld;
    da_seen feed[DA_FEED];
    uint8_t feed_w, feed_r;
    char msg[24];                                  /* a word for the screen ("NEXT DISCO", "SOLO IN 1"); the UI clears it */
    uint8_t msg_new;
    uint8_t sound_sig;                             /* bumped when every part's sound must load again (a style, a set): the glue
                                                    * reloads (a card's sound and the solo it follows by itself) */
    void (*sound)(void *ctx, const da_event *e);
    void *sound_ctx;
} da_groove;

typedef struct {
    const char *name;
    uint8_t side, bpm, key, scale;
    uint8_t sweep, pump, voice, energy;
    uint8_t snd[4];                                /* STABS BASS LEAD sound, the kit */
    uint8_t solo_snd;                              /* the LEAD sound for solos */
    uint8_t loops[4];                              /* the chord loops a song picks from (DA_LOOP indices) */
    uint8_t bars;                                  /* bars a chord */
    uint8_t swing, human, hats, fills;
    uint8_t kick, snare;
    uint8_t stab, bass, lead, drums;               /* the rhythm families (da_groove.c) */
    uint8_t sweep_parts;                           /* bit per part: what the SWEEP filters */
    uint8_t reverb, delay, dtime;
    uint8_t reso, drive, phaser;
} da_style;

extern const da_style DA_STYLE[DA_STYLES];

void da_defaults(da_settings *s);                  /* FILTER HOUSE, as it starts */
void da_style_apply(da_settings *s, int style);    /* a style's own settings into s (PLAY SOUND TONE VOICE KEY GROOVE
                                                    * DRUMS SPACE and the tempo); MIX ARC SOLO MENU stay */
void da_settings_fix(da_settings *s);              /* every field into its range (a loaded record) */
void da_init(da_groove *g, float sample_rate);
void da_set_sound(da_groove *g, void (*fn)(void *ctx, const da_event *e), void *ctx);

void da_play(da_groove *g, bool on);
void da_rebuild(da_groove *g);                     /* the song again from the settings (KEY, the loop, a sound) */
void da_queue_style(da_groove *g, int style);      /* playing: on the next bar line; stopped: at once */
void da_queue_song(da_groove *g, int song);
void da_drop(da_groove *g, bool down);             /* REC: down starts it, up lets it come back */
void da_solo(da_groove *g);                        /* SEQ: a solo from the next beat; again: stop it */
void da_key(da_groove *g, int key, int note, bool down);   /* the FM-1's key (0..26) playing note: the lead, snapped */
void da_process(da_groove *g, int frames);         /* the audio: notes in time, the sweep and the pump */
void da_all_off(da_groove *g);

bool da_feed_pop(da_groove *g, da_seen *n);
int da_chord_now(const da_groove *g);              /* the loop's chord sounding (0..nloop-1) */
int da_bars_per_chord(const da_settings *s);
bool da_soloing(const da_groove *g);               /* a solo playing or armed: the LEAD plays its solo sound */
const char *da_style_name(int i);
const char *da_scale_name(int i);
const char *da_note_name(int pc);
const char *da_arc_name(int phase);
void da_loop_name(const da_settings *s, int loop, char *out, int max);   /* "LOOP 4" */
int da_snap(const da_settings *s, int note);       /* a note onto the scale (the nearest step, ties down) */

#endif
