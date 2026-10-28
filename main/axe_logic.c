#include "axe_logic.h"

#include <stdio.h>
#include <string.h>

// sin(degrees) * 1024, rounded, with sin(0/180)=0 and sin(90/270)=±1024.
static const int16_t SIN1024[360] = {
       0,    18,    36,    54,    71,    89,   107,   125,   143,   160,   178,   195,
     213,   230,   248,   265,   282,   299,   316,   333,   350,   367,   384,   400,
     416,   433,   449,   465,   481,   496,   512,   527,   543,   558,   573,   587,
     602,   616,   630,   644,   658,   672,   685,   698,   711,   724,   737,   749,
     761,   773,   784,   796,   807,   818,   828,   839,   849,   859,   868,   878,
     887,   896,   904,   912,   920,   928,   935,   943,   949,   956,   962,   968,
     974,   979,   984,   989,   994,   998,  1002,  1005,  1008,  1011,  1014,  1016,
    1018,  1020,  1022,  1023,  1023,  1024,  1024,  1024,  1023,  1023,  1022,  1020,
    1018,  1016,  1014,  1011,  1008,  1005,  1002,   998,   994,   989,   984,   979,
     974,   968,   962,   956,   949,   943,   935,   928,   920,   912,   904,   896,
     887,   878,   868,   859,   849,   839,   828,   818,   807,   796,   784,   773,
     761,   749,   737,   724,   711,   698,   685,   672,   658,   644,   630,   616,
     602,   587,   573,   558,   543,   527,   512,   496,   481,   465,   449,   433,
     416,   400,   384,   367,   350,   333,   316,   299,   282,   265,   248,   230,
     213,   195,   178,   160,   143,   125,   107,    89,    71,    54,    36,    18,
       0,   -18,   -36,   -54,   -71,   -89,  -107,  -125,  -143,  -160,  -178,  -195,
    -213,  -230,  -248,  -265,  -282,  -299,  -316,  -333,  -350,  -367,  -384,  -400,
    -416,  -433,  -449,  -465,  -481,  -496,  -512,  -527,  -543,  -558,  -573,  -587,
    -602,  -616,  -630,  -644,  -658,  -672,  -685,  -698,  -711,  -724,  -737,  -749,
    -761,  -773,  -784,  -796,  -807,  -818,  -828,  -839,  -849,  -859,  -868,  -878,
    -887,  -896,  -904,  -912,  -920,  -928,  -935,  -943,  -949,  -956,  -962,  -968,
    -974,  -979,  -984,  -989,  -994,  -998, -1002, -1005, -1008, -1011, -1014, -1016,
   -1018, -1020, -1022, -1023, -1023, -1024, -1024, -1024, -1023, -1023, -1022, -1020,
   -1018, -1016, -1014, -1011, -1008, -1005, -1002,  -998,  -994,  -989,  -984,  -979,
    -974,  -968,  -962,  -956,  -949,  -943,  -935,  -928,  -920,  -912,  -904,  -896,
    -887,  -878,  -868,  -859,  -849,  -839,  -828,  -818,  -807,  -796,  -784,  -773,
    -761,  -749,  -737,  -724,  -711,  -698,  -685,  -672,  -658,  -644,  -630,  -616,
    -602,  -587,  -573,  -558,  -543,  -527,  -512,  -496,  -481,  -465,  -449,  -433,
    -416,  -400,  -384,  -367,  -350,  -333,  -316,  -299,  -282,  -265,  -248,  -230,
    -213,  -195,  -178,  -160,  -143,  -125,  -107,   -89,   -71,   -54,   -36,   -18,
};

static const char *const COMMENTS_HANDS[AXE_COMMENT_VARIANTS] = {
    "飞哥，你这一斧砍空了",
    "飞哥，你连一个小兵都没碰到",
    "飞哥，你的斧子去度假了",
};
static const char *const COMMENTS_CRY[AXE_COMMENT_VARIANTS] = {
    "飞哥，你只蹭到一点点",
    "飞哥，你这波有点难绷",
    "飞哥，你再瞄准一次吧",
};
static const char *const COMMENTS_CUTE[AXE_COMMENT_VARIANTS] = {
    "飞哥，你砍中了一半左右",
    "飞哥，你这表情有点呆",
    "飞哥，你打得还算稳",
};
static const char *const COMMENTS_SHADES[AXE_COMMENT_VARIANTS] = {
    "飞哥，你这波很帅",
    "飞哥，你几乎清完了",
    "飞哥，你戴上墨镜也配",
};
static const char *const COMMENTS_TEETH[AXE_COMMENT_VARIANTS] = {
    "飞哥，你十个全中",
    "飞哥，你这斧王太狠了",
    "飞哥，你完美收割",
};

