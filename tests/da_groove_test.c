/* SPDX-License-Identifier: GPL-3.0-only */
/* DAURADE's groove engine on its own (firmware/src/da_groove.c): every style plays, in time, in key, every note
 * ends; a song replays exactly; a style and a song come in on the bar line; DROP, SOLO and the keys do what the
 * manual says; the settings survive a bad record.
 *   cc -std=c11 -O2 -Ifirmware/src -o build/host/da_groove_test tests/da_groove_test.c firmware/src/da_groove.c -lm */
#include <stdio.h>
#include <string.h>
#include "da_groove.h"

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

#define SR 44100.0f
static da_groove g;
static int on[DA_PARTS], off[DA_PARTS], sounding[DA_PARTS][128], out_of_key, bad_pc;
static uint32_t hash;
static int syl_prev = -1, syl_bad, syl_resets, syl_notes;
static void sound(void *ctx, const da_event *e)
{
    (void)ctx;
    hash = (hash ^ (uint32_t)(e->on << 24 | e->part << 16 | e->note << 8 | e->vel)) * 16777619u + g.clock;
    if (e->on) {
        on[e->part]++;
        sounding[e->part][e->note]++;
        if (e->part != DA_DRUMS && e->part != DA_LEAD && da_snap(&g.s, e->note) != e->note) {
            /* (STABS and BASS: in the scale but for the dominant's raised third and the bass's approach note) */
            bad_pc++;
        }
        if (e->part == DA_LEAD && g.solo_left > 0 && da_snap(&g.s, e->note) != e->note)
            out_of_key++;
        if (e->part == DA_LEAD) {                  /* PHRASE: each lead note the next vowel, or the phrase again */
            int k = g.lead_syl_now;
            if (k == 0 && syl_prev >= 0) syl_resets++;
            else if (syl_prev >= 0 && k != (syl_prev + 1) % DA_PHRASE_LEN) syl_bad++;
            syl_prev = k;
            syl_notes++;
        }
    } else {
        off[e->part]++;
        if (sounding[e->part][e->note] > 0) sounding[e->part][e->note]--;
    }
}
static void reset_counts(void) { memset(on, 0, sizeof on); memset(off, 0, sizeof off); memset(sounding, 0, sizeof sounding); }
static int still_sounding(int p) { int n = 0; for (int k = 0; k < 128; ++k) n += sounding[p][k]; return n; }
static void run_bars(int n)
{
    int b = g.bar + n;
    while (g.bar < b) da_process(&g, 32);
}

