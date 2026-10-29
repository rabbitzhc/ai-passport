#include "axe_ui.h"

#include "axe_logic.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"

#include "esp_log.h"
#include "lvgl.h"

#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(axe_king_font_16);
LV_FONT_DECLARE(axe_king_font_24);
LV_IMAGE_DECLARE(axe_king_bead_64);
LV_IMAGE_DECLARE(axe_king_bead_96);
LV_IMAGE_DECLARE(axe_king_hands_96);
LV_IMAGE_DECLARE(axe_king_cry_96);
LV_IMAGE_DECLARE(axe_king_cute_96);
LV_IMAGE_DECLARE(axe_king_shades_96);
LV_IMAGE_DECLARE(axe_king_teeth_96);

#define AXE_Q_PLAY 64
#define AXE_Q_COVER 96
#define AXE_SPARKS 8
#define AXE_BOLTS 3
#define AXE_CRACKS 5
#define AXE_IMPACT_MS 760

static const char *TAG = "axe_ui";

#define COL_BG 0x141A2A
#define COL_TEXT 0xF4EFE2
#define COL_GOLD 0xF2C14E
#define COL_KING 0xE23B3B
#define COL_MINION 0x3DDC76
#define COL_HIT 0xFF4D4D
#define COL_THREAT 0xFF3355
#define COL_RING 0x7DFF9A
#define AXE_ARROW_LEN 36
#define AXE_ARROW_DOTS 8
#define AXE_IDLE_DIM_MS 30000
#define AXE_BACKLIGHT_ON 80
#define AXE_BACKLIGHT_DIM 8

static lv_obj_t *s_scr;
static lv_obj_t *s_layer;
static lv_obj_t *s_king;
static lv_obj_t *s_arrow[AXE_ARROW_DOTS];
static lv_obj_t *s_minions[AXE_MINION_COUNT];
static lv_obj_t *s_threat;
static lv_obj_t *s_strike;
static lv_obj_t *s_flash;
static lv_obj_t *s_burst[3];
static lv_obj_t *s_spark[AXE_SPARKS];
static lv_obj_t *s_bolt[AXE_BOLTS];
static lv_obj_t *s_crack[AXE_CRACKS];
static lv_obj_t *s_batt_shell;
static lv_obj_t *s_batt_fill;
static lv_obj_t *s_batt_nub;
static lv_obj_t *s_batt_label;
static lv_timer_t *s_timer;

static axe_phase_t s_phase;
static axe_round_t s_round;
static axe_rng_t s_rng;
static int s_elapsed_ms;
static int s_hold_dir;
static int s_hold_steps;
static int s_dist_tick;
static int s_max_distance;
static int s_jump_step;
static bool s_landed;
static bool s_impact_done;
static int s_impact_ms;
static int s_idle_ms;
static int s_backlight = AXE_BACKLIGHT_ON;
static int s_battery_soc = -1;
static int s_battery_poll;
static bool s_battery_ready;
static bool s_swallow_confirm;

static int utf8_next(const unsigned char *text, uint32_t *codepoint) {
    if (text[0] < 0x80) {
        *codepoint = text[0];
        return 1;
    }
    if ((text[0] & 0xE0) == 0xC0 && (text[1] & 0xC0) == 0x80) {
        *codepoint = ((uint32_t)(text[0] & 0x1F) << 6) | (text[1] & 0x3F);
        return 2;
    }
    if ((text[0] & 0xF0) == 0xE0 && (text[1] & 0xC0) == 0x80 && (text[2] & 0xC0) == 0x80) {
        *codepoint = ((uint32_t)(text[0] & 0x0F) << 12) |
                     ((uint32_t)(text[1] & 0x3F) << 6) | (text[2] & 0x3F);
        return 3;
    }
    if ((text[0] & 0xF8) == 0xF0 && (text[1] & 0xC0) == 0x80 &&
        (text[2] & 0xC0) == 0x80 && (text[3] & 0xC0) == 0x80) {
        *codepoint = ((uint32_t)(text[0] & 0x07) << 18) |
                     ((uint32_t)(text[1] & 0x3F) << 12) |
                     ((uint32_t)(text[2] & 0x3F) << 6) | (text[3] & 0x3F);
        return 4;
    }
    return 0;
}

