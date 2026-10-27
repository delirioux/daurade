/* SPDX-License-Identifier: GPL-3.0-only */
/* DAURADE's screen and controls on the host: firmware/src/da_ui.c drawn by Felucca's gfx.c into a 240 x 240 frame,
 * the groove running (its notes counted, no sound), the controls driven as a player would, every page in every
 * palette written out, and the layout lint (from Tuna's and Flyfish's UI tests): every text inside its canvas, not
 * cut, not ellipsised unless it is a name, no two texts of one canvas overlapping.
 *   tests/da_ui_test.sh [OUTDIR]     (exit 0: all passed; PNGs and a GIF of PLAY under OUTDIR) */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, checks;
#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        checks++;                                                          \
        if (!(cond)) {                                                     \
            fails++;                                                       \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                    \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
        }                                                                  \
    } while (0)

/* ---- the layout lint: the ink boxes of one canvas, checked when it is blitted -------------------------- */
typedef struct { int x0, y0, x1, y1, flags; char s[24]; } box;
static box boxes[256];
static int nbox;
static const char *lint_where = "";
static int lint_findings;
static void hook_text(int x0, int y0, int x1, int y1, const char *s, int flags)
{
    if (nbox < 256) {
        box *b = &boxes[nbox++];
        b->x0 = x0; b->y0 = y0; b->x1 = x1; b->y1 = y1; b->flags = flags;
        strncpy(b->s, s, sizeof b->s - 1);
        b->s[sizeof b->s - 1] = 0;
    }
}
#define GFX_HOOK_TEXT(x0, y0, x1, y1, s, flags) hook_text((x0), (y0), (x1), (y1), (s), (flags))
#define GFX_HOOK_BLIT(x, y, r0) ((void)0)
#define GFX_HOOK_BEGIN() (nbox = 0)
#define GFX_HOOK_PIXELS(n) ((void)0)

static uint16_t fb[240 * 240];
static void lint_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    for (int i = 0; i < nbox; ++i) {
        box *b = &boxes[i];
        if (b->x1 <= b->x0)
            continue;
        bool name = (b->flags & 8) != 0;            /* free text (a name, a message) may be ellipsised */
        if (b->x0 < 0 || b->y0 < 0 || b->x1 > (int)w || b->y1 > (int)h) {
            printf("LINT %s: \"%s\" leaves its canvas (%d,%d..%d,%d of %ux%u at %u,%u)\n", lint_where, b->s, b->x0,
                   b->y0, b->x1, b->y1, w, h, x, y);
            lint_findings++;
        }
        if ((int)x + b->x1 > 240 || (int)y + b->y1 > 240) {
            printf("LINT %s: \"%s\" off the screen\n", lint_where, b->s);
            lint_findings++;
        }
        if ((b->flags & 1) && !name) {
            printf("LINT %s: \"%s\" ellipsised\n", lint_where, b->s);
            lint_findings++;
        }
        for (int j = i + 1; j < nbox; ++j) {
            box *c = &boxes[j];
            if (c->x1 <= c->x0)
                continue;
            if (b->x0 < c->x1 && c->x0 < b->x1 && b->y0 < c->y1 && c->y0 < b->y1) {
                printf("LINT %s: \"%s\" overlaps \"%s\" (%d,%d..%d,%d / %d,%d..%d,%d)\n", lint_where, b->s, c->s, b->x0,
                       b->y0, b->x1, b->y1, c->x0, c->y0, c->x1, c->y1);
                lint_findings++;
            }
        }
    }
    nbox = 0;
}
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    lint_blit(x, y, w, h);
    for (uint32_t j = 0; j < h && y + j < 240; j++)
        for (uint32_t i = 0; i < w && x + i < 240; i++)
            fb[(y + j) * 240 + x + i] = p[j * w + i];
}
#define main hostsim_main
#include "hostsim.c"                               /* (core.h, the matrix: da_sounds.h's track settings) */
#undef main
#include "gfx.c"
#include "da_groove.h"
#include "da_sounds.h"
#include "da_ui.c"

static const char *outdir;
static int frames_written;
static void write_ppm(const char *name)
{
    if (!outdir) return;
    char path[512];
    snprintf(path, sizeof path, "%s/ppm/%s.ppm", outdir, name);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6 240 240 255\n");
    for (int i = 0; i < 240 * 240; i++) {
        uint16_t c = (uint16_t)((fb[i] >> 8) | (fb[i] << 8));
        uint8_t px[3] = {(uint8_t)((c >> 11) << 3 | (c >> 13)), (uint8_t)(((c >> 5) & 63) << 2 | ((c >> 9) & 3)),
                         (uint8_t)((c & 31) << 3 | ((c >> 2) & 7))};
        fwrite(px, 1, 3, f);
    }
    fclose(f);
    frames_written++;
}

