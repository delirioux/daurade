/* SPDX-License-Identifier: GPL-3.0-only */
/* DAURADE screen and controls (da_ui.h): Daurade's pages in Tuna's frame, drawn with Felucca's gfx.c. Included after
 * gfx.c (the firmware's felucca.c unit through da_fw.c, or tests/da_ui_test.c). Needs da_sounds.h (the sounds'
 * names) before it. */
#include "da_ui.h"
#include "da_version.h"
#include "ui_icons.h"

#define DU_HEAD 24
#define DU_CARD_Y 28
#define DU_CARD_W 57
#define DU_CARD_H 44
#define DU_CARD_X(c) (3 + 59 * (int32_t)(c))
#define DU_PANEL_Y 76
#define DU_PANEL_H 122
#define DU_FOOT_Y 202
#define DU_FOOT_H 38
#define DU_MENU_HOLD_MS 600

static int du_clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int du_wrap(int v, int n) { return n > 0 ? ((v % n) + n) % n : 0; }
static void du_cpy(char *d, const char *s, unsigned n)
{
    unsigned i = 0;
    for (; s[i] && i + 1 < n; ++i) d[i] = s[i];
    d[i] = 0;
}
static void du_cat(char *d, const char *s, unsigned n)
{
    unsigned k = 0;
    while (d[k]) ++k;
    du_cpy(d + k, s, n > k ? n - k : 0);
}
static void du_fmt_int(char *d, int v)
{
    char b[12];
    int k = 0, neg = v < 0;
    unsigned u = neg ? (unsigned)-v : (unsigned)v;
    do { b[k++] = (char)('0' + u % 10); u /= 10; } while (u && k < 11);
    int j = 0;
    if (neg) d[j++] = '-';
    while (k) d[j++] = b[--k];
    d[j] = 0;
}
static uint32_t du_hash(uint32_t h, const char *s)
{
    while (*s) h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}
static int32_t du_icon_cell(uint32_t *size, uint32_t id)
{
    const uint8_t *idx = *size == 24u ? AI24_IDX : *size == 16u ? AI16_IDX : AI12_IDX;
    if (id >= ICON_COUNT)
        return -1;
    if (idx[id] == 0xFFu) {
        if (*size == 12u || AI12_IDX[id] == 0xFFu)
            return -1;
        *size = 12u;
        idx = AI12_IDX;
    }
    return idx[id];
}
/* the icon's ink centred up and down on the line cy */
static int32_t du_icon_mid(int32_t x, int32_t cy, uint32_t size, uint32_t id, uint16_t fg, uint16_t bg)
{
    uint32_t sz = size;
    int32_t k = du_icon_cell(&sz, id), y0, y1;
    if (k < 0)
        return 0;
    if (sz == 24u) {
        uint32_t v = AI24_INK[k];
        y0 = (int32_t)(v >> 8 & 255u);
        y1 = (int32_t)(v >> 24 & 255u);
    } else {
        uint32_t v = sz == 16u ? AI16_INK[k] : AI12_INK[k];
        y0 = (int32_t)(v >> 4 & 15u);
        y1 = (int32_t)(v >> 12 & 15u) + 1;
    }
    int32_t y = cy - HALF_UP(y1 - y0) - y0;
    cv_alpha_hc(x, y, sz, sz, sz == 24u ? AI24_DATA + AI24_OFF[k] : sz == 16u ? AI16_DATA + AI16_OFF[k] :
                AI12_DATA + AI12_OFF[k], AI_HC, ramp(fg, bg));
    GFX_HOOK_TEXT(x, y + cv_oy, x + (int32_t)sz, y + cv_oy + (int32_t)sz, "icon", 4u);
    return (int32_t)sz;
}

/* ---- the cards ------------------------------------------------------------------------------------------- */
enum { DC_NUM, DC_LIST, DC_SND, DC_KEY, DC_SIGNED, DC_LOOP, DC_VALS, DC_PAL, DC_CRUSH, DC_SETSEL, DC_LOAD, DC_STORE,
       DC_CLEAR, DC_OFF };