static bool font_covers(const lv_font_t *font, const char *text) {
    const unsigned char *cursor = (const unsigned char *)text;
    while (*cursor) {
        uint32_t codepoint = 0;
        int used = utf8_next(cursor, &codepoint);
        if (used <= 0) return false;
        cursor += used;
        if (codepoint == '\n' || codepoint == '\r' || codepoint == '\t') continue;
        lv_font_glyph_dsc_t glyph;
        memset(&glyph, 0, sizeof(glyph));
        if (!lv_font_get_glyph_dsc(font, &glyph, codepoint, 0) || glyph.is_placeholder) {
            ESP_LOGE(TAG, "missing glyph U+%04lX", (unsigned long)codepoint);
            return false;
        }
    }
    return true;
}

static void check_fonts(void) {
    int count = 0;
    const char *const *strings = axe_display_strings(&count);
    bool ok = true;
    for (int i = 0; i < count; i++) {
        if (!font_covers(&axe_king_font_16, strings[i]) ||
            !font_covers(&axe_king_font_24, strings[i])) {
            ok = false;
        }
    }
    lv_font_glyph_dsc_t negative;
    memset(&negative, 0, sizeof(negative));
    bool negative_hit = lv_font_get_glyph_dsc(&axe_king_font_16, &negative, 0x9F98, 0) &&
                        !negative.is_placeholder;
    if (negative_hit) {
        ESP_LOGE(TAG, "U+9F98 should stay outside the subset");
        ok = false;
    }
    if (ok) ESP_LOGI(TAG, "CJK subset covers the UI strings; U+9F98 is absent");
}

static void style_flat(lv_obj_t *obj) {
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_outline_width(obj, 0, 0);
}