static const char *const DISPLAY_STRINGS[] = {
    AXE_TEXT_TITLE,
    AXE_TEXT_COVER_HINT,
    AXE_TEXT_PREVIEW,
    AXE_TEXT_AIM_CW,
    AXE_TEXT_AIM_CCW,
    AXE_TEXT_LOCK_ANGLE,
    AXE_TEXT_DISTANCE,
    AXE_TEXT_LOCK_DISTANCE,
    AXE_TEXT_RESOLVE,
    AXE_TEXT_AGAIN,
    AXE_TEXT_BATTERY_UNKNOWN,
    "命中 %d/10",
    "%d%%",
    COMMENTS_HANDS[0], COMMENTS_HANDS[1], COMMENTS_HANDS[2],
    COMMENTS_CRY[0], COMMENTS_CRY[1], COMMENTS_CRY[2],
    COMMENTS_CUTE[0], COMMENTS_CUTE[1], COMMENTS_CUTE[2],
    COMMENTS_SHADES[0], COMMENTS_SHADES[1], COMMENTS_SHADES[2],
    COMMENTS_TEETH[0], COMMENTS_TEETH[1], COMMENTS_TEETH[2],
};

static int mul1024(int value, int factor1024) {
    int64_t product = (int64_t)value * (int64_t)factor1024;
    if (product >= 0) return (int)((product + 512) / 1024);
    return (int)((product - 512) / 1024);
}

static int sin1024(int deg) {
    return SIN1024[axe_wrap_deg(deg)];
}

static int rng_below(axe_rng_t *rng, int span) {
    if (span <= 1) return 0;
    return (int)(axe_rng_next(rng) % (uint32_t)span);
}

static int dist2_at_least(int ax, int ay, int bx, int by, int gap) {
    int64_t dx = (int64_t)ax - bx;
    int64_t dy = (int64_t)ay - by;
    int64_t need = (int64_t)gap * (int64_t)gap;
    return dx * dx + dy * dy >= need;
}

static const char *const *comment_table(int band, int *count) {
    *count = AXE_COMMENT_VARIANTS;
    switch (band) {
    case AXE_BAND_HANDS: return COMMENTS_HANDS;
    case AXE_BAND_CRY: return COMMENTS_CRY;
    case AXE_BAND_CUTE: return COMMENTS_CUTE;
    case AXE_BAND_SHADES: return COMMENTS_SHADES;
    case AXE_BAND_TEETH: return COMMENTS_TEETH;
    default:
        *count = 0;
        return NULL;
    }
}

static void place_minions(axe_point_t out[AXE_MINION_COUNT], axe_point_t camp, int rotation) {
    const int inner = (AXE_CLUSTER_R * 2) / 5;
    const int outer = (AXE_CLUSTER_R * 7) / 10;
    out[0] = camp;
    for (int i = 0; i < 4; i++) {
        int angle = rotation + i * 90;
        out[1 + i].x = camp.x + mul1024(inner, sin1024(angle));
        out[1 + i].y = camp.y + mul1024(inner, -sin1024(angle + 90));
    }
    for (int i = 0; i < 5; i++) {
        int angle = rotation + 36 + i * 72;
        out[5 + i].x = camp.x + mul1024(outer, sin1024(angle));
        out[5 + i].y = camp.y + mul1024(outer, -sin1024(angle + 90));
    }
}

int axe_wrap_deg(int deg) {
    int wrapped = deg % 360;
    if (wrapped < 0) wrapped += 360;
    return wrapped;
}

bool axe_point_in_circle(int px, int py, int cx, int cy, int radius) {
    if (radius < 0) return false;
    int64_t dx = (int64_t)px - cx;
    int64_t dy = (int64_t)py - cy;
    int64_t r = radius;
    return dx * dx + dy * dy <= r * r;
}

int axe_count_hits(const axe_point_t *minions, int count, int cx, int cy, int radius) {
    if (!minions || count <= 0) return 0;
    int hits = 0;
    for (int i = 0; i < count; i++) {
        if (axe_point_in_circle(minions[i].x, minions[i].y, cx, cy, radius)) hits++;
    }
    return hits;
}

void axe_landing(int origin_x, int origin_y, int angle_deg, int distance, int *x, int *y) {
    int dx = mul1024(distance, sin1024(angle_deg));
    int dy = mul1024(distance, -sin1024(angle_deg + 90));
    if (x) *x = origin_x + dx;
    if (y) *y = origin_y + dy;
}

int axe_max_distance(int origin_x, int origin_y, int angle_deg, const axe_field_t *field) {
    if (!field) return 0;
    int lo = 0;
    int hi = 400;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        int x = origin_x;
        int y = origin_y;
        axe_landing(origin_x, origin_y, angle_deg, mid, &x, &y);
        if (x >= field->min_x && x <= field->max_x && y >= field->min_y && y <= field->max_y) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    return lo;
}

int axe_oscillating_distance(int max_distance, int tick, int period) {
    if (max_distance <= 0 || period < 2 || tick < 0) return 0;
    int half = period / 2;
    if (half <= 0) return 0;
    int phase = tick % period;
    if (phase > half) phase = period - phase;
    return (int)(((int64_t)max_distance * phase) / half);
}

int axe_preview_digit(int elapsed_ms) {
    if (elapsed_ms < 0) elapsed_ms = 0;
    if (elapsed_ms >= AXE_PREVIEW_MS) return 0;
    return (AXE_PREVIEW_MS - elapsed_ms + 999) / 1000;
}