typedef struct {
    const char *label;
    uint8_t off;                                   /* offsetof(da_settings, the field) */
    uint8_t kind;
    int16_t lo, hi;
    uint8_t step;
    const char *const *names;
    const uint8_t *vals;                           /* DC_VALS: the values it steps through (hi + 1 of them) */
} du_cdef;
#define DS_(f) ((uint8_t)__builtin_offsetof(da_settings, f))
static const char *const DN_ONOFF[] = {"OFF", "ON"};
static const char *const DN_AUTO[] = {"OFF", "AUTO"};
static const char *const DN_LOOK[] = {"FLAT", "LINE"};
static const char *const DN_HATS[] = {"8THS", "16THS", "OFFBEAT", "OPEN"};
static const char *const DN_FILLS[] = {"OFF", "8 BARS", "4 BARS", "2 BARS"};
static const char *const DN_KICK[] = {"PUNCH", "ROUND"};
static const char *const DN_SNARE[] = {"CLAP", "SNARE", "BOTH"};
static const char *const DN_SPEED[] = {"1/8", "TRIP", "1/16"};
static const char *const DN_SCALE[] = {"DOR", "MIN", "MAJ", "MIX", "LYD", "HARM"};
static const char *const DN_SCALE_LONG[] = {"DORIAN", "MINOR", "MAJOR", "MIXOLYDIAN", "LYDIAN", "HARMONIC MINOR"};
static const uint8_t DV_BARS[] = {1, 2, 4}, DV_BUILD[] = {4, 8, 16}, DV_SOLO[] = {2, 4, 8}, DV_RANGE[] = {1, 2, 3};
#define C_NUM(l, f) {l, DS_(f), DC_NUM, 0, 100, 5, 0, 0}
#define C_LIST(l, f, n) {l, DS_(f), DC_LIST, 0, (int16_t)(sizeof n / sizeof n[0]) - 1, 1, n, 0}
#define C_VALS(l, f, v) {l, DS_(f), DC_VALS, 0, (int16_t)(sizeof v / sizeof v[0]) - 1, 1, 0, v}
#define C_SND(l, p) {l, (uint8_t)(DS_(snd) + (p)), DC_SND, 0, (p) == 3 ? DA_KIT_COUNT - 1 : DA_SOUNDS - 1, 1, 0, 0}
#define C_OFF {"", 0, DC_OFF, 0, 0, 0, 0, 0}
static const du_cdef DU_CARDS[DU_PAGES][4] = {
    [DU_PG_PLAY] = {C_NUM("SWEEP", sweep), C_NUM("PUMP", pump), C_NUM("VOICE", voice), C_NUM("ENERGY", energy)},
    [DU_PG_SOUND] = {C_SND("STABS", 0), C_SND("BASS", 1), C_SND("LEAD", 2), C_SND("DRUMS", 3)},
    [DU_PG_TONE] = {C_NUM("RESO", reso), C_NUM("DRIVE", drive), C_NUM("PHASER", phaser),
                    {"CRUSH", DS_(crush), DC_CRUSH, 0, 100, 5, 0, 0}},
    [DU_PG_VOICE] = {{"VOWELS", DS_(vowels), DC_LIST, 0, DA_VOWELS_COUNT - 1, 1, DAS_VOWEL_NAMES, 0},
                     {"SHIFT", DS_(shift), DC_SIGNED, -12, 12, 1, 0, 0},
                     {"TALK", DS_(talk), DC_LIST, 0, DA_TALK_COUNT - 1, 1, DAS_TALK_NAMES, 0}, C_NUM("GLIDE", glide)},
    [DU_PG_KEY] = {{"KEY", DS_(key), DC_KEY, 0, 11, 1, 0, 0}, C_LIST("SCALE", scale, DN_SCALE),
                   {"CHORDS", DS_(chords), DC_LOOP, 0, DA_LOOPS - 1, 1, 0, 0}, C_VALS("BARS", bars, DV_BARS)},
    [DU_PG_GROOVE] = {{"SWING", DS_(swing), DC_NUM, 0, 60, 2, 0, 0}, C_NUM("HUMAN", human), C_LIST("HATS", hats, DN_HATS),
                      C_LIST("FILLS", fills, DN_FILLS)},
    [DU_PG_DRUMS] = {C_SND("KIT", 3), C_LIST("KICK", kick, DN_KICK), C_LIST("SNARE", snare, DN_SNARE), C_NUM("SNAP", snap)},
    [DU_PG_MIX] = {C_NUM("STABS", lvl[0]), C_NUM("BASS", lvl[1]), C_NUM("LEAD", lvl[2]), C_NUM("DRUMS", lvl[3])},
    [DU_PG_SPACE] = {C_NUM("REVERB", reverb), C_NUM("DELAY", delay),
                     {"TIME", DS_(dtime), DC_LIST, 0, DA_DTIME_COUNT - 1, 1, DAS_DTIME_NAMES, 0}, C_NUM("WIDTH", width)},
    [DU_PG_ARC] = {C_LIST("ARC", arc, DN_ONOFF), C_VALS("BUILD", build, DV_BUILD), C_LIST("DROP", drop, DN_AUTO),
                   C_LIST("SOLO", solo, DN_AUTO)},
    [DU_PG_SOLO] = {C_LIST("SPEED", speed, DN_SPEED), C_NUM("BEND", bend), C_VALS("RANGE", range, DV_RANGE),
                    C_VALS("BARS", solo_bars, DV_SOLO)},
    [DU_PG_SETS] = {{"SET", 0, DC_SETSEL, 0, DA_SETS - 1, 1, 0, 0}, {"LOAD", 0, DC_LOAD, 0, 0, 1, 0, 0},
                    {"STORE", 0, DC_STORE, 0, 0, 1, 0, 0}, {"CLEAR", 0, DC_CLEAR, 0, 0, 1, 0, 0}},
    [DU_PG_MENU] = {{"COLOR", DS_(palette), DC_PAL, 0, 10, 1, 0, 0}, C_LIST("LOOK", look, DN_LOOK), C_OFF, C_OFF},
};
static const char *const DU_PAGE_NAMES[DU_PAGES] = {"PLAY", "SOUND", "TONE", "VOICE", "KEY", "GROOVE", "DRUMS", "MIX",
                                                    "SPACE", "ARC", "SOLO", "SETS", "MENU"};
const char *du_page_name(int page) { return page >= 0 && page < DU_PAGES ? DU_PAGE_NAMES[page] : ""; }

static int du_get(const da_ui *u, const du_cdef *c)
{
    const uint8_t *b = (const uint8_t *)&u->g->s + c->off;
    if (c->kind == DC_SIGNED)
        return (int)*(const int8_t *)b;
    if (c->kind == DC_VALS) {                      /* the index of the value it holds */
        for (int i = 0; i <= c->hi; ++i)
            if (c->vals[i] == *b) return i;
        return 0;
    }
    if (c->kind == DC_SETSEL)
        return u->set_sel;
    return (int)*b;
}
static void du_set(da_ui *u, const du_cdef *c, int v)
{
    uint8_t *b = (uint8_t *)&u->g->s + c->off;
    if (c->kind == DC_SIGNED) *(int8_t *)b = (int8_t)v;
    else if (c->kind == DC_VALS) *b = c->vals[du_clampi(v, c->lo, c->hi)];
    else if (c->kind == DC_SETSEL) u->set_sel = v;
    else *b = (uint8_t)v;
}
static void du_card_value(const da_ui *u, const du_cdef *c, char *t, unsigned n)
{
    int v = du_get(u, c);
    t[0] = 0;
    switch (c->kind) {
    case DC_LIST: du_cpy(t, c->names[du_clampi(v, c->lo, c->hi)], n); break;
    case DC_SND: du_cpy(t, das_name(c->off - DS_(snd), v), n); break;
    case DC_KEY: du_cpy(t, da_note_name(v), n); break;
    case DC_SIGNED: if (v > 0) { du_cpy(t, "+", n); } du_fmt_int(t + (v > 0), v); break;
    case DC_LOOP: da_loop_name(&u->g->s, v, t, (int)n); break;
    case DC_VALS:
        du_fmt_int(t, c->vals[du_clampi(v, c->lo, c->hi)]);
        if (c->off == DS_(build)) du_cat(t, " BARS", n);
        if (c->off == DS_(range)) du_cat(t, " OCT", n);
        break;
    case DC_PAL: du_cpy(t, UI_PALETTES[du_wrap(v, (int)NPALETTES)].name, n); break;
    case DC_CRUSH: if (v) du_fmt_int(t, v); else du_cpy(t, "OFF", n); break;
    case DC_SETSEL: du_fmt_int(t, v + 1); break;
    case DC_LOAD: du_cpy(t, (u->set_used >> u->set_sel) & 1u ? ">" : "EMPTY", n); break;
    case DC_STORE: du_cpy(t, ">", n); break;
    case DC_CLEAR:
        if (u->clear_turns == 1) du_cpy(t, ">>", n);
        else if (u->clear_turns == 2) du_cpy(t, ">", n);
        else du_cpy(t, ">>>", n);
        break;
    case DC_OFF: break;
    default: du_fmt_int(t, v); break;
    }
}

