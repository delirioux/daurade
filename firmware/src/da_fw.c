/* SPDX-License-Identifier: GPL-3.0-only */
/* DAURADE on the FM-1: the glue between Daurade (the groove engine and its screen) and Felucca's firmware (the
 * engines, voices and effects, the panel, the screen driver, USB MIDI, the flash). Included by felucca.c after
 * Felucca's own sources when FELUCCA_DAURADE is 1; the groove engine (da_groove.c) and the maths (tuna_libm.c, from
 * Tuna) are their own translation units, linked beside felucca.o (tools/build.py). Shaped as Flyfish's ff_fw.c.
 *
 * Who runs what:
 *   the audio ISR (audio.c audio_block -> da_audio_pre): the groove in time, its notes straight into Felucca's
 *     tracks (trk_note_on / off) and out as USB MIDI; the sweep and the pump into the tracks (das_block);
 *   the main loop (main.c -> da_frame, da_input): the panel into the controls (the audio ISR held off while one
 *     acts), a part's sound loaded when it changed (a preset load is the main loop's), the cards into the tracks,
 *     the LEDs, the settings and sets saved while stopped, the screen.
 * Tracks: 1 STABS, 2 BASS, 3 LEAD, 4 DRUMS (da_sounds.h). */
#include "da_groove.h"
#include "da_sounds.h"
#include "da_ui.h"
#include "da_ui.c"
#include "da_version.h"

static da_groove da_g;
static da_ui da_u;

/* what the tracks were last set to */
static da_settings da_applied;
static bool da_applied_ok;
static uint8_t da_loaded_sig = 0xFF;
static int8_t da_loaded_snd[DA_PARTS] = {-1, -1, -1, -1};
static uint8_t da_loaded_solo = 0xFF;

/* ---- the audio ISR --------------------------------------------------------------------------------------------- */
static void da_sound(void *ctx, const da_event *e)
{
    (void)ctx;
    uint32_t p = e->part % DA_PARTS;
    int n = p == DA_DRUMS ? das_drum_note(&da_g.s, e->note) : e->note;
    if (e->on)
        trk_note_on(&trk[p], (uint32_t)n, e->vel);
    else
        trk_note_off(&trk[p], (uint32_t)n);
    /* MIDI out: STABS on channel 1, BASS 2, LEAD 3, the drums 10 (General MIDI's) */
    uint32_t ch = p == DA_DRUMS ? 9u : p, st = (e->on ? 0x90u : 0x80u) | ch, cin = st >> 4;
    int mn = p == DA_DRUMS ? (int)(e->note == DA_DR_KICK ? 36 : e->note == DA_DR_SNARE ? 38 : e->note == DA_DR_CLAP ? 39
                                   : e->note == DA_DR_HAT ? 42 : e->note == DA_DR_OPEN ? 46 : 49) : n;
    midi_out_event(cin | st << 8 | (uint32_t)mn << 16 | (uint32_t)(e->on ? e->vel : 0u) << 24);
}

static void da_audio_pre(uint32_t n)
{
    da_process(&da_g, (int)n);
    das_block(&da_g);
}

static void da_audio_post(int32_t *out, uint32_t n)
{
    (void)out;
    (void)n;
}

/* ---- the settings: one record in Felucca's first project object, the sets in the second; saved only while stopped
 * (the flash is the code's: XIP, writing it while playing would stall the audio) ---- */