static lv_obj_t *dot(lv_obj_t *parent, int cx, int cy, int diameter, uint32_t color) {
    lv_obj_t *obj = lv_obj_create(parent);
    style_flat(obj);
    lv_obj_set_size(obj, diameter, diameter);
    lv_obj_set_pos(obj, cx - diameter / 2, cy - diameter / 2);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

static lv_obj_t *block(lv_obj_t *parent, int x, int y, int w, int h, int radius, uint32_t color) {
    lv_obj_t *obj = lv_obj_create(parent);
    style_flat(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

static lv_obj_t *ring(lv_obj_t *parent, int cx, int cy, int radius, uint32_t color,
                      lv_opa_t fill, int border) {
    lv_obj_t *obj = lv_obj_create(parent);
    style_flat(obj);
    lv_obj_set_size(obj, radius * 2, radius * 2);
    lv_obj_set_pos(obj, cx - radius, cy - radius);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, fill, 0);
    lv_obj_set_style_border_width(obj, border, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_border_opa(obj, border > 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    return obj;
}

static lv_obj_t *text_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                            int x, int y, int width, uint32_t color) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    if (width > 0) {
        lv_obj_set_width(label, width);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    }
    const lv_font_t *used = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    if (used != font) {
        ESP_LOGE(TAG, "label font mismatch");
    }
    return label;
}

static void center_text(lv_obj_t *label) {
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void move_circle(lv_obj_t *obj, int cx, int cy, int diameter) {
    if (!obj) return;
    lv_obj_set_size(obj, diameter, diameter);
    lv_obj_set_pos(obj, cx - diameter / 2, cy - diameter / 2);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *avatar(lv_obj_t *parent, int cx, int cy, const lv_image_dsc_t *src) {
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, src);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(img, LV_OPA_TRANSP, 0);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_TRANSP, 0);
    lv_obj_set_pos(img, cx - (int)src->header.w / 2, cy - (int)src->header.h / 2);
    return img;
}

static void move_avatar(int cx, int cy) {
    if (!s_king) return;
    lv_obj_set_pos(s_king, cx - AXE_Q_PLAY / 2, cy - AXE_Q_PLAY / 2);
}

static void layout_arrow(int cx, int cy, int angle, int length) {
    int shown = length < 8 ? 8 : length;
    for (int i = 0; i < AXE_ARROW_DOTS; i++) {
        if (!s_arrow[i]) continue;
        if (length <= 0 && i + 1 < AXE_ARROW_DOTS) {
            lv_obj_add_flag(s_arrow[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        int dist = shown * (i + 1) / AXE_ARROW_DOTS;
        int x = cx;
        int y = cy;
        axe_landing(cx, cy, angle, dist, &x, &y);
        move_circle(s_arrow[i], x, y, i + 1 == AXE_ARROW_DOTS ? 8 : 4);
    }
}

static void create_avatar(lv_obj_t *parent, int cx, int cy) {
    s_king = avatar(parent, cx, cy, &axe_king_bead_64);
}

static void create_arrow(lv_obj_t *parent, int cx, int cy, int angle, int length) {
    for (int i = 0; i < AXE_ARROW_DOTS; i++) {
        s_arrow[i] = dot(parent, cx, cy, 4, 0xFFE7A0);
    }
    layout_arrow(cx, cy, angle, length);
}

static void build_minions(lv_obj_t *parent, uint32_t color) {
    for (int i = 0; i < AXE_MINION_COUNT; i++) {
        s_minions[i] = dot(parent, s_round.minions[i].x, s_round.minions[i].y, 12, color);
    }
}

static void recolor_hits(void) {
    for (int i = 0; i < AXE_MINION_COUNT; i++) {
        if (!s_minions[i]) continue;
        bool hit = axe_point_in_circle(s_round.minions[i].x, s_round.minions[i].y,
                                       s_round.landing_x, s_round.landing_y,
                                       AXE_CLUSTER_R);
        lv_obj_set_style_bg_color(s_minions[i], lv_color_hex(hit ? COL_HIT : COL_MINION), 0);
    }
}

static void place_ring(lv_obj_t *obj, int cx, int cy, int radius) {
    if (!obj) return;
    lv_obj_set_size(obj, radius * 2, radius * 2);
    lv_obj_set_pos(obj, cx - radius, cy - radius);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void forget_dynamic(void) {
    s_king = NULL;
    s_threat = NULL;
    s_strike = NULL;
    s_flash = NULL;
    for (int i = 0; i < 3; i++) s_burst[i] = NULL;
    for (int i = 0; i < AXE_SPARKS; i++) s_spark[i] = NULL;
    for (int i = 0; i < AXE_BOLTS; i++) s_bolt[i] = NULL;
    for (int i = 0; i < AXE_CRACKS; i++) s_crack[i] = NULL;
    for (int i = 0; i < AXE_ARROW_DOTS; i++) s_arrow[i] = NULL;
    for (int i = 0; i < AXE_MINION_COUNT; i++) s_minions[i] = NULL;
}

static const lv_image_dsc_t *result_face(int band) {
    switch (band) {
    case AXE_BAND_HANDS: return &axe_king_hands_96;
    case AXE_BAND_CRY: return &axe_king_cry_96;
    case AXE_BAND_CUTE: return &axe_king_cute_96;
    case AXE_BAND_SHADES: return &axe_king_shades_96;
    case AXE_BAND_TEETH: return &axe_king_teeth_96;
    default: return &axe_king_bead_96;
    }
}

static lv_obj_t *king_ring(lv_obj_t *parent, int cx, int cy, lv_opa_t fill) {
    return ring(parent, cx, cy, AXE_CLUSTER_R, COL_THREAT, fill, 3);
}

static void build_cover(void) {
    avatar(s_layer, 120, 118, &axe_king_bead_96);
    center_text(text_label(s_layer, AXE_TEXT_TITLE, &axe_king_font_24, 12, 196, 216, COL_TEXT));
    center_text(text_label(s_layer, AXE_TEXT_COVER_HINT, &axe_king_font_16, 12, 250, 216, COL_GOLD));
}

static void build_preview(void) {
    s_threat = king_ring(s_layer, s_round.king.x, s_round.king.y, LV_OPA_30);
    ring(s_layer, s_round.camp.x, s_round.camp.y, AXE_CLUSTER_R, COL_RING, LV_OPA_TRANSP, 2);
    build_minions(s_layer, COL_MINION);
    create_avatar(s_layer, s_round.king.x, s_round.king.y);
    center_text(text_label(s_layer, AXE_TEXT_REMEMBER, &axe_king_font_16, 8, 4, 224, COL_TEXT));
}

static void build_aim(void) {
    s_threat = king_ring(s_layer, s_round.king.x, s_round.king.y, LV_OPA_30);
    create_avatar(s_layer, s_round.king.x, s_round.king.y);
    if (s_phase == AXE_PHASE_AIM_DISTANCE) {
        create_arrow(s_layer, s_round.king.x, s_round.king.y, s_round.angle_deg, 0);
        text_label(s_layer, AXE_TEXT_DISTANCE, &axe_king_font_16, 12, 8, 216, COL_TEXT);
        text_label(s_layer, AXE_TEXT_LOCK_DISTANCE, &axe_king_font_16, 12, 30, 216, COL_GOLD);
    } else {
        create_arrow(s_layer, s_round.king.x, s_round.king.y, s_round.angle_deg, AXE_ARROW_LEN);
        text_label(s_layer, AXE_TEXT_AIM_CW, &axe_king_font_16, 12, 6, 216, COL_TEXT);
        text_label(s_layer, AXE_TEXT_AIM_CCW, &axe_king_font_16, 12, 26, 216, COL_TEXT);
        text_label(s_layer, AXE_TEXT_LOCK_ANGLE, &axe_king_font_16, 12, 46, 216, COL_GOLD);
    }
}

static void build_resolve(void) {
    s_flash = block(s_layer, 0, 0, 240, 320, 0, 0xFFF4D0);
    lv_obj_set_style_bg_opa(s_flash, LV_OPA_TRANSP, 0);
    build_minions(s_layer, COL_MINION);
    s_strike = king_ring(s_layer, s_round.king.x, s_round.king.y, LV_OPA_30);
    for (int i = 0; i < 3; i++) {
        s_burst[i] = ring(s_layer, s_round.landing_x, s_round.landing_y, 8, i == 2 ? 0xFFD0A0 : COL_GOLD,
                          LV_OPA_TRANSP, 3);
        lv_obj_add_flag(s_burst[i], LV_OBJ_FLAG_HIDDEN);
    }
    for (int i = 0; i < AXE_BOLTS; i++) {
        s_bolt[i] = block(s_layer, 20 + i * 70, 30, 4, 180, 1, 0xF4FBFF);
        lv_obj_add_flag(s_bolt[i], LV_OBJ_FLAG_HIDDEN);
    }
    for (int i = 0; i < AXE_CRACKS; i++) {
        s_crack[i] = block(s_layer, s_round.landing_x, s_round.landing_y, 6, 36, 0, 0xE8EEF4);
        lv_obj_add_flag(s_crack[i], LV_OBJ_FLAG_HIDDEN);
    }
    for (int i = 0; i < AXE_SPARKS; i++) {
        s_spark[i] = dot(s_layer, s_round.landing_x, s_round.landing_y, 4, 0xFFE7A0);
        lv_obj_add_flag(s_spark[i], LV_OBJ_FLAG_HIDDEN);
    }
    create_avatar(s_layer, s_round.king.x, s_round.king.y);
    text_label(s_layer, AXE_TEXT_RESOLVE, &axe_king_font_16, 12, 8, 216, COL_TEXT);
}

static void build_result(void) {
    int band = axe_score_band(s_round.hits);
    avatar(s_layer, 120, 108, result_face(band));
    char hits[24];
    axe_format_hits(hits, sizeof(hits), s_round.hits);
    center_text(text_label(s_layer, hits, &axe_king_font_24, 12, 176, 216, COL_GOLD));
    const char *comment = axe_comment(band, s_round.comment_index);
    if (comment) {
        center_text(text_label(s_layer, comment, &axe_king_font_16, 12, 214, 216, COL_TEXT));
    }
    center_text(text_label(s_layer, AXE_TEXT_AGAIN, &axe_king_font_16, 12, 258, 216, COL_GOLD));
}

static void rebuild(void) {
    forget_dynamic();
    if (s_layer) {
        lv_obj_delete(s_layer);
        s_layer = NULL;
    }
    s_layer = lv_obj_create(s_scr);
    style_flat(s_layer);
    lv_obj_set_size(s_layer, 240, 320);
    lv_obj_set_pos(s_layer, 0, 0);
    lv_obj_set_style_bg_opa(s_layer, LV_OPA_TRANSP, 0);
    switch (s_phase) {
    case AXE_PHASE_COVER: build_cover(); break;
    case AXE_PHASE_PREVIEW: build_preview(); break;
    case AXE_PHASE_AIM_ANGLE:
    case AXE_PHASE_AIM_DISTANCE: build_aim(); break;
    case AXE_PHASE_RESOLVE: build_resolve(); break;
    case AXE_PHASE_RESULT: build_result(); break;
    default: break;
    }
    if (s_batt_shell) lv_obj_move_foreground(s_batt_shell);
    if (s_batt_fill) lv_obj_move_foreground(s_batt_fill);
    if (s_batt_nub) lv_obj_move_foreground(s_batt_nub);
    if (s_batt_label) lv_obj_move_foreground(s_batt_label);
}

static void refresh_battery(void) {
    if (!s_batt_label || !s_batt_fill) return;
    if (s_battery_ready) s_battery_soc = bsp_battery_soc();
    char buf[16];
    if (s_battery_soc < 0) {
        snprintf(buf, sizeof(buf), "%s", AXE_TEXT_BATTERY_UNKNOWN);
        lv_obj_set_style_bg_opa(s_batt_fill, LV_OPA_TRANSP, 0);
    } else {
        int soc = s_battery_soc > 100 ? 100 : s_battery_soc;
        snprintf(buf, sizeof(buf), "%d%%", soc);
        int fill = (s_battery_soc * 12) / 100;
        lv_obj_set_width(s_batt_fill, fill > 0 ? fill : 1);
        lv_obj_set_style_bg_opa(s_batt_fill, fill > 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
    lv_label_set_text(s_batt_label, buf);
}

static void set_backlight(int level) {
    if (s_backlight == level) return;
    s_backlight = level;
    bsp_display_backlight((uint8_t)level);
}

static void note_activity(void) {
    s_idle_ms = 0;
    set_backlight(AXE_BACKLIGHT_ON);
}

static void tick_power(void) {
    if (s_phase != AXE_PHASE_COVER) {
        s_idle_ms = 0;
        set_backlight(AXE_BACKLIGHT_ON);
        return;
    }
    s_idle_ms += AXE_TICK_MS;
    set_backlight(s_idle_ms >= AXE_IDLE_DIM_MS ? AXE_BACKLIGHT_DIM : AXE_BACKLIGHT_ON);
}

static void show_landing(void) {
    if (s_strike) {
        lv_obj_set_style_bg_opa(s_strike, LV_OPA_30, 0);
        lv_obj_set_style_border_opa(s_strike, LV_OPA_COVER, 0);
    }
    recolor_hits();
    s_impact_ms = 0;
    s_impact_done = false;
}

static void play_impact(void) {
    int t = s_impact_ms;
    int span = AXE_IMPACT_MS;
    if (t > span) t = span;
    static const int kick_x[] = {10, -9, 8, -7, 6, -5, 4, -3, 2, -2, 1, 0};
    static const int kick_y[] = {-6, 7, -5, 4, -3, 3, -2, 2, -1, 1, 0, 0};
    int slot = t < 440 ? (t / 40) % 12 : 11;
    if (s_layer) lv_obj_set_pos(s_layer, kick_x[slot], kick_y[slot]);
    int shake = kick_x[slot] / 2;
    place_ring(s_strike, s_round.landing_x + shake, s_round.landing_y, AXE_CLUSTER_R);
    if (s_flash) {
        int flash = 0;
        uint32_t color = 0xFFFFFF;
        if (t < 70) {
            flash = 210;
        } else if (t < 160) {
            flash = 120;
            color = 0xFF5A3A;
        } else if (t < 280) {
            flash = ((t / 40) % 2) ? 90 : 30;
            color = 0xFF3B2A;
        } else if (t < 460) {
            flash = 50 - (t - 280) / 8;
            color = 0xFF6A3A;
        }
        if (flash < 0) flash = 0;
        lv_obj_set_style_bg_color(s_flash, lv_color_hex(color), 0);
        lv_obj_set_style_bg_opa(s_flash, (lv_opa_t)flash, 0);
    }
    for (int i = 0; i < AXE_BOLTS; i++) {
        if (!s_bolt[i]) continue;
        int show = t < 220 && ((t / 40 + i) % 2) == 0;
        if (!show) {
            lv_obj_add_flag(s_bolt[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(s_bolt[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(s_bolt[i], 16 + i * 78 + (t / 40) * 6, 8 + i * 18);
        lv_obj_set_size(s_bolt[i], (i == 1) ? 3 : 5, 200 - i * 24);
    }
    for (int i = 0; i < AXE_CRACKS; i++) {
        if (!s_crack[i]) continue;
        if (t < 60 || t > 520) {
            lv_obj_add_flag(s_crack[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        int x = 0;
        int y = 0;
        axe_landing(s_round.landing_x, s_round.landing_y, 24 + i * 72, 18 + i * 8, &x, &y);
        lv_obj_set_pos(s_crack[i], x, y);
        lv_obj_set_size(s_crack[i], (i % 2) ? 34 : 8, (i % 2) ? 6 : 40);
        lv_obj_remove_flag(s_crack[i], LV_OBJ_FLAG_HIDDEN);
        int opa = 230 - (t - 60) / 2;
        if (opa < 0) opa = 0;
        lv_obj_set_style_bg_opa(s_crack[i], (lv_opa_t)opa, 0);
    }
    for (int i = 0; i < 3; i++) {
        if (!s_burst[i]) continue;
        int local = t - i * 70;
        if (local < 0) {
            lv_obj_add_flag(s_burst[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        int radius = 8 + (local * (70 + i * 16)) / span;
        place_ring(s_burst[i], s_round.landing_x, s_round.landing_y, radius);
        int opa = 240 - (local * 240) / span;
        if (opa < 0) opa = 0;
        lv_obj_set_style_bg_opa(s_burst[i], (lv_opa_t)(opa / 5), 0);
        lv_obj_set_style_border_opa(s_burst[i], (lv_opa_t)opa, 0);
    }
    for (int i = 0; i < AXE_SPARKS; i++) {
        if (!s_spark[i]) continue;
        int reach = (t * (28 + i * 6)) / span;
        int x = s_round.landing_x;
        int y = s_round.landing_y;
        axe_landing(s_round.landing_x, s_round.landing_y, i * 45, reach, &x, &y);
        move_circle(s_spark[i], x, y, (i % 3) + 3);
        if (t > span - 100) lv_obj_add_flag(s_spark[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void enter_result(void) {
    s_phase = AXE_PHASE_RESULT;
    rebuild();
}

static void on_tick(lv_timer_t *timer) {
    (void)timer;
    switch (s_phase) {
    case AXE_PHASE_PREVIEW:
        s_elapsed_ms += AXE_TICK_MS;
        if (s_threat) {
            int on = ((s_elapsed_ms / 200) % 2) == 0;
            lv_obj_set_style_bg_opa(s_threat, on ? LV_OPA_50 : LV_OPA_20, 0);
        }
        break;
    case AXE_PHASE_AIM_ANGLE:
        if (s_hold_dir != 0) {
            s_round.angle_deg = axe_apply_rotation(
                s_round.angle_deg, s_hold_dir * AXE_ROTATE_HOLD_STEPS, AXE_ROTATE_STEP_DEG);
            s_hold_steps++;
            layout_arrow(s_round.king.x, s_round.king.y, s_round.angle_deg, AXE_ARROW_LEN);
        }
        break;
    case AXE_PHASE_AIM_DISTANCE: {
        s_dist_tick += AXE_DISTANCE_ADVANCE;
        int distance = axe_oscillating_distance(s_max_distance, s_dist_tick, AXE_DISTANCE_PERIOD);
        layout_arrow(s_round.king.x, s_round.king.y, s_round.angle_deg, distance);
        break;
    }
    case AXE_PHASE_RESOLVE:
        if (!s_landed) {
            if (s_jump_step < AXE_JUMP_STEPS) s_jump_step++;
            int x = axe_ease_out(s_round.king.x, s_round.landing_x, s_jump_step, AXE_JUMP_STEPS);
            int y = axe_ease_out(s_round.king.y, s_round.landing_y, s_jump_step, AXE_JUMP_STEPS);
            move_avatar(x, y);
            place_ring(s_strike, x, y, AXE_CLUSTER_R);
            if (s_jump_step >= AXE_JUMP_STEPS) {
                s_landed = true;
                move_avatar(s_round.landing_x, s_round.landing_y);
                show_landing();
            }
        } else if (!s_impact_done) {
            s_impact_ms += AXE_TICK_MS;
            play_impact();
            if (s_impact_ms >= AXE_IMPACT_MS) {
                s_impact_done = true;
                s_elapsed_ms = 0;
                if (s_layer) lv_obj_set_pos(s_layer, 0, 0);
                if (s_flash) lv_obj_set_style_bg_opa(s_flash, LV_OPA_TRANSP, 0);
            }
        } else {
            s_elapsed_ms += AXE_TICK_MS;
            if (s_elapsed_ms >= AXE_RESOLVE_HOLD_MS) enter_result();
        }
        break;
    default:
        break;
    }

    tick_power();
    s_battery_poll += AXE_TICK_MS;
    if (s_battery_poll >= 5000) {
        s_battery_poll = 0;
        refresh_battery();
    }
}

static bool confirm_event(int button, int event) {
    return button == (int)BSP_BTN_OK &&
           (event == (int)BSP_BTN_CLICK || event == (int)BSP_BTN_DOUBLE ||
            event == (int)BSP_BTN_LONG);
}

static void rotate_by_hold(int direction) {
    s_round.angle_deg = axe_apply_rotation(s_round.angle_deg, direction, AXE_ROTATE_STEP_DEG);
    layout_arrow(s_round.king.x, s_round.king.y, s_round.angle_deg, AXE_ARROW_LEN);
}

static void lock_from_confirm(void) {
    axe_field_t field = axe_default_field();
    if (s_phase == AXE_PHASE_PREVIEW) {
        s_phase = axe_next_phase(AXE_PHASE_PREVIEW, AXE_INPUT_CONFIRM);
        s_round.angle_deg = 0;
        s_hold_dir = 0;
        s_hold_steps = 0;
        rebuild();
    } else if (s_phase == AXE_PHASE_AIM_ANGLE) {
        s_hold_dir = 0;
        s_max_distance = axe_max_distance(s_round.king.x, s_round.king.y, s_round.angle_deg, &field);
        s_dist_tick = 0;
        s_phase = axe_next_phase(AXE_PHASE_AIM_ANGLE, AXE_INPUT_CONFIRM);
        rebuild();
    } else if (s_phase == AXE_PHASE_AIM_DISTANCE) {
        int distance = axe_oscillating_distance(s_max_distance, s_dist_tick, AXE_DISTANCE_PERIOD);
        axe_resolve(&s_round, s_round.angle_deg, distance, &field);
        s_phase = axe_next_phase(AXE_PHASE_AIM_DISTANCE, AXE_INPUT_CONFIRM);
        s_jump_step = 0;
        s_landed = false;
        s_elapsed_ms = 0;
        rebuild();
    } else if (s_phase == AXE_PHASE_RESULT) {
        s_phase = axe_next_phase(AXE_PHASE_RESULT, AXE_INPUT_CONFIRM);
        rebuild();
    }
}

void axe_ui_on_key(int button, int event) {
    note_activity();
    if (event == (int)BSP_BTN_PRESS) s_swallow_confirm = false;
    if (s_phase == AXE_PHASE_COVER && event == (int)BSP_BTN_PRESS) {
        axe_field_t field = axe_default_field();
        axe_round_begin(&s_round, &s_rng, &field);
        s_phase = axe_next_phase(AXE_PHASE_COVER, AXE_INPUT_ANY);
        s_elapsed_ms = 0;
        s_hold_dir = 0;
        /* The click or long-press that left the cover must not also skip memorize. */
        s_swallow_confirm = true;
        rebuild();
        return;
    }
    if (s_phase == AXE_PHASE_AIM_ANGLE && event == (int)BSP_BTN_PRESS) {
        if (button == (int)BSP_BTN_UP) {
            s_hold_dir = 1;
            s_hold_steps = 0;
        } else if (button == (int)BSP_BTN_DOWN) {
            s_hold_dir = -1;
            s_hold_steps = 0;
        }
        return;
    }
    if (event == (int)BSP_BTN_RELEASE) {
        int direction = 0;
        if (button == (int)BSP_BTN_UP && s_hold_dir > 0) direction = 1;
        if (button == (int)BSP_BTN_DOWN && s_hold_dir < 0) direction = -1;
        if (direction != 0) {
            if (s_hold_steps == 0 && s_phase == AXE_PHASE_AIM_ANGLE) rotate_by_hold(direction);
            s_hold_dir = 0;
        }
        return;
    }
    if (!confirm_event(button, event)) return;
    if (s_swallow_confirm) {
        s_swallow_confirm = false;
        return;
    }
    if (event == (int)BSP_BTN_LONG || event == (int)BSP_BTN_DOUBLE) s_swallow_confirm = true;
    lock_from_confirm();
}

int axe_ui_phase(void) {
    return (int)s_phase;
}

void axe_ui_start(uint32_t rng_seed, bool battery_ready) {
    axe_rng_seed(&s_rng, rng_seed);
    s_battery_ready = battery_ready;
    s_battery_soc = battery_ready ? bsp_battery_soc() : -1;
    s_phase = AXE_PHASE_COVER;
    s_idle_ms = 0;
    s_backlight = AXE_BACKLIGHT_ON;
    check_fonts();

    s_scr = lv_obj_create(NULL);
    style_flat(s_scr);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(s_scr, &axe_king_font_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(s_scr, lv_color_hex(COL_TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);

    s_batt_shell = block(s_scr, 148, 300, 18, 10, 2, 0x141A2A);
    lv_obj_set_style_border_width(s_batt_shell, 1, 0);
    lv_obj_set_style_border_color(s_batt_shell, lv_color_hex(COL_GOLD), 0);
    lv_obj_set_style_border_opa(s_batt_shell, LV_OPA_COVER, 0);
    s_batt_fill = block(s_scr, 150, 302, 1, 6, 1, COL_GOLD);
    s_batt_nub = block(s_scr, 166, 303, 2, 4, 1, COL_GOLD);
    s_batt_label = text_label(s_scr, AXE_TEXT_BATTERY_UNKNOWN, &axe_king_font_16,
                              172, 294, 60, COL_TEXT);

    lv_screen_load(s_scr);
    rebuild();
    refresh_battery();
    if (!s_timer) s_timer = lv_timer_create(on_tick, AXE_TICK_MS, NULL);
}