static int notes_on, notes_off;
static int part_on[DA_PARTS];
static void sound(void *ctx, const da_event *e)
{
    (void)ctx;
    if (e->on) { notes_on++; part_on[e->part % DA_PARTS]++; }
    else notes_off++;
}

static da_groove g;
static da_ui ui;
static void run_ms(int ms)
{
    for (int t = 0; t < ms; t += 16) {
        da_process(&g, 44100 * 16 / 1000);
        du_tick(&ui, 16);
        du_draw(&ui);
    }
}
static void press(int b) { du_button(&ui, b, true); du_button(&ui, b, false); }
static void shot(const char *name)
{
    lint_where = name;
    du_draw_all(&ui);
    write_ppm(name);
}
static void all_palettes(const char *page)
{
    int keep = g.s.palette, keep_st = g.s.look;
    for (int p = 0; p < (int)NPALETTES; ++p)
        for (int st = 0; st < 2; ++st) {
            char name[64];
            g.s.palette = (uint8_t)p;
            g.s.look = (uint8_t)st;
            du_apply_style(&ui);
            snprintf(name, sizeof name, "%s%s_%s", st ? "LINE_" : "", UI_PALETTES[p].name, page);
            shot(name);
        }
    g.s.palette = (uint8_t)keep;
    g.s.look = (uint8_t)keep_st;
    du_apply_style(&ui);
}
static void bars(int n)                            /* play until n bars on */
{
    int b = g.bar + n;
    for (int k = 0; k < 4000 && g.bar < b; ++k)
        run_ms(16);
}