static void du_draw_card(da_ui *u, int i, const du_cdef *c)
{
    bool hot = u->hot == i && u->hot_ms > 0;
    uint16_t lc = hot ? T_ACCENT : T_MID, vc = hot ? T_ACCENT : T_THEME;
    char val[24];
    du_card_value(u, c, val, sizeof val);
    int ratio = -1;
    if ((c->kind == DC_NUM || c->kind == DC_CRUSH || c->kind == DC_SIGNED || c->kind == DC_VALS || c->kind == DC_SETSEL ||
         c->kind == DC_LOOP || c->kind == DC_KEY || c->kind == DC_SND) && c->hi > c->lo)
        ratio = (du_get(u, c) - c->lo) * 1000 / (c->hi - c->lo);
    uint32_t sig = du_hash(2166136261u + (uint32_t)i * 7u + ux.gen * 131u + (uint32_t)hot * 3u + (uint32_t)u->page * 977u,
                           c->label);
    sig = du_hash(sig, val) + (uint32_t)(ratio + 1) * 31u;
    if (!u->force && sig == u->sig_card[i])
        return;
    u->sig_card[i] = sig;
    cv_begin(DU_CARD_W, DU_CARD_H, T_BG);
    cv_rrect(0, 0, DU_CARD_W, DU_CARD_H, 4, T_SURF, T_BG);
    if (c->kind == DC_OFF) {
        cv_rect(DU_CARD_W / 2 - 6, DU_CARD_H / 2, 12, 1, T_DIM);
    } else {
        cv_text_on(5, 3, &AF_S, c->label, lc, T_SURF);
        const aafont_t *vf = text_w(&AF_M, val) <= DU_CARD_W - 8 ? &AF_M : &AF_S;
        cv_text_fit(5, vf == &AF_M ? 17 : 19, vf, val, vc, T_SURF, DU_CARD_W - 8);
        if (ratio >= 0) {
            int32_t w = DU_CARD_W - 10, x0 = 5, y = 37, r = du_clampi(ratio, 0, 1000) * w / 1000;
            cv_rect(x0, y, w, 3, T_LINE);
            if (c->kind == DC_SIGNED) {
                int32_t mid = x0 + w / 2;
                if (r >= w / 2) cv_rect(mid, y, r - w / 2 + 1, 3, vc);
                else cv_rect(x0 + r, y, mid - x0 - r + 1, 3, vc);
                cv_rect(mid, y - 2, 1, 7, hot ? T_ACCENT : T_MID);
            } else {
                cv_rect(x0, y, r > 0 ? r : 1, 3, vc);
            }
        }
    }
    cv_blit((uint32_t)DU_CARD_X(i), DU_CARD_Y);
}

/* ---- the header -------------------------------------------------------------------------------------------- */
static void du_head_msg(const da_ui *u, char *t, unsigned n)
{
    const da_groove *g = u->g;
    t[0] = 0;
    if (u->msg_ms > 0) {
        du_cpy(t, u->msg, n);
    } else if (g->next_style >= 0) {
        du_cpy(t, "NEXT ", n);
        du_cat(t, da_style_name(g->next_style), n);
    } else if (g->next_song >= 0) {
        char a[8];
        du_cpy(t, "NEXT SONG ", n);
        du_fmt_int(a, g->next_song + 1);
        du_cat(t, a, n);
    } else if (g->playing && g->drop) {
        du_cpy(t, "DROP", n);
    } else if (g->playing && da_soloing(g)) {
        du_cpy(t, g->solo_wait >= 0 ? "SOLO IN 1" : "SOLO", n);
    } else if (g->playing && g->s.arc) {
        du_cpy(t, da_arc_name(g->arc_phase), n);
    }
}

static void du_draw_head(da_ui *u)
{
    const da_groove *g = u->g;
    char b[16], m[40];
    bool hot = u->hot == 9 && u->hot_ms > 0;
    du_head_msg(u, m, sizeof m);
    uint32_t sig = du_hash((uint32_t)g->playing * 3u + (uint32_t)g->s.bpm * 101u + ux.gen * 13u + (uint32_t)u->batt * 7777u +
                           (uint32_t)hot * 23u, m);
    if (!u->force && sig == u->sig_head)
        return;
    u->sig_head = sig;
    cv_begin(240, DU_HEAD, T_BG);
    du_icon_mid(8, DU_HEAD / 2, 16, g->playing ? ICON_X_PLAY : ICON_X_STOP, g->playing ? T_THEME : T_MID, T_BG);
    du_icon_mid(54, DU_HEAD / 2, 16, ICON_TEMPO, T_MID, T_BG);
    du_fmt_int(b, g->s.bpm);
    cv_text_on(72, CAP_IN(M, DU_HEAD), &AF_M, b, hot ? T_ACCENT : T_THEME, T_BG);
    if (m[0]) {
        if (m[0] == 'N' && m[1] == 'E' && m[2] == 'X' && m[3] == 'T' && m[4] == ' ') {   /* NEXT in the accent */
            int32_t x = cv_text_on(106, CAP_IN(S, DU_HEAD), &AF_S, "NEXT", T_ACCENT, T_BG) + 5;
            cv_free_text(x, CAP_IN(S, DU_HEAD), &AF_S, m + 5, T_TEXT, T_BG, 186 - x);
        } else {
            cv_free_text(106, CAP_IN(S, DU_HEAD), &AF_S, m, T_TEXT, T_BG, 186 - 106);
        }
    }
    {
        static const uint8_t BAT[5] = {ICON_X_BAT0, ICON_X_BAT1, ICON_X_BAT3, ICON_X_BAT4, ICON_X_BAT_CHG};
        int lv = du_clampi(u->batt, 0, 4);
        du_icon_mid(214, DU_HEAD / 2, 24, BAT[lv], lv == 1 ? T_ACCENT : lv == 0 ? T_MID : T_THEME, T_BG);
    }
    cv_blit(0, 0);
}

/* ---- the footer: the 16-bar sweep cycle as 16 dashes, the style and its side, the page ---------------------- */
static void du_draw_foot(da_ui *u)
{
    const da_groove *g = u->g;
    int filled = g->playing ? g->bar % 16 : 0;
    const char *style = da_style_name(g->s.style);
    const char *side = DA_STYLE[g->s.style].side == DA_SIDE_CHROME ? "CHROME" : "SESSION";
    uint32_t sig = du_hash(du_hash((uint32_t)filled * 7u + (uint32_t)g->playing * 3u + ux.gen * 11u + (uint32_t)u->page * 13u,
                                   style), du_page_name(u->page));
    if (!u->force && sig == u->sig_foot)
        return;
    u->sig_foot = sig;
    cv_begin(240, DU_FOOT_H, T_BG);
    for (int i = 0; i < 16; ++i)
        cv_rect(10 + i * 14, 6, 10, 2, i < filled ? T_THEME : i == filled && g->playing ? T_ACCENT : T_LINE);
    cv_free_text(10, 17, &AF_M, style, T_THEME, T_BG, 130);
    int32_t nx = 10 + text_w(&AF_M, style);
    if (nx > 140) nx = 140;
    int32_t yb = 17 + AF_M_CAP_Y + AF_M_CAP_H - AF_S_CAP_Y - AF_S_CAP_H;
    int32_t px = 230 - text_w(&AF_S, du_page_name(u->page)) - 8;
    if (px - nx - 6 > 24)
        cv_free_text(nx + 6, yb, &AF_S, side, T_MID, T_BG, px - nx - 6);
    cv_text_r(230, yb, &AF_S, du_page_name(u->page), T_MID, T_BG);
    cv_blit(0, DU_FOOT_Y);
}