bool axe_preview_done(int elapsed_ms) {
    return elapsed_ms >= AXE_PREVIEW_MS;
}

int axe_lerp(int start, int end, int numerator, int denominator) {
    if (denominator <= 0 || numerator >= denominator) return end;
    if (numerator <= 0) return start;
    return start + (int)(((int64_t)(end - start) * numerator) / denominator);
}

int axe_apply_rotation(int angle_deg, int steps, int degrees_per_step) {
    return axe_wrap_deg(angle_deg + steps * degrees_per_step);
}

int axe_score_band(int hits) {
    if (hits < 0 || hits > AXE_MINION_COUNT) return -1;
    if (hits == 0) return AXE_BAND_HANDS;
    if (hits <= 3) return AXE_BAND_CRY;
    if (hits <= 6) return AXE_BAND_CUTE;
    if (hits <= 9) return AXE_BAND_SHADES;
    return AXE_BAND_TEETH;
}

const char *axe_comment(int band, int index) {
    int count = 0;
    const char *const *table = comment_table(band, &count);
    if (!table || count <= 0) return NULL;
    if (index < 0) index = -index;
    return table[index % count];
}

int axe_comment_count(int band) {
    int count = 0;
    comment_table(band, &count);
    return count;
}

void axe_format_hits(char *buf, size_t size, int hits) {
    if (!buf || size == 0) return;
    snprintf(buf, size, "命中 %d/10", hits);
}

void axe_round_begin(axe_round_t *round, axe_rng_t *rng, const axe_field_t *field) {
    if (!round || !rng || !field) return;
    memset(round, 0, sizeof(*round));
    int width = field->max_x - field->min_x + 1;
    int height = field->max_y - field->min_y + 1;
    bool placed = false;
    if (width > 0 && height > 0) {
        for (int attempt = 0; attempt < 48; attempt++) {
            round->king.x = field->min_x + rng_below(rng, width);
            round->king.y = field->min_y + rng_below(rng, height);
            round->camp.x = field->min_x + rng_below(rng, width);
            round->camp.y = field->min_y + rng_below(rng, height);
            if (dist2_at_least(round->king.x, round->king.y, round->camp.x, round->camp.y,
                               field->min_center_gap)) {
                placed = true;
                break;
            }
        }
    }
    if (!placed) {
        round->king.x = field->min_x;
        round->king.y = field->min_y;
        round->camp.x = field->max_x;
        round->camp.y = field->max_y;
    }
    place_minions(round->minions, round->camp, rng_below(rng, 360));
    round->comment_index = rng_below(rng, AXE_COMMENT_VARIANTS);
}

void axe_resolve(axe_round_t *round, int angle_deg, int distance_px, const axe_field_t *field) {
    if (!round || !field) return;
    round->angle_deg = axe_wrap_deg(angle_deg);
    round->distance_px = distance_px < 0 ? 0 : distance_px;
    axe_landing(round->king.x, round->king.y, round->angle_deg, round->distance_px,
                &round->landing_x, &round->landing_y);
    round->hits = axe_count_hits(round->minions, AXE_MINION_COUNT,
                                  round->landing_x, round->landing_y, field->cluster_radius);
}

bool axe_round_valid(const axe_round_t *round, const axe_field_t *field) {
    if (!round || !field) return false;
    if (round->king.x < field->min_x || round->king.x > field->max_x) return false;
    if (round->king.y < field->min_y || round->king.y > field->max_y) return false;
    if (round->camp.x < field->min_x || round->camp.x > field->max_x) return false;
    if (round->camp.y < field->min_y || round->camp.y > field->max_y) return false;
    if (!dist2_at_least(round->king.x, round->king.y, round->camp.x, round->camp.y,
                        field->min_center_gap)) {
        return false;
    }
    for (int i = 0; i < AXE_MINION_COUNT; i++) {
        if (!axe_point_in_circle(round->minions[i].x, round->minions[i].y,
                                 round->camp.x, round->camp.y, field->cluster_radius)) {
            return false;
        }
    }
    return true;
}

axe_phase_t axe_next_phase(axe_phase_t phase, axe_input_t input) {
    switch (phase) {
    case AXE_PHASE_COVER:
        return input == AXE_INPUT_ANY ? AXE_PHASE_PREVIEW : phase;
    case AXE_PHASE_AIM_ANGLE:
        return input == AXE_INPUT_CONFIRM ? AXE_PHASE_AIM_DISTANCE : phase;
    case AXE_PHASE_AIM_DISTANCE:
        return input == AXE_INPUT_CONFIRM ? AXE_PHASE_RESOLVE : phase;
    case AXE_PHASE_RESULT:
        return input == AXE_INPUT_CONFIRM ? AXE_PHASE_COVER : phase;
    case AXE_PHASE_PREVIEW:
    case AXE_PHASE_RESOLVE:
    default:
        return phase;
    }
}

const char *const *axe_display_strings(int *count) {
    if (count) *count = (int)(sizeof(DISPLAY_STRINGS) / sizeof(DISPLAY_STRINGS[0]));
    return DISPLAY_STRINGS;
}
