#include "ui.h"
#include "led_ring.h"
#include "ui_theme.h"
#include "ui_backgrounds.h"
#include "ui_background_state.h"
#include <lvgl.h>
#include <math.h>

/* Tick-Ring (VU/Studio-Look):
 * 270°-Ring feiner Ticks mit Gap unten, drei gleich große Farbzonen
 * (grün / amber / rot). Nur Ticks bis zum aktuellen Pegel leuchten,
 * der Rest ist gedimmt. Zahl mittig, Disconnect-Punkt in der unteren Gap. */

#define TICK_COUNT     37            /* 0..36 -> 36 Segmente */
#define ARC_START_DEG  135.0f        /* unten-links (≈ 7:30 Uhr) */
#define ARC_SWEEP_DEG  270.0f        /* 270° Sweep, Gap unten */
#define CENTER         120           /* 240×240 Display */
#define R_OUT          60
#define R_IN           50
#define TICK_WIDTH     3
#define TICK_GLOW_WIDTH 7
#define SYMBOL_PLAY_PAUSE LV_SYMBOL_PLAY " " LV_SYMBOL_PAUSE
#define HINT_COLOR     lv_color_make(210, 210, 210)
#define MEDIA_COLOR    lv_color_make(32, 230, 255)

static lv_obj_t*  s_tick_glow[TICK_COUNT];
static lv_obj_t*  s_ticks[TICK_COUNT];
static lv_point_t s_tick_pts[TICK_COUNT][2];
static lv_obj_t*  s_bg_img;
static lv_obj_t*  s_vol_label;
static lv_obj_t*  s_mute_label;
static lv_obj_t*  s_mute_cross_a;
static lv_obj_t*  s_mute_cross_b;
static lv_point_t s_mute_cross_a_pts[2] = {{137, 106}, {155, 134}};
static lv_point_t s_mute_cross_b_pts[2] = {{155, 106}, {137, 134}};
static lv_obj_t*  s_conn_dot;
static lv_obj_t*  s_hint_prev;
static lv_obj_t*  s_hint_play;
static lv_obj_t*  s_hint_next;
static lv_obj_t*  s_feedback_label;
static lv_obj_t*  s_boot_caption;   /* "FLASH-MODUS" über dem Countdown */
static lv_obj_t*  s_boot_digit;     /* große Countdown-Ziffer 5..0 */

/* Theme-Switcher-Overlay (Variante B): Eyebrow + Name-Pille + Positions-Punkte. */
#define SW_MAX_DOTS 8
static lv_obj_t*  s_sw_eyebrow;
static lv_obj_t*  s_sw_name;
static lv_obj_t*  s_sw_dots[SW_MAX_DOTS];

static int  s_value = 50;
static bool s_muted = false;
static UiBackgroundBand s_bg_band = UI_BG_04;
static int  s_theme = 0;          /* Index in UI_THEMES */
static bool s_sw_open = false;    /* Switcher-Overlay aktiv? */
static int  s_sw_sel  = 0;        /* markierter Index im Switcher */

static inline lv_color_t rgb(UiRgb c) { return lv_color_make(c.r, c.g, c.b); }

static const lv_img_dsc_t* background_src(int theme, UiBackgroundBand band) {
    const UiThemeDef& t = UI_THEMES[theme];
    int b = (int)band;
    if (b < 0) b = 0;
    if (b >= t.band_count) b = t.band_count - 1;
    return t.backgrounds[b];
}

static void update_background() {
    if (!s_bg_img) return;
    lv_img_set_src(s_bg_img, background_src(s_theme, s_bg_band));
    lv_obj_move_background(s_bg_img);
}

static void apply_background(int pct) {
    if (s_sw_open) return;   /* Switcher zeigt Live-Preview — Volume-Pushes ändern den BG nicht */
    UiBackgroundBand next = ui_background_band_for_volume(s_bg_band, pct);
    if (next == s_bg_band) return;

    s_bg_band = next;
    update_background();
}

static lv_color_t zone_color(int i) {
    const UiThemeDef& t = UI_THEMES[s_theme];
    int third = TICK_COUNT / 3;
    if (i < third)      return rgb(t.zone_low);
    if (i < 2 * third)  return rgb(t.zone_mid);
    return rgb(t.zone_high);
}