/* ---- the pictures -------------------------------------------------------------------------------------------- */
static void du_panel_begin(void)
{
    cv_begin(240, DU_PANEL_H, T_BG);
    cv_rrect(3, 0, 234, DU_PANEL_H, 4, T_SURF, T_BG);
}
static int32_t du_yb_s(int32_t y_m) { return y_m + AF_M_CAP_Y + AF_M_CAP_H - AF_S_CAP_Y - AF_S_CAP_H; }
static void du_label(int32_t x, int32_t y, const char *s, uint16_t c) { cv_text_on(x, y, &AF_S, s, c, T_SURF); }
static uint16_t du_well(void) { return ux_mix(T_SURF, T_LINE, 100); }

/* the loop's chords, the one sounding filled */
static void du_chord_strip(const da_groove *g, int32_t y, int now)
{
    int n = g->nloop < 4 ? g->nloop : 4;
    int32_t gap = 4, w = (224 - gap * (n - 1)) / (n > 0 ? n : 1);
    for (int i = 0; i < n; ++i) {
        int32_t x = 10 + i * (w + gap);
        uint16_t f = i == now ? T_THEME : du_well();
        cv_rrect(x, y, w, 20, 3, f, T_SURF);
        cv_text_in(x, y + CAP_IN(S, 20), w, &AF_S, g->loop[i].name, i == now ? T_INK : T_MID, f);
    }
}

/* the low-pass's curve: flat, the resonant peak at the cutoff, then the fall; cutoff and reso 0..1000 */
static void du_filter_curve(int32_t x0, int32_t y0, int32_t w, int32_t h, int cutoff, int reso, uint16_t c, bool fill)
{
    int32_t cx = x0 + 8 + cutoff * (w - 16) / 1000, base = y0 + h * 45 / 100, py = -1;
    int32_t pk = reso * (h * 40 / 100) / 1000;
    for (int32_t x = x0; x < x0 + w; ++x) {
        int32_t d = x - cx, y;
        if (d < -24) y = base;
        else if (d <= 0) y = base - pk * (24 + d) * (24 + d) / (24 * 24);
        else y = base - pk + d * d / 6;
        if (d > 0 && y < base - pk + d) y = base - pk + d;
        if (y > y0 + h) y = y0 + h;
        if (y < y0) y = y0;
        if (fill)
            cv_rect(x, y + 1, 1, y0 + h - y, ux_mix(T_SURF, c, 18));
        if (py >= 0)
            cv_line_t(x - 1, py, x, y, c, 2);
        py = y;
    }
    cv_rect(cx, y0 + h + 3, 1, 4, T_ACCENT);
}

/* the pump: the mix ducking on every kick and breathing back, one bar of four beats; the beat playing marked */
static void du_pump_wave(int32_t x0, int32_t y0, int32_t w, int32_t h, int depth, uint16_t c, int beat_now)
{
    int32_t beat = w / 4, py = -1;
    for (int32_t x = 0; x < w; ++x) {
        int32_t p = x % beat;
        int32_t dip = depth * h / 1000;
        int32_t rec = p * 1000 / (beat * 7 / 10);
        if (rec > 1000) rec = 1000;
        int32_t y = y0 + dip - dip * (1000 - (1000 - rec) * (1000 - rec) / 1000) / 1000;
        if (py >= 0)
            cv_line_t(x0 + x - 1, py, x0 + x, y, c, 2);
        py = y;
    }
    for (int b = 0; b < 4; ++b)
        cv_rect(x0 + b * beat, y0 + h + 3, 2, 4, b == beat_now ? T_ACCENT : T_MID);
}

static void du_panel_play(da_ui *u)
{
    const da_groove *g = u->g;
    if (!g->playing) {
        char line[40], a[8];
        int32_t w = kc_w(KC_PLAY) + 5 + text_w(&AF_S, "TO START");
        int32_t x = 3 + HALF_UP(234 - w), y = DU_PANEL_H / 2 - KC_H / 2 - 8;
        cv_keycap(x, y, KC_PLAY, T_KEY, T_INK, T_SURF);
        cv_text_on(x + kc_w(KC_PLAY) + 5, y + CAP_IN(S, KC_H), &AF_S, "TO START", T_MID, T_SURF);
        du_cpy(line, "SONG ", sizeof line);
        du_fmt_int(a, g->s.song + 1);
        du_cat(line, a, sizeof line);
        du_cat(line, "   ", sizeof line);
        du_cat(line, da_note_name(g->s.key), sizeof line);
        du_cat(line, " ", sizeof line);
        du_cat(line, DN_SCALE_LONG[g->s.scale % DA_SCALES], sizeof line);
        int32_t lw = text_w(&AF_S, line);
        cv_text_on(3 + HALF_UP(234 - lw), DU_PANEL_H / 2 + 14, &AF_S, line, T_DIM, T_SURF);
        return;
    }
    if (g->drop) {                                 /* DROP: the filtered loop alone */
        static const char *const parts[4] = {"BASS", "LEAD", "DRUMS", "KICK"};
        char t[16], a[8];
        cv_text_on(10, 6, &AF_L, "DROP", T_ACCENT, T_SURF);
        int back = g->drop_held ? -1 : g->drop_end - g->bar;
        if (back > 0) {
            du_cpy(t, "BACK IN ", sizeof t);
            du_fmt_int(a, back);
            du_cat(t, a, sizeof t);
        } else {
            du_cpy(t, "HELD", sizeof t);
        }
        cv_text_r(230, du_yb_s(10), &AF_S, t, T_TEXT, T_SURF);
        for (int i = 0; i < 4; ++i) {
            int32_t x = 10 + i * 56;
            cv_rrect(x, 46, 52, 18, 3, du_well(), T_SURF);
            cv_text_in(x, 46 + CAP_IN(S, 18), 52, &AF_S, parts[i], T_DIM, du_well());
        }
        du_label(10, 74, "LOOP", T_MID);
        du_filter_curve(46, 72, 184, 34, (int)(g->sweep_now * 1000.0f), g->s.reso * 10, T_THEME, true);
        return;
    }
    char t[24];
    du_chord_strip(g, 6, da_chord_now(g));
    du_label(10, 36, "SWEEP", T_MID);
    du_cpy(t, da_note_name(g->s.key), sizeof t);
    du_cat(t, " ", sizeof t);
    du_cat(t, DN_SCALE_LONG[g->s.scale % DA_SCALES], sizeof t);
    cv_text_r(230, 36, &AF_S, t, T_MID, T_SURF);
    du_filter_curve(10, 50, 220, 30, (int)(g->sweep_now * 1000.0f), 300 + g->s.reso * 6, T_THEME, true);
    du_label(10, 92, "PUMP", T_MID);
    du_pump_wave(46, 92, 184, 16, g->s.pump * 8, T_TEXT, g->step < 0 ? 0 : g->step / 4);
}

