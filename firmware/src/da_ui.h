/* SPDX-License-Identifier: GPL-3.0-only */
/* DAURADE screen and controls: Daurade's pages on Felucca's drawing (gfx.c: canvas, fonts, palettes, icons), in
 * Tuna's frame: the header, four knob cards, the panel, the footer. Compiled after gfx.c in one unit, as Felucca's
 * ui*.c are (the firmware's unity build, or a host test).
 *
 * The controls
 *   SELECT     tempo                                  MASTER     volume (Felucca's)
 *   PRESETS    the style (the list shows while turning; playing, it comes in on the next bar)
 *   ALGORITHM  a new song in the same style (on the next bar)
 *   KNOB 1..4  the four cards of the page (PLAY: SWEEP PUMP VOICE ENERGY)
 *   PLAY       start (with the intro, ARC on) / stop
 *   REC        DROP: tapped, everything but the filtered loop out for two bars, back on the bar; held, until let go
 *   SEQ        SOLO: a solo from the next beat (SOLO's bars), again: stop it; shows the SOLO page
 *   keys       the lead, live, snapped to the scale. Music only, never settings
 *   OCT- OCT+  the keys' octave
 *   HOME       PLAY; held: MENU (colour, look)
 *   FX         SOUND, again: TONE           LFO   VOICE              SCL   KEY
 *   ARP        GROOVE                       EDIT  DRUMS              GLO   MIX, again: SPACE
 *   ENV        ARC                          SAVE  SETS (eight sets on the knobs) */
#ifndef DA_UI_H
#define DA_UI_H

#include <stdbool.h>
#include <stdint.h>
#include "da_groove.h"

enum { DU_K_MASTER, DU_K_SELECT, DU_K_PRESETS, DU_K_ALGO, DU_K1, DU_K2, DU_K3, DU_K4, DU_KNOBS };
enum {
    DU_B_FX, DU_B_SCL, DU_B_ENV, DU_B_LFO, DU_B_EDIT, DU_B_GLO,
    DU_B_HOME, DU_B_SAVE, DU_B_ARP, DU_B_SEQ, DU_B_PLAY, DU_B_REC,
    DU_B_OCTDN, DU_B_OCTUP, DU_BUTTONS
};
enum { DU_PG_PLAY, DU_PG_SOUND, DU_PG_TONE, DU_PG_VOICE, DU_PG_KEY, DU_PG_GROOVE, DU_PG_DRUMS, DU_PG_MIX,
       DU_PG_SPACE, DU_PG_ARC, DU_PG_SOLO, DU_PG_SETS, DU_PG_MENU, DU_PAGES };
#define DU_KEY_LO 53                      /* F3: the FM-1's lowest key */
#define DU_KEY_HI 79                      /* G5 */

typedef struct {
    da_groove *g;
    int page;
    int hot, hot_ms;                      /* the knob just turned (its card in the accent); 9: SELECT */
    int browse_ms;                        /* the style list over the panel (PRESETS) */
    char msg[32];
    int msg_ms;
    int home_ms;
    bool home_down;
    uint32_t keys_down;                   /* bit (note - DU_KEY_LO) */
    int oct;                              /* OCT- OCT+: the keys' octave, -2 .. 2 */
    int key_sent[27];                     /* the note each key sent (with the octave it was pressed in) */
    /* SETS */
    da_settings sets[DA_SETS];
    uint8_t set_used;                     /* bit per set */
    int set_sel;
    int clear_turns, clear_ms;            /* CLEAR: three turns right within a second */
    bool sets_changed;                    /* the glue saves the sets while stopped */
    uint32_t frame;
    int batt;                             /* 0..3 bars, 4 = USB power */
    bool changed;                         /* a setting changed: the glue applies it (sound, mix) and saves it later */
    uint32_t sig_head, sig_card[4], sig_foot;
    bool force;
} da_ui;

void du_init(da_ui *u, da_groove *g);
void du_knob(da_ui *u, int knob, int delta);
void du_button(da_ui *u, int button, bool down);
void du_key(da_ui *u, int note, bool down);
void du_tick(da_ui *u, int ms);           /* timers; every frame */
void du_draw(da_ui *u);                   /* what changed; the panel every frame */
void du_draw_all(da_ui *u);
const char *du_page_name(int page);

#endif