#define DA_REC_MAGIC 0x31534144u                             /* "DAS1" */
#define DA_SETS_MAGIC 0x31544544u                            /* "DET1" */
typedef struct { uint32_t magic; uint16_t size, ver; da_settings s; } da_record;
typedef struct { uint32_t magic; uint16_t size; uint8_t used, pad; da_settings s[DA_SETS]; } da_sets_rec;
static uint32_t da_save_ms;
static bool da_dirty;
static void da_settings_load(void)
{
#if FELUCCA_FLASH
    da_record r;
    memset(&r, 0, sizeof r);
    int n = st_load(OBJ_PROJECT0, &r, sizeof r);
    if (n >= (int)(sizeof r - sizeof r.s) && r.magic == DA_REC_MAGIC) {
        da_settings s;
        da_defaults(&s);                                     /* (a shorter, older record: the new fields their defaults) */
        uint32_t k = r.size < sizeof s ? r.size : sizeof s;
        memcpy(&s, &r.s, k);
        da_settings_fix(&s);
        da_g.s = s;
    }
#endif
}
static void da_settings_save(void)
{
#if FELUCCA_FLASH
    da_record r;
    memset(&r, 0, sizeof r);
    r.magic = DA_REC_MAGIC;
    r.size = (uint16_t)sizeof r.s;
    r.ver = 1;
    r.s = da_g.s;
    st_save(OBJ_PROJECT0, &r, sizeof r);
#endif
}
static void da_sets_load(void)
{
#if FELUCCA_FLASH
    static da_sets_rec r;                                    /* (static: 8 records, off the stack) */
    memset(&r, 0, sizeof r);
    if (st_load(OBJ_PROJECT0 + 1u, &r, sizeof r) == (int)sizeof r && r.magic == DA_SETS_MAGIC && r.size == sizeof r.s[0]) {
        for (int i = 0; i < DA_SETS; ++i) {
            da_u.sets[i] = r.s[i];
            da_settings_fix(&da_u.sets[i]);
        }
        da_u.set_used = r.used;
    }
#endif
}
static void da_sets_save(void)
{
#if FELUCCA_FLASH
    static da_sets_rec r;
    memset(&r, 0, sizeof r);
    r.magic = DA_SETS_MAGIC;
    r.size = (uint16_t)sizeof r.s[0];
    r.used = da_u.set_used;
    memcpy(r.s, da_u.sets, sizeof r.s);
    st_save(OBJ_PROJECT0 + 1u, &r, sizeof r);
#endif
}

/* ---- the sounds into the tracks (main loop) ---------------------------------------------------------------------- */
/* a sound of Daurade's own over the factory preset just loaded (as ui.c apply_preset_to puts a preset in) */
static void da_apply_own(int16_t *p, const preset_t *pr)
{
    static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
    for (int i = 0; i < 8; i++)
        p[P_E0 + i] = pr->e[i];
    p[P_ATK] = pr->env[0];
    p[P_DEC] = pr->env[1];
    p[P_SUS] = pr->env[2];
    p[P_REL] = pr->env[3];
    p[P_ED_FLT] = pr->fenv;
    p[P_VOICE] = pr->mono ? V_LEGATO : V_POLY;
    for (int i = 0; i < 4; i++)
        p[P_DIST + i] = (int16_t)(pr->fx[i] ? pr->fx[i] - 1 : FX_DEF[i]);
}
/* one of Tuna's FM6 patches as the track's own (SLOT OWN), so nothing reloads a factory one over it (eng_fm6.c) */
static void da_fm6_load(track_t *t, const uint8_t *packed)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK;
    uint8_t v[FP_SIZE + 1u];
    fm6_unpack(packed, v);
    fm6_set_patch(tr, v);
    t->p[P_E7] = FM6_OWN;
    fm6_slot[tr] = FM6_OWN;
    fm6_own_ok &= (uint8_t)~(1u << tr);
}
static void da_load_part(uint32_t k)
{
    undo_depth++;                                            /* (no undo copy of Daurade's loads) */
    if (k == DA_DRUMS) {
        set_engine_of(&trk[k], das_kit_engine(da_g.s.snd[3]));
        apply_preset_to(&trk[k], 0u);
        undo_depth--;
        return;
    }
    const das_sound *snd = das_of(&da_g, (int)k);
    set_engine_of(&trk[k], snd->engine);
    apply_preset_to(&trk[k], snd->preset);
    undo_depth--;
    fm1_irq_off();
    if (snd->own)
        da_apply_own(trk[k].p, snd->own);
    if (snd->fm6)
        da_fm6_load(&trk[k], snd->fm6);
    fm1_irq_on();
}

static void da_apply(void)
{
    const da_settings *s = &da_g.s;
    bool all = !da_applied_ok, every = all || da_loaded_sig != da_g.sound_sig;   /* (a style or a set: every part) */
    uint8_t solo = (uint8_t)da_soloing(&da_g);
    for (uint32_t k = 0; k < DA_PARTS; ++k)                  /* a sound changed (a card, a style, a solo): load it */
        if (every || da_loaded_snd[k] != (int8_t)s->snd[k] || (k == DA_LEAD && solo != da_loaded_solo)) {
            da_load_part(k);
            da_loaded_snd[k] = (int8_t)s->snd[k];
            all = true;
        }
    da_loaded_solo = solo;
    da_loaded_sig = da_g.sound_sig;
    if (all || memcmp(s, &da_applied, sizeof *s)) {
        fm1_irq_off();
        for (uint32_t k = 0; k < DA_PARTS; ++k)
            das_track(&trk[k], &da_g, (int)k);
        das_song(s);
        fm1_irq_on();
    }
    if (s->palette != settings.palette % NPALETTES) {        /* (the menu's colour, kept as Felucca keeps its own) */
        settings.palette = s->palette;
        settings_save();
    }
    da_applied = *s;
    da_applied_ok = true;
}