/* PRESETS turned: the ten styles, the queued one marked */
static void du_panel_styles(da_ui *u)
{
    const da_groove *g = u->g;
    int sel = g->next_style >= 0 ? g->next_style : g->s.style;
    du_label(10, 5, "CHROME", T_MID);
    du_label(124, 5, "SESSION", T_MID);
    for (int i = 0; i < 10; ++i) {
        int col = DA_STYLE[i].side, row = 0;
        for (int k = 0; k < i; ++k) row += DA_STYLE[k].side == col;
        int32_t x = col ? 120 : 6, y = 22 + row * 19;
        bool now = i == g->s.style, q = i == sel && g->next_style >= 0;
        if (q || (i == sel && !g->playing)) {
            cv_rrect(x, y - 3, 110, 17, 3, T_ACCENT, T_SURF);
            cv_text_fit(x + 6, y, &AF_S, DA_STYLE[i].name, T_INK, T_ACCENT, 100);
        } else {
            cv_text_fit(x + 6, y, &AF_S, DA_STYLE[i].name, now ? T_THEME : T_TEXT, T_SURF, 100);
        }
        if (now)
            cv_rect(x - 1, y + 2, 2, 8, T_THEME);
    }
}

static void du_panel_sound(da_ui *u)
{
    const da_groove *g = u->g;
    static const char *const nm[4] = {"STABS", "BASS", "LEAD", "DRUMS"};
    for (int i = 0; i < 4; ++i) {
        int32_t y = 8 + i * 28;
        bool hot = u->hot == i && u->hot_ms > 0;
        cv_text_on(10, y, &AF_S, nm[i], hot ? T_ACCENT : T_MID, T_SURF);
        char t[24];
        du_cpy(t, das_name(i, g->s.snd[i]), sizeof t);
        if (i == DA_LEAD && da_soloing(g)) {
            du_cpy(t, "SOLO: ", sizeof t);
            du_cat(t, DAS_SOUND[2][DA_STYLE[g->s.style].solo_snd % DA_SOUNDS].name, sizeof t);
        }
        cv_text_fit(64, y, &AF_S, t, T_TEXT, T_SURF, 164);
        uint16_t bits = g->playing ? g->hits_last[i] : 0;
        for (int k = 0; k < 16; ++k) {
            bool on = (bits >> k) & 1;
            cv_rect(64 + k * 10, y + 18, 7, 2, on ? (hot ? T_ACCENT : T_THEME) : T_LINE);
        }
    }
}

static void du_panel_tone(da_ui *u)
{
    const da_groove *g = u->g;
    du_label(10, 6, "THE FILTER THE SWEEP MOVES", T_MID);
    du_filter_curve(10, 30, 220, 56, (int)(g->sweep_now * 1000.0f), 150 + g->s.reso * 8, T_THEME, true);
    char t[24], a[8];
    du_cpy(t, "RESO ", sizeof t);
    du_fmt_int(a, g->s.reso);
    du_cat(t, a, sizeof t);
    cv_text_r(230, 100, &AF_S, t, T_THEME, T_SURF);
    du_cpy(t, g->s.crush ? "CRUSH" : g->s.phaser ? "PHASER" : "CLEAN", sizeof t);
    du_label(10, 100, t, T_DIM);
}

/* the robot voice: where the formant sits between the vowels, moving with each note */
static void du_panel_voice(da_ui *u)
{
    const da_groove *g = u->g;
    static const int32_t VX[5] = {120, 88, 40, 200, 152}, VY[5] = {18, 96, 96, 96, 18};   /* A E I O U as drawn */
    static const char *const VN[5] = {"AH", "EH", "EE", "OH", "OO"};
    const uint8_t *vw = DAS_VOWELS[g->s.vowels % DA_VOWELS_COUNT];
    int a = du_clampi((vw[0] + 16) / 32, 0, 4), b = du_clampi((vw[1] + 16) / 32, 0, 4);
    /* the triangle of the five: AH at the top, EE OH at the bottom corners, EH and OO between */
    cv_line(40, 96, 120, 18, T_LINE);
    cv_line(120, 18, 200, 96, T_LINE);
    cv_line(40, 96, 200, 96, T_LINE);
    for (int i = 0; i < 5; ++i) {
        bool on = i == a || i == b;
        cv_text_in(VX[i] - 20, VY[i] + (VY[i] < 50 ? -14 : 6), 40, &AF_S, VN[i], on ? T_THEME : T_DIM, T_SURF);
    }
    cv_line_t(VX[a], VY[a], VX[b], VY[b], T_THEME, 2);
    /* the dot: from the first vowel to the second over TALK after each lead note */
    float talk_ms = 1.0f;
    {
        int16_t v = das_talk(&g->s);
        float ms = 1.0f;
        for (int k = 0; k < v; ++k) ms *= 1.0752f;     /* 10000^(1/127) */
        talk_ms = ms;
    }
    float since = g->playing ? (float)(g->clock - g->lead_on_clock) / g->sr * 1000.0f : talk_ms;
    float f = since / talk_ms;
    if (f > 1.0f) f = 1.0f;
    if (f < 0.0f) f = 0.0f;
    int32_t x = VX[a] + (int32_t)((float)(VX[b] - VX[a]) * f), y = VY[a] + (int32_t)((float)(VY[b] - VY[a]) * f);
    cv_rrect(x - 5, y - 5, 10, 10, 3, T_ACCENT, T_SURF);
    if (das_of(g, DA_LEAD)->engine != DAS_VOICE)
        cv_text_r(230, 106, &AF_S, "LEAD: NOT A VOICE", T_DIM, T_SURF);
}

static void du_panel_key(da_ui *u)
{
    const da_groove *g = u->g;
    char t[32], a[8];
    int bpc = da_bars_per_chord(&g->s), now = g->playing ? da_chord_now(g) : -1;
    du_cpy(t, "THE LOOP, ", sizeof t);
    du_fmt_int(a, bpc);
    du_cat(t, a, sizeof t);
    du_cat(t, bpc == 1 ? " BAR A CHORD" : " BARS A CHORD", sizeof t);
    du_label(10, 6, t, T_MID);
    int n = g->nloop < 4 ? g->nloop : 4;
    int32_t gap = 4, w = (224 - gap * (n - 1)) / (n > 0 ? n : 1);
    for (int i = 0; i < n; ++i) {
        int32_t x = 10 + i * (w + gap);
        uint16_t f = i == now ? T_THEME : du_well();
        cv_rrect(x, 30, w, 52, 4, f, T_SURF);
        const aafont_t *ft = text_w(&AF_M, g->loop[i].name) <= w - 4 ? &AF_M : &AF_S;
        cv_text_in(x, 30 + (ft == &AF_M ? CAP_IN(M, 52) : CAP_IN(S, 52)), w, ft, g->loop[i].name, i == now ? T_INK : T_TEXT, f);
        int b0 = i * bpc + 1, b1 = b0 + bpc - 1;
        du_fmt_int(t, b0);
        if (b1 > b0) { du_cat(t, "-", sizeof t); du_fmt_int(a, b1); du_cat(t, a, sizeof t); }
        cv_text_in(x, 92, w, &AF_S, t, T_MID, T_SURF);
    }
}

