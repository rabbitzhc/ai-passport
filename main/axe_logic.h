// Offline aim/hit rules for the Axe King mini-game.
//
// This header is intentionally free of ESP-IDF and LVGL so host tests can
// cover containment, the angle/distance jump, and score bands directly.
// Angles are degrees clockwise from up (0 = up, 90 = right). The strike
// circle and the minion cluster share one radius.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AXE_MINION_COUNT 10
#define AXE_CLUSTER_R 40
#define AXE_FIELD_MIN_X 46
#define AXE_FIELD_MAX_X 194
#define AXE_FIELD_MIN_Y 100
#define AXE_FIELD_MAX_Y 244
#define AXE_MIN_CENTER_GAP 72
#define AXE_PREVIEW_MS 5000
#define AXE_ROTATE_STEP_DEG 2
#define AXE_DISTANCE_PERIOD 120
#define AXE_JUMP_STEPS 20
#define AXE_RESOLVE_HOLD_MS 1000
#define AXE_TICK_MS 50
#define AXE_COMMENT_VARIANTS 3

typedef struct {
    int x;
    int y;
} axe_point_t;

typedef struct {
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int cluster_radius;
    int min_center_gap;
} axe_field_t;

typedef enum {
    AXE_PHASE_COVER = 0,
    AXE_PHASE_PREVIEW,
    AXE_PHASE_AIM_ANGLE,
    AXE_PHASE_AIM_DISTANCE,
    AXE_PHASE_RESOLVE,
    AXE_PHASE_RESULT,
} axe_phase_t;

typedef enum {
    AXE_INPUT_NONE = 0,
    AXE_INPUT_ANY,
    AXE_INPUT_CONFIRM,
    AXE_INPUT_ROTATE_CW,
    AXE_INPUT_ROTATE_CCW,
} axe_input_t;

typedef enum {
    AXE_BAND_HANDS = 0,
    AXE_BAND_CRY,
    AXE_BAND_CUTE,
    AXE_BAND_SHADES,
    AXE_BAND_TEETH,
} axe_band_t;

typedef struct {
    uint32_t state;
} axe_rng_t;

typedef struct {
    axe_point_t king;
    axe_point_t camp;
    axe_point_t minions[AXE_MINION_COUNT];
    int angle_deg;
    int distance_px;
    int landing_x;
    int landing_y;
    int hits;
    int comment_index;
} axe_round_t;

// Display copy. Every user-visible string lives here so the font subset and
// the on-device glyph check share one inventory. UI code must not introduce
// additional Chinese literals.
#define AXE_TEXT_TITLE "飞哥玩斧王"
#define AXE_TEXT_COVER_HINT "按任意键继续"
#define AXE_TEXT_REMEMBER "请记住小兵位置，按确认键继续"
#define AXE_TEXT_AIM_CW "音量+顺时针"
#define AXE_TEXT_AIM_CCW "音量-逆时针"
#define AXE_TEXT_LOCK_ANGLE "确定锁定方向"
#define AXE_TEXT_DISTANCE "距离正在伸缩"
#define AXE_TEXT_LOCK_DISTANCE "再按确定出手"
#define AXE_TEXT_RESOLVE "斧已出手"
#define AXE_TEXT_AGAIN "按确定再来一局"
#define AXE_TEXT_BATTERY_UNKNOWN "--"

static inline axe_field_t axe_default_field(void) {
    axe_field_t field = {
        .min_x = AXE_FIELD_MIN_X,
        .min_y = AXE_FIELD_MIN_Y,
        .max_x = AXE_FIELD_MAX_X,
        .max_y = AXE_FIELD_MAX_Y,
        .cluster_radius = AXE_CLUSTER_R,
        .min_center_gap = AXE_MIN_CENTER_GAP,
    };
    return field;
}

static inline void axe_rng_seed(axe_rng_t *rng, uint32_t seed) {
    rng->state = seed == 0 ? 0xA5C31E27u : seed;
}

static inline uint32_t axe_rng_next(axe_rng_t *rng) {
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

int axe_wrap_deg(int deg);
bool axe_point_in_circle(int px, int py, int cx, int cy, int radius);
int axe_count_hits(const axe_point_t *minions, int count, int cx, int cy, int radius);
void axe_landing(int origin_x, int origin_y, int angle_deg, int distance,
                 int *x, int *y);
int axe_max_distance(int origin_x, int origin_y, int angle_deg, const axe_field_t *field);
int axe_oscillating_distance(int max_distance, int tick, int period);
int axe_preview_digit(int elapsed_ms);
bool axe_preview_done(int elapsed_ms);
int axe_lerp(int start, int end, int numerator, int denominator);
int axe_ease_out(int start, int end, int numerator, int denominator);
int axe_apply_rotation(int angle_deg, int steps, int degrees_per_step);
int axe_score_band(int hits);
const char *axe_comment(int band, int index);
int axe_comment_count(int band);
void axe_format_hits(char *buf, size_t size, int hits);
void axe_round_begin(axe_round_t *round, axe_rng_t *rng, const axe_field_t *field);
void axe_resolve(axe_round_t *round, int angle_deg, int distance_px, const axe_field_t *field);
bool axe_round_valid(const axe_round_t *round, const axe_field_t *field);
axe_phase_t axe_next_phase(axe_phase_t phase, axe_input_t input);
const char *const *axe_display_strings(int *count);