/* ---- the power-on screen: Daurade's logo (tools/da_logo.py: 176 px, 8 greys, rows run-length coded) and the
 * Felucca it runs on; main.c holds it 0.75 s ---- */
#include "da_logo.inc"
#define DA_SPLASH_MS 750u
static void da_splash(void)
{
    uint16_t row[2][DA_LOGO_SIZE];                           /* (two: one fills while the other goes out by DMA) */
    uint32_t y, x0 = (240u - DA_LOGO_SIZE) / 2u, y0 = 16u;
    char line[40] = "Daurade " DA_VERSION " on Felucca ";
    uint32_t k = sizeof "Daurade " DA_VERSION " on Felucca " - 1u;
    lcd_fill(0, 0, 240, 240, DA_LOGO_BG);
    for (y = 0; y < DA_LOGO_SIZE; y++) {
        const uint8_t *p = DA_LOGO_RLE + DA_LOGO_ROW[y];
        uint16_t *r = row[y & 1u];
        uint32_t x = 0;
        while (x < DA_LOGO_SIZE) {
            uint32_t n = (*p >> 3) + 1u, c = DA_LOGO_PAL[*p & 7u];
            p++;
            while (n-- && x < DA_LOGO_SIZE)
                r[x++] = (uint16_t)c;
        }
        lcd_blit(x0, y0 + y, DA_LOGO_SIZE, 1, r);
    }
    for (const char *v = FELUCCA_VERSION; *v && k < sizeof line - 1u; v++)
        line[k++] = *v;
    line[k] = 0;
    draw_text_line(0, y0 + DA_LOGO_SIZE + 18u, 240, &AF_S, line, 0xA535u, DA_LOGO_BG, 1);   /* (grey, 160 165 170) */
    lcd_sync();
}

/* ---- start ------------------------------------------------------------------------------------------------------ */
static void da_fw_init(void)
{
    uint32_t i;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    undo_depth++;
    fm6_init();
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        set_engine_of(t, i == 3u ? das_kit_engine(DA_KIT_CRISP) : DAS_ANALOG);
        apply_preset_to(t, 0u);
        t->engine = t->eng_req;
        track_defaults_steps(t);                             /* (Felucca's sequencer stays empty) */
    }
    undo_depth--;
    song.sel = 0;
    song.master_q12 = 2048;
    da_init(&da_g, (float)FS);
    da_settings_load();
    da_g.s.palette = (uint8_t)(settings.palette % NPALETTES);
    da_rebuild(&da_g);
    da_set_sound(&da_g, da_sound, 0);
    du_init(&da_u, &da_g);
    da_sets_load();
    da_apply();
}

/* ---- the main loop ----------------------------------------------------------------------------------------------- */
static uint32_t da_notes_prev;
static void da_input(void)
{
    uint32_t released = 0, pressed = fm1_input_edges(&released), b, k;
    static const uint8_t KNOB[NE] = {DU_K_SELECT, DU_K_ALGO, DU_K_PRESETS, DU_K1, DU_K2, DU_K3, DU_K4};
    fm1_input_note_edges();                                  /* (the keys: from their state below) */
    for (b = 0; b < NB; b++) {                               /* (Felucca's B_* are Daurade's DU_B_*, in the same order) */
        uint32_t bit = 1u << panel.btn[b];
        if (pressed & bit) {
            fm1_irq_off();
            du_button(&da_u, (int)b, true);
            fm1_irq_on();
        }
        if (released & bit) {
            fm1_irq_off();
            du_button(&da_u, (int)b, false);
            fm1_irq_on();
        }
    }
    for (k = 0; k < NE; k++) {
        int32_t s = panel_enc(k);
        if (s) {
            fm1_irq_off();
            du_knob(&da_u, KNOB[k], s);
            fm1_irq_on();
        }
    }
    uint32_t notes = fm1_in.notes, ch = notes ^ da_notes_prev;
    for (k = 0; ch && k < 27u; k++)
        if ((ch >> k) & 1u) {
            fm1_irq_off();
            du_key(&da_u, DU_KEY_LO + (int)k, (notes >> k) & 1u);
            fm1_irq_on();
        }
    da_notes_prev = notes;
}