static void du_panel_groove(da_ui *u)
{
    const da_groove *g = u->g;
    static const char *const rows[4] = {"KICK", "SNARE", "HAT", "OPEN"};
    uint16_t m[4];
    m[0] = g->drum_last[DA_DR_KICK];
    m[1] = (uint16_t)(g->drum_last[DA_DR_SNARE] | g->drum_last[DA_DR_CLAP]);
    m[2] = g->drum_last[DA_DR_HAT];
    m[3] = g->drum_last[DA_DR_OPEN];
    for (int r = 0; r < 4; ++r) {
        int32_t y = 10 + r * 26;
        du_label(10, y + 2, rows[r], T_MID);
        for (int k = 0; k < 16; ++k) {
            bool on = g->playing && ((m[r] >> k) & 1);
            int32_t x = 54 + k * 11;
            cv_rrect(x, y, 9, 16, 2, on ? (r == 3 ? T_ACCENT : T_THEME) : du_well(), T_SURF);
        }
    }
    if (g->playing && g->step >= 0)
        cv_rect(54 + g->step * 11, 112, 9, 2, T_ACCENT);
}

static void du_panel_drums(da_ui *u)
{
    const da_groove *g = u->g;
    char t[32];
    du_cpy(t, g->s.snd[3] == DA_KIT_CRISP ? "ONE-SHOTS, DRY, NO TAIL" : "FELUCCA'S DRUM ENGINE", sizeof t);
    du_label(10, 6, t, T_MID);
    static const char *const nm[5] = {"KICK", "SNARE", "CLAP", "HAT", "OPEN"};
    static const int LEN[5] = {70, 40, 46, 12, 60};
    for (int i = 0; i < 5; ++i) {
        int32_t x = 10 + i * 45, y = 30, h = 56;
        int len = LEN[i] * (60 + g->s.snap * 4 / 10) / 100;
        for (int k = 0; k < 40; ++k) {             /* the hit: a hard edge, then its decay */
            int32_t a = k < 2 ? h : h * (len - k * len / 40) / (len > 0 ? len : 1);
            a = a * du_clampi(len * 2 - k * 3, 0, 100) / 100;
            if (a > 0) cv_rect(x + k, y + h - a, 1, a, k < 2 ? T_ACCENT : ux_mix(T_SURF, T_THEME, 70));
        }
        cv_text_on(x, 94, &AF_S, nm[i], T_TEXT, T_SURF);
    }
}

static void du_panel_mix(da_ui *u)
{
    const da_groove *g = u->g;
    for (int i = 0; i < 4; ++i) {
        int32_t x = 32 + i * 50, top = 12, h = 90, v = g->s.lvl[i] * h / 100;
        cv_rrect(x, top, 18, h, 4, du_well(), T_SURF);
        if (v > 0)
            cv_rrect(x, top + h - v, 18, v, 4, g->muted[i] && g->playing ? T_DIM : i == 3 ? T_TEXT : T_THEME, du_well());
    }
}

static void du_panel_space(da_ui *u)
{
    const da_groove *g = u->g;
    du_label(10, 6, "ECHOES ON THE BEAT", T_MID);
    static const int GAP[DA_DTIME_COUNT] = {12, 24, 36, 48};   /* px a sixteenth .. a quarter, a beat 48 px */
    int gap = GAP[g->s.dtime % DA_DTIME_COUNT];
    int n = 1 + g->s.delay / 20;
    for (int k = 0; k <= n && 14 + k * gap < 226; ++k) {
        int32_t x = 14 + k * gap, h = 60 * (n + 1 - k) / (n + 1);
        cv_rrect(x, 90 - h, 8, h, 2, k == 0 ? T_THEME : ux_mix(T_SURF, T_THEME, 80 - k * 10), T_SURF);
    }
    for (int b = 0; b < 5; ++b)
        cv_rect(14 + b * 48, 96, 1, 6, T_MID);
}

/* the song's arc: the phases as they go, the one playing in the accent */
static void du_panel_arc(da_ui *u)
{
    const da_groove *g = u->g;
    if (!g->s.arc) {
        du_label(10, 30, "ARC OFF", T_THEME);
        du_label(10, 50, "THE SONG PLAYS AS YOU SHAPE IT:", T_MID);
        du_label(10, 66, "REC DROPS, SEQ SOLOS", T_MID);
        return;
    }
    static const int ORDER[6] = {DA_ARC_INTRO, DA_ARC_MAIN, DA_ARC_BUILD, DA_ARC_DROP, DA_ARC_SOLO, DA_ARC_OUT};
    static const int LVL[6] = {30, 70, 85, 20, 95, 45};
    int32_t px = -1, py = 0;
    for (int i = 0; i < 6; ++i) {
        int ph = ORDER[i];
        if (ph == DA_ARC_DROP && !g->s.drop) continue;
        int32_t x = 30 + i * 36, y = 84 - LVL[i] * 62 / 100;
        bool now = g->playing && g->arc_phase == ph;
        if (px >= 0) cv_line_t(px, py, x, y, T_THEME, 2);
        cv_rrect(x - 4, y - 4, 9, 9, 3, now ? T_ACCENT : T_THEME, T_SURF);
        px = x;
        py = y;
    }
    for (int i = 0; i < 6; i += 2) {
        int ph = ORDER[i];
        bool now = g->playing && g->arc_phase == ph;
        cv_text_in(30 + i * 36 - 24, 98, 48, &AF_S, da_arc_name(ph), now ? T_ACCENT : T_MID, T_SURF);
    }
    for (int i = 1; i < 6; i += 2) {
        int ph = ORDER[i];
        if (ph == DA_ARC_DROP && !g->s.drop) continue;
        bool now = g->playing && g->arc_phase == ph;
        cv_text_in(30 + i * 36 - 24, 4, 48, &AF_S, da_arc_name(ph), now ? T_ACCENT : T_DIM, T_SURF);
    }
}