static void apply_ticks() {
    /* gerundeter Index des höchsten leuchtenden Ticks */
    int lit = (s_value * (TICK_COUNT - 1) + 50) / 100;
    lv_color_t dim = rgb(UI_THEMES[s_theme].dim);
    for (int i = 0; i < TICK_COUNT; i++) {
        bool on = !s_muted && i <= lit;
        lv_color_t c = on ? zone_color(i) : dim;
        lv_obj_set_style_line_color(s_ticks[i], c, 0);
        lv_obj_set_style_line_color(s_tick_glow[i], c, 0);
        lv_obj_set_style_line_opa(s_tick_glow[i], on ? LV_OPA_50 : LV_OPA_TRANSP, 0);
    }
}

static void apply_theme_visibility() {
    if (!s_hint_prev || !s_hint_play || !s_hint_next) return;

    /* Im Switcher bleiben die Hints immer aus — das Overlay übernimmt den Screen. */
    bool show = !s_sw_open && UI_THEMES[s_theme].show_media_hints;
    if (show) {
        lv_obj_clear_flag(s_hint_prev, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_hint_play, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_hint_next, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_hint_prev, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_hint_play, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_hint_next, LV_OBJ_FLAG_HIDDEN);
    }
}

static void style_icon_label(lv_obj_t* obj, const lv_font_t* font, lv_color_t color, lv_opa_t opa) {
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, color, 0);
    lv_obj_set_style_text_opa(obj, opa, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void set_text_opa(void* obj, int32_t opa) {
    lv_obj_set_style_text_opa((lv_obj_t*)obj, (lv_opa_t)opa, 0);
}

static void hide_anim_target(lv_anim_t* anim) {
    lv_obj_add_flag((lv_obj_t*)anim->var, LV_OBJ_FLAG_HIDDEN);
}

static void restore_hint_color(lv_anim_t* anim) {
    lv_obj_set_style_text_color((lv_obj_t*)anim->var, HINT_COLOR, 0);
    lv_obj_set_style_text_opa((lv_obj_t*)anim->var, LV_OPA_60, 0);
}

static void animate_text_opa(lv_obj_t* obj, lv_opa_t from, lv_opa_t to, uint16_t time, uint16_t delay, lv_anim_ready_cb_t ready_cb) {
    lv_anim_del(obj, set_text_opa);
    lv_obj_set_style_text_opa(obj, from, 0);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_exec_cb(&anim, set_text_opa);
    lv_anim_set_values(&anim, from, to);
    lv_anim_set_time(&anim, time);
    lv_anim_set_delay(&anim, delay);
    if (ready_cb) lv_anim_set_ready_cb(&anim, ready_cb);
    lv_anim_start(&anim);
}

void ui_init() {
    ledRingInit();

    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    s_bg_img = lv_img_create(scr);
    s_bg_band = ui_background_band_for_volume(s_bg_band, s_value);
    lv_img_set_src(s_bg_img, background_src(s_theme, s_bg_band));
    lv_obj_align(s_bg_img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(s_bg_img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_background(s_bg_img);

    /* Tick-Ring: jeder Tick ist eine kurze radiale Linie */
    for (int i = 0; i < TICK_COUNT; i++) {
        float frac = (float)i / (float)(TICK_COUNT - 1);
        float rad  = (ARC_START_DEG + frac * ARC_SWEEP_DEG) * (float)M_PI / 180.0f;
        float c = cosf(rad), s = sinf(rad);
        s_tick_pts[i][0].x = (lv_coord_t)(CENTER + R_IN  * c);
        s_tick_pts[i][0].y = (lv_coord_t)(CENTER + R_IN  * s);
        s_tick_pts[i][1].x = (lv_coord_t)(CENTER + R_OUT * c);
        s_tick_pts[i][1].y = (lv_coord_t)(CENTER + R_OUT * s);

        lv_obj_t* glow = lv_line_create(scr);
        lv_line_set_points(glow, s_tick_pts[i], 2);
        lv_obj_set_pos(glow, 0, 0);
        lv_obj_set_style_line_width(glow, TICK_GLOW_WIDTH, 0);
        lv_obj_set_style_line_rounded(glow, true, 0);
        lv_obj_set_style_line_opa(glow, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(glow, LV_OBJ_FLAG_SCROLLABLE);
        s_tick_glow[i] = glow;

        lv_obj_t* ln = lv_line_create(scr);
        lv_line_set_points(ln, s_tick_pts[i], 2);
        lv_obj_set_pos(ln, 0, 0);
        lv_obj_set_style_line_width(ln, TICK_WIDTH, 0);
        lv_obj_set_style_line_rounded(ln, true, 0);
        lv_obj_clear_flag(ln, LV_OBJ_FLAG_SCROLLABLE);
        s_ticks[i] = ln;
    }

    /* Volume-Label: große zentrierte Zahl */
    s_vol_label = lv_label_create(scr);
    lv_label_set_text(s_vol_label, "50%");
    lv_obj_set_style_text_font(s_vol_label, &lv_font_montserrat_34, 0);
    lv_obj_set_style_text_color(s_vol_label, lv_color_white(), 0);
    lv_obj_center(s_vol_label);

    /* Gesture-Hints: kleine Symbole dort, wo die Touch-Aktion stattfindet. */
    s_hint_prev = lv_label_create(scr);
    lv_label_set_text(s_hint_prev, LV_SYMBOL_PREV);
    style_icon_label(s_hint_prev, &lv_font_montserrat_24, HINT_COLOR, LV_OPA_60);
    lv_obj_align(s_hint_prev, LV_ALIGN_LEFT_MID, 18, 0);

    s_hint_play = lv_label_create(scr);
    lv_label_set_text(s_hint_play, SYMBOL_PLAY_PAUSE);
    style_icon_label(s_hint_play, &lv_font_montserrat_20, HINT_COLOR, LV_OPA_60);
    lv_obj_align(s_hint_play, LV_ALIGN_TOP_MID, 0, 18);

    s_hint_next = lv_label_create(scr);
    lv_label_set_text(s_hint_next, LV_SYMBOL_NEXT);
    style_icon_label(s_hint_next, &lv_font_montserrat_24, HINT_COLOR, LV_OPA_60);
    lv_obj_align(s_hint_next, LV_ALIGN_RIGHT_MID, -18, 0);

    /* Mute-Icon: zentriert, standardmäßig ausgeblendet */
    s_mute_label = lv_label_create(scr);
    lv_label_set_text(s_mute_label, LV_SYMBOL_MUTE);
    lv_obj_set_style_text_font(s_mute_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_mute_label, lv_color_make(255, 80, 80), 0);
    lv_obj_align(s_mute_label, LV_ALIGN_CENTER, -10, 0);
    lv_obj_add_flag(s_mute_label, LV_OBJ_FLAG_HIDDEN);

    s_mute_cross_a = lv_line_create(scr);
    lv_line_set_points(s_mute_cross_a, s_mute_cross_a_pts, 2);
    lv_obj_set_style_line_width(s_mute_cross_a, 5, 0);
    lv_obj_set_style_line_color(s_mute_cross_a, lv_color_make(255, 80, 80), 0);
    lv_obj_set_style_line_rounded(s_mute_cross_a, true, 0);
    lv_obj_add_flag(s_mute_cross_a, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_mute_cross_a, LV_OBJ_FLAG_SCROLLABLE);

    s_mute_cross_b = lv_line_create(scr);
    lv_line_set_points(s_mute_cross_b, s_mute_cross_b_pts, 2);
    lv_obj_set_style_line_width(s_mute_cross_b, 5, 0);
    lv_obj_set_style_line_color(s_mute_cross_b, lv_color_make(255, 80, 80), 0);
    lv_obj_set_style_line_rounded(s_mute_cross_b, true, 0);
    lv_obj_add_flag(s_mute_cross_b, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_mute_cross_b, LV_OBJ_FLAG_SCROLLABLE);

    /* Zentrales Media-Feedback, kurz eingeblendet nach Swipe/Double-Tap. */
    s_feedback_label = lv_label_create(scr);
    lv_label_set_text(s_feedback_label, SYMBOL_PLAY_PAUSE);
    style_icon_label(s_feedback_label, &lv_font_montserrat_48, MEDIA_COLOR, LV_OPA_COVER);
    lv_obj_center(s_feedback_label);
    lv_obj_add_flag(s_feedback_label, LV_OBJ_FLAG_HIDDEN);

    /* Disconnect-Punkt: kleiner roter Kreis in der unteren Gap.
     * Sichtbar, solange die Companion-App nicht verbunden ist. */
    s_conn_dot = lv_obj_create(scr);
    lv_obj_set_size(s_conn_dot, 14, 14);
    lv_obj_align(s_conn_dot, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_radius(s_conn_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_conn_dot, lv_color_make(255, 40, 40), 0);
    lv_obj_set_style_border_width(s_conn_dot, 0, 0);
    lv_obj_clear_flag(s_conn_dot, LV_OBJ_FLAG_SCROLLABLE);

    /* Flash-Countdown-Overlay: Caption + große Ziffer, standardmäßig versteckt.
     * Wird nur unmittelbar vor dem Bootloader-Reset gezeigt (kein Restore). */
    s_boot_caption = lv_label_create(scr);
    lv_label_set_text(s_boot_caption, "FLASH-MODUS");
    style_icon_label(s_boot_caption, &lv_font_montserrat_24, lv_color_make(255, 170, 0), LV_OPA_COVER);
    lv_obj_align(s_boot_caption, LV_ALIGN_CENTER, 0, -40);
    lv_obj_add_flag(s_boot_caption, LV_OBJ_FLAG_HIDDEN);

    s_boot_digit = lv_label_create(scr);
    lv_label_set_text(s_boot_digit, "5");
    lv_obj_set_style_text_font(s_boot_digit, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_boot_digit, lv_color_white(), 0);
    lv_obj_align(s_boot_digit, LV_ALIGN_CENTER, 0, 12);
    lv_obj_add_flag(s_boot_digit, LV_OBJ_FLAG_HIDDEN);

    /* Theme-Switcher-Overlay (standardmäßig versteckt) ---------------------- */
    s_sw_eyebrow = lv_label_create(scr);
    lv_label_set_text(s_sw_eyebrow, "THEME");
    style_icon_label(s_sw_eyebrow, &lv_font_montserrat_14, lv_color_make(180, 180, 180), LV_OPA_70);
    lv_obj_align(s_sw_eyebrow, LV_ALIGN_CENTER, 0, -34);
    lv_obj_add_flag(s_sw_eyebrow, LV_OBJ_FLAG_HIDDEN);

    s_sw_name = lv_label_create(scr);
    lv_label_set_text(s_sw_name, "Plasma");
    lv_obj_set_style_text_font(s_sw_name, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_sw_name, lv_color_white(), 0);
    lv_obj_set_style_bg_color(s_sw_name, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_sw_name, LV_OPA_40, 0);
    lv_obj_set_style_pad_left(s_sw_name, 18, 0);
    lv_obj_set_style_pad_right(s_sw_name, 18, 0);
    lv_obj_set_style_pad_top(s_sw_name, 8, 0);
    lv_obj_set_style_pad_bottom(s_sw_name, 8, 0);
    lv_obj_set_style_radius(s_sw_name, 22, 0);
    lv_obj_set_style_border_width(s_sw_name, 1, 0);
    lv_obj_set_style_border_color(s_sw_name, lv_color_make(200, 200, 200), 0);
    lv_obj_set_style_border_opa(s_sw_name, LV_OPA_50, 0);
    lv_obj_align(s_sw_name, LV_ALIGN_CENTER, 0, 2);
    lv_obj_clear_flag(s_sw_name, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_sw_name, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < SW_MAX_DOTS; i++) {
        lv_obj_t* dot = lv_obj_create(scr);
        lv_obj_set_size(dot, 7, 7);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_white(), 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
        s_sw_dots[i] = dot;
    }

    apply_ticks();
    apply_theme_visibility();
}

void ui_set_volume(int pct) {
    s_value = pct;
    apply_background(pct);
    lv_label_set_text_fmt(s_vol_label, "%d%%", pct);
    apply_ticks();
}

void ui_set_mute(bool muted) {
    s_muted = muted;
    ledRingSetMute(muted);
    if (muted) {
        lv_obj_add_flag(s_bg_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_vol_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_mute_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_mute_cross_a, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_mute_cross_b, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_mute_cross_a);
        lv_obj_move_foreground(s_mute_cross_b);
    } else {
        lv_obj_clear_flag(s_bg_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_background(s_bg_img);
        lv_obj_clear_flag(s_vol_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_mute_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_mute_cross_a, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_mute_cross_b, LV_OBJ_FLAG_HIDDEN);
    }
    apply_ticks();
}

void ui_theme_set(int index) {
    if (index < 0 || index >= UI_THEME_COUNT) index = 0;
    s_theme = index;
    update_background();
    apply_ticks();
    apply_theme_visibility();
}

int ui_theme_get()   { return s_theme; }
int ui_theme_count() { return UI_THEME_COUNT; }

const char* ui_theme_name(int index) {
    if (index < 0 || index >= UI_THEME_COUNT) return "";
    return UI_THEMES[index].name;
}

// HUD = reguläre Hauptansicht (Tick-Ring + Volume-Zahl). Im Switcher ausgeblendet,
// beim Schließen wiederhergestellt. Conn-Dot/Hints/Mute werden separat reasserted.
static void hud_set_hidden(bool hidden) {
    for (int i = 0; i < TICK_COUNT; i++) {
        if (hidden) {
            lv_obj_add_flag(s_ticks[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_tick_glow[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(s_ticks[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_tick_glow[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (hidden) lv_obj_add_flag(s_vol_label, LV_OBJ_FLAG_HIDDEN);
    else        lv_obj_clear_flag(s_vol_label, LV_OBJ_FLAG_HIDDEN);
}

static void render_switcher() {
    lv_label_set_text(s_sw_name, ui_theme_name(s_sw_sel));
    lv_obj_align(s_sw_name, LV_ALIGN_CENTER, 0, 2);   /* nach Textänderung neu zentrieren */

    int count = UI_THEME_COUNT;
    if (count > SW_MAX_DOTS) count = SW_MAX_DOTS;
    const int spacing = 14;
    int span = (count - 1) * spacing;
    for (int i = 0; i < SW_MAX_DOTS; i++) {
        if (i >= count) { lv_obj_add_flag(s_sw_dots[i], LV_OBJ_FLAG_HIDDEN); continue; }
        bool on = (i == s_sw_sel);
        lv_obj_clear_flag(s_sw_dots[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(s_sw_dots[i], on ? 16 : 7, 7);
        lv_obj_set_style_bg_opa(s_sw_dots[i], on ? LV_OPA_COVER : LV_OPA_50, 0);
        lv_obj_align(s_sw_dots[i], LV_ALIGN_BOTTOM_MID, -span / 2 + i * spacing, -26);
        lv_obj_move_foreground(s_sw_dots[i]);
    }
}

void ui_switcher_open() {
    s_sw_open = true;
    s_sw_sel  = s_theme;

    /* HUD + Mute-/Feedback-Visuals weg, Hintergrund bleibt als Live-Vorschau. */
    hud_set_hidden(true);
    lv_obj_add_flag(s_mute_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_mute_cross_a, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_mute_cross_b, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_feedback_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_conn_dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_bg_img, LV_OBJ_FLAG_HIDDEN);
    update_background();
    apply_theme_visibility();   /* Hints aus, da s_sw_open */

    lv_obj_clear_flag(s_sw_eyebrow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_sw_name, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_sw_eyebrow);
    lv_obj_move_foreground(s_sw_name);
    render_switcher();
}

void ui_switcher_scroll(int delta) {
    if (!s_sw_open || delta == 0) return;
    s_sw_sel = ui_theme_wrap(s_sw_sel + delta);
    ui_theme_set(s_sw_sel);     /* Live-Vorschau: BG + Tick-Ring (Hints bleiben aus) */
    render_switcher();
}

int ui_switcher_selected() { return s_sw_sel; }

void ui_switcher_close() {
    s_sw_open = false;

    lv_obj_add_flag(s_sw_eyebrow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_sw_name, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < SW_MAX_DOTS; i++) lv_obj_add_flag(s_sw_dots[i], LV_OBJ_FLAG_HIDDEN);

    /* Reguläre Ansicht zurück. Conn-Dot regelt ui_set_connected im nächsten Loop;
     * Mute-Visuals via ui_set_mute mit dem zuletzt bekannten Stand. */
    hud_set_hidden(false);
    apply_ticks();
    apply_theme_visibility();
    ui_set_mute(s_muted);
}

bool ui_switcher_is_open() { return s_sw_open; }

void ui_boot_countdown(int secondsLeft) {
    if (secondsLeft < 0) secondsLeft = 0;

    /* Reguläre UI ausblenden — das Overlay übernimmt den Screen. */
    lv_obj_add_flag(s_vol_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_mute_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_mute_cross_a, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_mute_cross_b, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_hint_prev, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_hint_play, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_hint_next, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_feedback_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_conn_dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_bg_img, LV_OBJ_FLAG_HIDDEN);

    /* Tick-Ring (Arc) komplett ausblenden — bleibt auch im Final-Frame weg,
     * da ui_boot_flashing() nach dem Countdown läuft. */
    for (int i = 0; i < TICK_COUNT; i++) {
        lv_obj_add_flag(s_ticks[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_tick_glow[i], LV_OBJ_FLAG_HIDDEN);
    }

    lv_label_set_text_fmt(s_boot_digit, "%d", secondsLeft);
    lv_obj_clear_flag(s_boot_caption, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_boot_digit, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_boot_caption);
    lv_obj_move_foreground(s_boot_digit);
}

void ui_boot_flashing() {
    /* Letzter Frame vor dem ROM-Reboot: statt der "1"/"0" ein klares Download-
     * Symbol, damit der eingefrorene Bootloader-Screen eindeutig "Flash" zeigt.
     * (Das reguläre UI ist zu diesem Zeitpunkt bereits von ui_boot_countdown
     *  ausgeblendet.) */
    lv_label_set_text(s_boot_digit, LV_SYMBOL_DOWNLOAD);
    lv_obj_clear_flag(s_boot_caption, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_boot_digit, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_boot_caption);
    lv_obj_move_foreground(s_boot_digit);
}

void ui_set_connected(bool connected) {
    if (connected) lv_obj_add_flag(s_conn_dot, LV_OBJ_FLAG_HIDDEN);
    else           lv_obj_clear_flag(s_conn_dot, LV_OBJ_FLAG_HIDDEN);
}

void ui_flash_media(UiMediaFeedback feedback) {
    const char* symbol = SYMBOL_PLAY_PAUSE;
    lv_obj_t* hint = s_hint_play;

    switch (feedback) {
        case UI_MEDIA_PREV:
            symbol = LV_SYMBOL_PREV;
            hint = s_hint_prev;
            break;
        case UI_MEDIA_PLAY_PAUSE:
            symbol = SYMBOL_PLAY_PAUSE;
            hint = s_hint_play;
            break;
        case UI_MEDIA_NEXT:
            symbol = LV_SYMBOL_NEXT;
            hint = s_hint_next;
            break;
    }

    lv_label_set_text(s_feedback_label, symbol);
    lv_obj_clear_flag(s_feedback_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_feedback_label);

    lv_obj_set_style_text_color(s_hint_prev, HINT_COLOR, 0);
    lv_obj_set_style_text_color(s_hint_play, HINT_COLOR, 0);
    lv_obj_set_style_text_color(s_hint_next, HINT_COLOR, 0);
    lv_obj_set_style_text_opa(s_hint_prev, LV_OPA_60, 0);
    lv_obj_set_style_text_opa(s_hint_play, LV_OPA_60, 0);
    lv_obj_set_style_text_opa(s_hint_next, LV_OPA_60, 0);
    lv_obj_set_style_text_color(hint, MEDIA_COLOR, 0);

    animate_text_opa(s_feedback_label, LV_OPA_COVER, LV_OPA_TRANSP, 360, 220, hide_anim_target);
    animate_text_opa(s_hint_prev,  LV_OPA_60, LV_OPA_60, 1, 0, NULL);
    animate_text_opa(s_hint_play,  LV_OPA_60, LV_OPA_60, 1, 0, NULL);
    animate_text_opa(s_hint_next,  LV_OPA_60, LV_OPA_60, 1, 0, NULL);
    animate_text_opa(hint, LV_OPA_COVER, LV_OPA_60, 520, 0, restore_hint_color);
}