int main(void)
{
    da_init(&g, SR);
    da_set_sound(&g, sound, 0);
    /* every style: 40 bars with the arc, its parts play, nothing hangs */
    for (int st = 0; st < DA_STYLES; ++st) {
        da_style_apply(&g.s, st);
        da_rebuild(&g);
        reset_counts();
        bad_pc = 0;
        da_play(&g, true);
        run_bars(40);
        da_play(&g, false);
        for (int p = 0; p < DA_PARTS; ++p) {
            CHECK(on[p] > 0 || (p == DA_LEAD && g.s.voice == 0), "%s: part %d plays (%d)", da_style_name(st), p, on[p]);
            if (p != DA_DRUMS)
                CHECK(!still_sounding(p), "%s: part %d left %d notes sounding", da_style_name(st), p, still_sounding(p));
        }
        CHECK(bad_pc < on[DA_STABS] / 8 + 8, "%s: %d stab and bass notes outside the scale", da_style_name(st), bad_pc);
        CHECK(g.loop[0].name[0], "%s: its first chord has a name (%s)", da_style_name(st), g.loop[0].name);
    }
    /* SLOW JAM sings the phrase: a vowel per lead note, in order, from the first at each hook */
    da_style_apply(&g.s, DA_ST_SLOW_JAM);
    CHECK(g.s.vowels == DA_VOWELS_PHRASE, "SLOW JAM: VOWELS is PHRASE");
    g.s.arc = 0;
    da_rebuild(&g);
    syl_prev = -1; syl_bad = syl_resets = syl_notes = 0;
    da_play(&g, true);
    run_bars(16);
    da_play(&g, false);
    CHECK(syl_notes > 16 && syl_bad == 0 && syl_resets > 0, "the phrase in order (%d notes, %d out of order, %d hooks)",
          syl_notes, syl_bad, syl_resets);
    g.s.arc = 1;
    /* the same song twice: the same notes at the same times */
    uint32_t h1, h2;
    da_style_apply(&g.s, DA_ST_FUNK);
    g.s.song = 17;
    da_rebuild(&g);
    hash = 2166136261u; da_play(&g, true); run_bars(12); da_play(&g, false); h1 = hash;
    hash = 2166136261u; da_play(&g, true); run_bars(12); da_play(&g, false); h2 = hash;
    CHECK(h1 == h2, "a song replays exactly");
    g.s.song = 18;
    da_rebuild(&g);
    hash = 2166136261u; da_play(&g, true); run_bars(12); da_play(&g, false);
    CHECK(hash != h1, "another song differs");
    /* the tempo: 16 bars at 120 BPM are 32 s */
    da_style_apply(&g.s, DA_ST_FILTER_HOUSE);
    g.s.bpm = 120;
    da_rebuild(&g);
    da_play(&g, true);
    run_bars(16);
    float secs = (float)g.clock / SR;
    CHECK(secs > 31.9f && secs < 32.1f, "16 bars at 120 BPM: %.3f s", (double)secs);
    /* a style queued: on the next bar line, not before */
    int bar = g.bar;
    while (g.step != 5) da_process(&g, 32);
    da_queue_style(&g, DA_ST_DISCO);
    CHECK(g.s.style == DA_ST_FILTER_HOUSE, "queued: still FILTER HOUSE mid-bar");
    while (g.bar == bar) da_process(&g, 32);
    CHECK(g.s.style == DA_ST_DISCO && g.step == 0, "DISCO from the bar line (step %d)", g.step);
    /* a song queued: the same */
    int song = g.s.song;
    da_queue_song(&g, song + 3);
    CHECK(g.s.song == song, "the song waits for the bar");
    run_bars(1);
    CHECK(g.s.song == song + 3, "the next song from the bar line");
    /* DROP: everything but the loop out, back two bars on */
    reset_counts();
    da_drop(&g, true);
    da_drop(&g, false);
    int drop_bar = g.bar;
    run_bars(1);
    CHECK(g.drop && on[DA_DRUMS] == 0 && on[DA_BASS] == 0, "dropped: no drums, no bass (%d %d)", on[DA_DRUMS], on[DA_BASS]);
    CHECK(on[DA_STABS] > 0, "the loop plays on (%d)", on[DA_STABS]);
    run_bars(2);
    CHECK(!g.drop && g.bar >= drop_bar + 2, "back after two bars");
    /* held: stays dropped until let go */
    da_drop(&g, true);
    run_bars(4);
    CHECK(g.drop, "held: still dropped after four bars");
    da_drop(&g, false);
    run_bars(2);
    CHECK(!g.drop, "let go: back on the next bar line");
    /* SOLO: from the next beat, in the scale, its bars long */
    g.s.arc = 0;
    out_of_key = 0;
    da_solo(&g);
    CHECK(da_soloing(&g) && g.solo_wait >= 0, "SEQ arms a solo");
    run_bars(1);
    CHECK(g.solo_left > 0, "the solo plays");
    run_bars(g.s.solo_bars + 1);
    CHECK(!da_soloing(&g), "the solo ends after its bars");
    CHECK(out_of_key == 0, "the solo stays in the scale (%d out)", out_of_key);
    /* the keys: snapped to the scale, the hook makes way */
    reset_counts();
    da_key(&g, 1, 54, true);
    CHECK(on[DA_LEAD] == 1, "a key plays the lead");
    da_key(&g, 1, 54, false);
    CHECK(off[DA_LEAD] >= 1, "its release lets go");
    da_play(&g, false);
    /* a bad record: every field back in range */
    da_settings s;
    memset(&s, 0xEE, sizeof s);
    da_settings_fix(&s);
    CHECK(s.style < DA_STYLES && s.sweep <= 100 && s.bars == 4 && s.scale < DA_SCALES && s.bpm >= 60 && s.bpm <= 200,
          "a bad record fixed");
    /* the scale snap: F# DORIAN has no G */
    da_style_apply(&s, DA_ST_FILTER_HOUSE);
    CHECK(da_snap(&s, 67) == 66 || da_snap(&s, 67) == 68, "G onto F# Dorian: %d", da_snap(&s, 67));
    printf("da_groove: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