static void du_panel_solo(da_ui *u)
{
    const da_groove *g = u->g;
    char t[24];
    du_cpy(t, !g->playing ? "PLAY, THEN SEQ" : g->solo_wait >= 0 ? "SOLO IN 1" : g->solo_left > 0 ? "SOLO" : "SEQ: A SOLO",
           sizeof t);
    du_label(10, 6, t, da_soloing(g) ? T_ACCENT : T_MID);
    char k[24];
    du_cpy(k, da_note_name(g->s.key), sizeof k);
    du_cat(k, " ", sizeof k);
    du_cat(k, DN_SCALE_LONG[g->s.scale % DA_SCALES], sizeof k);
    cv_text_r(230, 6, &AF_S, k, T_MID, T_SURF);
    /* the run: the scale over RANGE octaves, the note playing lit */
    int steps = 7 * g->s.range + 1;
    for (int i = 0; i < steps; ++i) {
        int32_t x = 12 + i * 216 / (steps > 1 ? steps - 1 : 1), h = 8 + i * 70 / (steps > 1 ? steps : 1);
        bool on = g->solo_left > 0 && g->solo_note - 7 == i;
        cv_rrect(x - 2, 104 - h, 5, h, 2, on ? T_ACCENT : ux_mix(T_SURF, T_THEME, 45), T_SURF);
    }
}

static void du_panel_sets(da_ui *u)
{
    for (int i = 0; i < DA_SETS; ++i) {
        int32_t col = i / 4, row = i % 4, x = 10 + col * 112, y = 8 + row * 27;
        bool sel = i == u->set_sel, used = (u->set_used >> i) & 1u;
        if (sel) cv_rrect(x - 4, y - 4, 110, 21, 3, T_THEME, T_SURF);
        char n[4];
        du_fmt_int(n, i + 1);
        uint16_t bg = sel ? T_THEME : T_SURF;
        cv_text_on(x, y, &AF_S, n, sel ? T_INK : T_MID, bg);
        cv_text_fit(x + 14, y, &AF_S, used ? da_style_name(u->sets[i].style) : "EMPTY", sel ? T_INK : used ? T_TEXT : T_DIM, bg, 88);
    }
}

static void du_panel_menu(da_ui *u)
{
    (void)u;
    cv_text_on(30, 20, &AF_M, "DAURADE " DA_VERSION, T_TEXT, T_SURF);
    cv_text_on(30, 44, &AF_S, "GROOVES IN CHROME FOR THE FM-1", T_MID, T_SURF);
    cv_text_on(30, 64, &AF_S, "ON FELUCCA, AFTER TUNA", T_MID, T_SURF);
    cv_text_on(30, 84, &AF_S, "GPL-3.0, HOME CLOSES", T_DIM, T_SURF);
}

static void du_draw_panel(da_ui *u)
{
    du_panel_begin();
    if (u->browse_ms > 0) {
        du_panel_styles(u);
    } else {
        switch (u->page) {
        case DU_PG_PLAY: du_panel_play(u); break;
        case DU_PG_SOUND: du_panel_sound(u); break;
        case DU_PG_TONE: du_panel_tone(u); break;
        case DU_PG_VOICE: du_panel_voice(u); break;
        case DU_PG_KEY: du_panel_key(u); break;
        case DU_PG_GROOVE: du_panel_groove(u); break;
        case DU_PG_DRUMS: du_panel_drums(u); break;
        case DU_PG_MIX: du_panel_mix(u); break;
        case DU_PG_SPACE: du_panel_space(u); break;
        case DU_PG_ARC: du_panel_arc(u); break;
        case DU_PG_SOLO: du_panel_solo(u); break;
        case DU_PG_SETS: du_panel_sets(u); break;
        case DU_PG_MENU: du_panel_menu(u); break;
        }
    }
    cv_blit(0, DU_PANEL_Y);
}

static void du_background(void)
{
    cv_begin(240, 40, T_BG);
    for (int y = 0; y < 240; y += 40)
        cv_blit(0, (uint32_t)y);
    if (ux.style == ST_LINE) {                    /* LINE: no cards, 1 px rules between the areas */
        cv_begin(240, 1, T_RULE);
        cv_blit(0, DU_HEAD + 1);
        cv_blit(0, DU_CARD_Y + DU_CARD_H + 1);
        cv_blit(0, DU_FOOT_Y - 3);
        cv_begin(1, DU_CARD_H, T_RULE);
        for (int c = 1; c < 4; ++c)
            cv_blit((uint32_t)(DU_CARD_X(c) - 2), DU_CARD_Y);
    }
}

void du_draw(da_ui *u)
{
    if (u->force)
        du_background();
    du_draw_head(u);
    for (int i = 0; i < 4; ++i)
        du_draw_card(u, i, &DU_CARDS[u->page][i]);
    du_draw_panel(u);
    du_draw_foot(u);
    u->force = false;
    u->frame++;
}

void du_draw_all(da_ui *u)
{
    u->force = true;
    du_draw(u);
}

/* ---- input --------------------------------------------------------------------------------------------------- */
static void du_msg(da_ui *u, const char *m)
{
    du_cpy(u->msg, m, sizeof u->msg);
    u->msg_ms = 1500;
}

static void du_goto_page(da_ui *u, int pg)
{
    if (pg != u->page) {
        u->page = pg;
        u->force = true;
    }
    u->browse_ms = 0;
}

static void du_apply_style(da_ui *u)
{
    ux.style = (uint8_t)(u->g->s.look ? ST_LINE : ST_FLAT);
    palette_set((uint32_t)u->g->s.palette % NPALETTES);
    u->force = true;
}

void du_init(da_ui *u, da_groove *g)
{
    memset(u, 0, sizeof *u);
    u->g = g;
    u->page = DU_PG_PLAY;
    u->hot = -1;
    u->batt = 3;
    for (int k = 0; k < 27; ++k)
        u->key_sent[k] = -1;
    du_apply_style(u);
}

/* a set loaded: its settings, the menu's colour and look stay; at once (the sound follows in the glue) */
static void du_load_set(da_ui *u, int i)
{
    da_groove *g = u->g;
    da_settings s = u->sets[i];
    s.palette = g->s.palette;
    s.look = g->s.look;
    da_settings_fix(&s);
    da_all_off(g);
    g->s = s;
    g->next_style = g->next_song = -1;
    da_rebuild(g);
    g->sound_sig++;
}