int main(int argc, char **argv)
{
    outdir = argc > 1 ? argv[1] : 0;
    da_init(&g, 44100.0f);
    da_set_sound(&g, sound, 0);
    du_init(&ui, &g);
    CHECK(g.s.palette == UI_NEBULA_INDEX && !strcmp(UI_PALETTES[UI_NEBULA_INDEX].name, "NEBULA"), "NEBULA by default");
    CHECK(g.s.style == DA_ST_FILTER_HOUSE, "FILTER HOUSE first");
    shot("01_play_stopped");
    all_palettes("play_stopped");
    press(DU_B_PLAY);
    CHECK(g.playing, "PLAY plays");
    bars(9);
    CHECK(g.arc_phase == DA_ARC_MAIN, "the intro over at bar 8 (%s at %d)", da_arc_name(g.arc_phase), g.bar);
    CHECK(part_on[DA_STABS] && part_on[DA_BASS] && part_on[DA_DRUMS], "the parts play (%d %d %d %d)", part_on[0],
          part_on[1], part_on[2], part_on[3]);
    shot("02_play");
    all_palettes("play");
    /* the PLAY animation: a bar, a frame every 48 ms */
    for (int f = 0; f < 40; ++f) {
        char nm[32];
        run_ms(48);
        snprintf(nm, sizeof nm, "anim_%03d", f);
        lint_where = nm;
        write_ppm(nm);
    }
    /* the knobs of PLAY */
    du_knob(&ui, DU_K1, 4);
    CHECK(g.s.sweep == 82, "SWEEP +4 steps of 5 (%d)", g.s.sweep);
    du_knob(&ui, DU_K1, -4);
    /* DROP: tapped, back two bars on */
    du_button(&ui, DU_B_REC, true);
    CHECK(g.drop, "REC: DROP");
    run_ms(64);
    du_button(&ui, DU_B_REC, false);
    shot("03_drop");
    all_palettes("drop");
    int back = g.drop_end;
    bars(3);
    CHECK(!g.drop && g.bar >= back, "back on the bar (%d, %d)", g.bar, back);
    /* the styles: PRESETS queues one for the next bar */
    du_knob(&ui, DU_K_PRESETS, 5);
    CHECK(g.next_style == DA_ST_DISCO, "PRESETS +5: DISCO queued (%d)", g.next_style);
    shot("04_styles");
    all_palettes("styles");
    bars(1);
    CHECK(g.s.style == DA_ST_DISCO && g.s.bpm == 116, "DISCO on the bar line (%d, %d BPM)", g.s.style, g.s.bpm);
    run_ms(2000);
    /* SOUND, TONE */
    press(DU_B_FX);
    CHECK(ui.page == DU_PG_SOUND, "FX: SOUND");
    bars(1);
    shot("05_sound");
    all_palettes("sound");
    du_knob(&ui, DU_K1, 1);
    CHECK(g.s.snd[DA_STABS] == 7, "K1: the next STABS sound (%d)", g.s.snd[DA_STABS]);
    du_knob(&ui, DU_K1, -1);
    press(DU_B_FX);
    CHECK(ui.page == DU_PG_TONE, "FX again: TONE");
    shot("06_tone");
    all_palettes("tone");
    /* ROBOT POP for VOICE */
    da_queue_style(&g, DA_ST_ROBOT_POP);
    bars(2);
    press(DU_B_LFO);
    CHECK(ui.page == DU_PG_VOICE, "LFO: VOICE");
    run_ms(300);
    shot("07_voice");
    all_palettes("voice");
    /* KEY */
    da_queue_style(&g, DA_ST_FILTER_HOUSE);
    bars(1);
    press(DU_B_SCL);
    CHECK(ui.page == DU_PG_KEY, "SCL: KEY");
    shot("08_key");
    all_palettes("key");
    du_knob(&ui, DU_K1, 1);
    CHECK(g.s.key == 7 && g.loop[0].root == 7, "KEY G: the loop follows (%d)", g.loop[0].root);
    du_knob(&ui, DU_K1, -1);
    /* GROOVE, DRUMS */
    press(DU_B_ARP);
    bars(1);
    CHECK(ui.page == DU_PG_GROOVE, "ARP: GROOVE");
    shot("09_groove");
    all_palettes("groove");
    press(DU_B_EDIT);
    CHECK(ui.page == DU_PG_DRUMS, "EDIT: DRUMS");
    shot("10_drums");
    all_palettes("drums");
    /* MIX, SPACE */
    press(DU_B_GLO);
    CHECK(ui.page == DU_PG_MIX, "GLO: MIX");
    shot("11_mix");
    all_palettes("mix");
    press(DU_B_GLO);
    CHECK(ui.page == DU_PG_SPACE, "GLO again: SPACE");
    shot("12_space");
    all_palettes("space");
    /* ARC */
    press(DU_B_ENV);
    CHECK(ui.page == DU_PG_ARC, "ENV: ARC");
    shot("13_arc");
    all_palettes("arc");
    /* SOLO */
    da_queue_style(&g, DA_ST_SHRED);
    bars(1);
    press(DU_B_SEQ);
    CHECK(ui.page == DU_PG_SOLO && da_soloing(&g), "SEQ: a solo (%d)", da_soloing(&g));
    run_ms(600);
    CHECK(g.solo_left > 0, "the solo plays from the next beat");
    shot("14_solo");
    all_palettes("solo");
    press(DU_B_SEQ);
    CHECK(!da_soloing(&g), "SEQ again: the solo stops");
    /* SETS: store into 3, load it back */
    da_queue_style(&g, DA_ST_FILTER_HOUSE);
    bars(1);
    press(DU_B_SAVE);
    CHECK(ui.page == DU_PG_SETS, "SAVE: SETS");
    du_knob(&ui, DU_K1, 2);
    du_knob(&ui, DU_K3, 1);
    CHECK((ui.set_used & 4u) && ui.sets[2].style == DA_ST_FILTER_HOUSE, "STORE into set 3");
    ui.sets[0] = g.s; ui.sets[0].style = DA_ST_DISCO; ui.set_used |= 1u;
    ui.sets[4] = g.s; ui.sets[4].style = DA_ST_SEQUENCER; ui.set_used |= 16u;
    ui.sets[6] = g.s; ui.sets[6].style = DA_ST_SHRED; ui.set_used |= 64u;
    shot("15_sets");
    all_palettes("sets");
    du_knob(&ui, DU_K1, -2);
    du_knob(&ui, DU_K2, 1);
    CHECK(g.s.style == DA_ST_DISCO, "LOAD set 1: DISCO at once");
    du_knob(&ui, DU_K4, 1); du_knob(&ui, DU_K4, 1);
    CHECK(ui.set_used & 1u, "CLEAR twice: not yet");
    du_knob(&ui, DU_K4, 1);
    CHECK(!(ui.set_used & 1u), "CLEAR three times: cleared");
    /* the keys: the lead, snapped to the scale */
    int before = part_on[DA_LEAD];
    du_key(&ui, 54, true);
    CHECK(part_on[DA_LEAD] == before + 1, "a key plays the lead");
    du_key(&ui, 54, false);
    /* MENU: HOME held */
    du_button(&ui, DU_B_HOME, true);
    run_ms(700);
    du_button(&ui, DU_B_HOME, false);
    CHECK(ui.page == DU_PG_MENU, "HOME held: MENU");
    shot("16_menu");
    all_palettes("menu");
    press(DU_B_HOME);
    CHECK(ui.page == DU_PG_PLAY, "HOME: PLAY");
    /* ALGORITHM: a new song on the next bar */
    int song = g.s.song;
    du_knob(&ui, DU_K_ALGO, 1);
    bars(1);
    CHECK(g.s.song == song + 1, "ALGORITHM: the next song (%d)", g.s.song);
    /* stopped again */
    press(DU_B_PLAY);
    CHECK(!g.playing, "PLAY stops");
    CHECK(g.nheld == 0, "nothing left sounding");
    CHECK(lint_findings == 0, "the layout lint: %d findings", lint_findings);
    printf("da_ui: %d checks, %d failed, %d frames, %d notes on (stabs %d, bass %d, lead %d, drums %d)\n", checks, fails,
           frames_written, notes_on, part_on[0], part_on[1], part_on[2], part_on[3]);
    return fails ? 1 : 0;
}