/* the LEDs: the page's button, REC while a DROP holds, SEQ while a solo plays, PLAY green while playing; the keys: the
 * ones held, else the chord's tones (the notes that fit right now) */
static void da_leds(void)
{
    static const uint8_t PAGE_BTN[DU_PAGES] = {
        [DU_PG_PLAY] = B_HOME, [DU_PG_SOUND] = B_FX, [DU_PG_TONE] = B_FX, [DU_PG_VOICE] = B_LFO, [DU_PG_KEY] = B_SCL,
        [DU_PG_GROOVE] = B_ARP, [DU_PG_DRUMS] = B_EDIT, [DU_PG_MIX] = B_GLO, [DU_PG_SPACE] = B_GLO, [DU_PG_ARC] = B_ENV,
        [DU_PG_SOLO] = B_SEQ, [DU_PG_SETS] = B_SAVE, [DU_PG_MENU] = B_HOME};
    static uint8_t ready;
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0};
    uint32_t k, c;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    led_put(nl, panel.btn[PAGE_BTN[da_u.page % DU_PAGES]], 1);
    led_put(nl, panel.btn[B_REC], da_g.drop);
    led_put(nl, panel.btn[B_SEQ], da_soloing(&da_g));
    for (k = 0; k < NB; k++)
        led_put(nd, panel.btn[k], 1);
    uint32_t lit = 0;
    if (da_g.playing && da_g.nloop > 0) {
        const da_chord *chd = &da_g.loop[da_chord_now(&da_g)];
        for (k = 0; k < 27u; k++)
            for (int t = 0; t < 4; ++t)
                if ((DU_KEY_LO + k) % 12u == chd->tone[t]) lit |= 1u << k;
    }
    for (k = 0; k < 27u; k++) {
        led_put(nl, 14u + k, (int)((fm1_in.notes >> k) & 1u));
        led_put(nd, 14u + k, (int)((lit >> k) & 1u) || !((fm1_in.notes >> k) & 1u));
    }
    if (da_g.playing)
        led_clear(nd, panel.btn[B_PLAY]);
    for (c = 0; c < FM1_NCOL; c++)
        nd[c] = settings_leds == LEDS_DIM || settings_leds == LEDS_DIM_LO ? (uint8_t)(nd[c] & ~nl[c]) : 0u;
    /* the chord's keys glow dim even with the dim LEDs off: they are music, not decoration */
    for (k = 0; k < 27u; k++)
        if ((lit >> k) & 1u && !((fm1_in.notes >> k) & 1u))
            led_put(nd, 14u + k, 1);
    if (da_g.playing)
        nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u));
    fm1_led_dim_level(settings_leds == LEDS_DIM_LO);
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led_dim[c] = nd[c];
        fm1_led_breath[c] = 0;
        fm1_led[c] = nl[c];
    }
}

/* a frame: the settings into the tracks, the LEDs, the screen; the settings and sets saved a while after the last
 * change, stopped */
static uint32_t da_last_ms;
static void da_frame(void)
{
    uint32_t now = fm1_ms, dt = now - da_last_ms;
    static uint8_t first = 1;
    da_last_ms = now;
    if (da_u.changed) {
        da_u.changed = false;
        da_dirty = true;
        da_save_ms = now;
    }
    da_apply();
    if (da_dirty && !da_g.playing && now - da_save_ms > 2000u) {
        da_settings_save();
        da_dirty = false;
    }
    if (da_u.sets_changed && !da_g.playing) {                /* (stopped: the flash is the code's while playing) */
        da_u.sets_changed = false;
        da_sets_save();
    }
    song.playing = da_g.playing;                             /* (PLAY's green LED, Felucca's meters) */
    da_leds();
    da_u.batt = (int)batt_shown();
    du_tick(&da_u, dt > 100u ? 100 : (int)dt);
    if (first) {
        du_draw_all(&da_u);
        first = 0;
    } else {
        du_draw(&da_u);
    }
}