void du_knob(da_ui *u, int k, int d)
{
    da_groove *g = u->g;
    char m[24], a[8];
    switch (k) {
    case DU_K_SELECT:
        g->s.bpm = (uint8_t)du_clampi(g->s.bpm + d, 60, 200);
        u->hot = 9;
        u->hot_ms = 700;
        u->changed = true;
        return;
    case DU_K_PRESETS: {
        int cur = g->next_style >= 0 ? g->next_style : g->s.style;
        da_queue_style(g, du_wrap(cur + d, DA_STYLES));
        u->browse_ms = 1800;
        u->changed = true;
        return;
    }
    case DU_K_ALGO: {
        int cur = g->next_song >= 0 ? g->next_song : g->s.song;
        da_queue_song(g, du_wrap(cur + d, 1000));
        du_cpy(m, g->playing ? "NEXT SONG " : "SONG ", sizeof m);
        du_fmt_int(a, du_wrap(cur + d, 1000) + 1);
        du_cat(m, a, sizeof m);
        du_msg(u, m);
        u->changed = true;
        return;
    }
    case DU_K1: case DU_K2: case DU_K3: case DU_K4: {
        const du_cdef *c = &DU_CARDS[u->page][k - DU_K1];
        u->hot = k - DU_K1;
        u->hot_ms = 700;
        if (c->kind == DC_OFF)
            return;
        if (c->kind == DC_LOAD) {                  /* turned right: the set, at once */
            if (d > 0 && ((u->set_used >> u->set_sel) & 1u)) {
                du_load_set(u, u->set_sel);
                du_cpy(m, "SET ", sizeof m);
                du_fmt_int(a, u->set_sel + 1);
                du_cat(m, a, sizeof m);
                du_cat(m, " LOADED", sizeof m);
                du_msg(u, m);
                du_apply_style(u);
                u->changed = true;
            } else if (d > 0) {
                du_msg(u, "THAT SET IS EMPTY");
            }
            return;
        }
        if (c->kind == DC_STORE) {                 /* turned right: what plays now into the set */
            if (d > 0) {
                u->sets[u->set_sel] = g->s;
                u->set_used |= (uint8_t)(1u << u->set_sel);
                u->sets_changed = true;
                du_cpy(m, "STORED IN SET ", sizeof m);
                du_fmt_int(a, u->set_sel + 1);
                du_cat(m, a, sizeof m);
                du_msg(u, m);
            }
            return;
        }
        if (c->kind == DC_CLEAR) {                 /* three turns right within a second */
            if (d > 0) {
                u->clear_turns++;
                u->clear_ms = 1000;
                if (u->clear_turns >= 3) {
                    u->set_used &= (uint8_t)~(1u << u->set_sel);
                    u->sets_changed = true;
                    u->clear_turns = 0;
                    du_msg(u, "SET CLEARED");
                } else {
                    du_msg(u, "TURN ON TO CLEAR");
                }
            }
            return;
        }
        int v = du_get(u, c) + d * c->step;
        if (c->kind == DC_PAL)
            v = du_wrap(v, (int)NPALETTES);
        else if (c->kind == DC_SND || c->kind == DC_KEY || c->kind == DC_LOOP)
            v = c->lo + du_wrap(v - c->lo, c->hi - c->lo + 1);
        v = du_clampi(v, c->lo, c->hi);
        du_set(u, c, v);
        if (c->off == DS_(key) || c->off == DS_(scale) || c->off == DS_(chords) || c->off == DS_(bars))
            da_rebuild(g);
        if (c->off == DS_(scale))
            du_msg(u, DN_SCALE_LONG[g->s.scale % DA_SCALES]);
        if (c->off == DS_(palette) || c->off == DS_(look))
            du_apply_style(u);
        if (c->off == DS_(arc) && g->playing) {    /* ARC turned on while playing: from its MAIN */
            g->arc_phase = DA_ARC_MAIN;
            g->arc_bar = 0;
        }
        u->changed = true;
        return;
    }
    default:
        return;
    }
}

void du_button(da_ui *u, int b, bool down)
{
    da_groove *g = u->g;
    if (b == DU_B_HOME) {
        if (down) {
            u->home_down = true;
            u->home_ms = 0;
        } else if (u->home_down) {
            u->home_down = false;
            if (u->home_ms < DU_MENU_HOLD_MS) du_goto_page(u, DU_PG_PLAY);
        }
        return;
    }
    if (b == DU_B_REC) {                           /* DROP: down and up both count */
        if (!g->playing) {
            if (down) du_msg(u, "PLAY FIRST");
            return;
        }
        da_drop(g, down);
        return;
    }
    if (!down)
        return;
    switch (b) {
    case DU_B_PLAY: da_play(g, !g->playing); break;
    case DU_B_SEQ:
        du_goto_page(u, DU_PG_SOLO);
        if (g->playing) da_solo(g);
        else du_msg(u, "PLAY FIRST");
        break;
    case DU_B_OCTDN: case DU_B_OCTUP: {
        char m[16], a[8];
        u->oct = du_clampi(u->oct + (b == DU_B_OCTUP ? 1 : -1), -2, 2);
        du_cpy(m, "KEYS OCT ", sizeof m);
        if (u->oct > 0) du_cat(m, "+", sizeof m);
        du_fmt_int(a, u->oct);
        du_cat(m, a, sizeof m);
        du_msg(u, m);
        break;
    }
    case DU_B_FX: du_goto_page(u, u->page == DU_PG_SOUND ? DU_PG_TONE : DU_PG_SOUND); break;
    case DU_B_LFO: du_goto_page(u, DU_PG_VOICE); break;
    case DU_B_SCL: du_goto_page(u, DU_PG_KEY); break;
    case DU_B_ARP: du_goto_page(u, DU_PG_GROOVE); break;
    case DU_B_EDIT: du_goto_page(u, DU_PG_DRUMS); break;
    case DU_B_GLO: du_goto_page(u, u->page == DU_PG_MIX ? DU_PG_SPACE : DU_PG_MIX); break;
    case DU_B_ENV: du_goto_page(u, DU_PG_ARC); break;
    case DU_B_SAVE: du_goto_page(u, DU_PG_SETS); break;
    default: break;
    }
}

void du_key(da_ui *u, int note, bool down)
{
    if (note < DU_KEY_LO || note > DU_KEY_HI)
        return;
    int k = note - DU_KEY_LO;
    uint32_t bit = 1u << k;
    if (down) {
        u->keys_down |= bit;
        u->key_sent[k] = note + 12 * u->oct;      /* (the octave it was pressed in: its release lets go of the same) */
        da_key(u->g, k, u->key_sent[k], true);
    } else {
        u->keys_down &= ~bit;
        da_key(u->g, k, u->key_sent[k], false);
        u->key_sent[k] = -1;
    }
}

void du_tick(da_ui *u, int ms)
{
    da_groove *g = u->g;
    da_seen n;
    while (da_feed_pop(g, &n))
        ;                                          /* (the panels read the groove's own state) */
    if (g->msg_new) {
        g->msg_new = 0;
        du_msg(u, g->msg);
    }
    if (u->hot_ms > 0) u->hot_ms -= ms;
    if (u->clear_ms > 0 && (u->clear_ms -= ms) <= 0) u->clear_turns = 0;
    if (u->browse_ms > 0 && (u->browse_ms -= ms) <= 0) {
        u->browse_ms = 0;
        u->force = true;
    }
    if (u->msg_ms > 0) u->msg_ms -= ms;
    if (u->home_down) {
        u->home_ms += ms;
        if (u->home_ms >= DU_MENU_HOLD_MS && u->page != DU_PG_MENU) {
            du_goto_page(u, DU_PG_MENU);
            u->home_down = false;
        }
    }
}
