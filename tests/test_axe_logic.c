#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "axe_logic.h"

static void test_circle(void) {
    assert(axe_point_in_circle(0, 0, 0, 0, 10));
    assert(axe_point_in_circle(10, 0, 0, 0, 10));
    assert(axe_point_in_circle(0, -10, 0, 0, 10));
    assert(!axe_point_in_circle(11, 0, 0, 0, 10));
    assert(axe_point_in_circle(7, 7, 0, 0, 10));
    assert(!axe_point_in_circle(8, 8, 0, 0, 10));
    assert(!axe_point_in_circle(0, 0, 0, 0, -1));
    assert(axe_point_in_circle(3, 4, 0, 0, 5));
    assert(!axe_point_in_circle(3, 5, 0, 0, 5));

    axe_point_t minions[4] = {{0, 0}, {10, 0}, {30, 0}, {6, 8}};
    assert(axe_count_hits(minions, 4, 0, 0, 10) == 3);
    assert(axe_count_hits(NULL, 4, 0, 0, 10) == 0);
    assert(axe_count_hits(minions, 0, 0, 0, 10) == 0);
}

static void test_jump(void) {
    int x = 0;
    int y = 0;
    axe_landing(100, 100, 0, 40, &x, &y);
    assert(x == 100 && y == 60);
    axe_landing(100, 100, 90, 40, &x, &y);
    assert(x == 140 && y == 100);
    axe_landing(100, 100, 180, 40, &x, &y);
    assert(x == 100 && y == 140);
    axe_landing(100, 100, 270, 40, &x, &y);
    assert(x == 60 && y == 100);
    axe_landing(100, 100, 360, 15, &x, &y);
    assert(x == 100 && y == 85);
    axe_landing(100, 100, -90, 40, &x, &y);
    assert(x == 60 && y == 100);
    axe_landing(20, 30, 45, 40, &x, &y);
    assert(x == 48 && y == 2);

    assert(axe_wrap_deg(0) == 0);
    assert(axe_wrap_deg(360) == 0);
    assert(axe_wrap_deg(-1) == 359);
    assert(axe_wrap_deg(720) == 0);
    assert(axe_apply_rotation(350, 2, 8) == 6);
    assert(axe_apply_rotation(10, -2, 8) == 354);

    axe_field_t field = axe_default_field();
    int up = axe_max_distance(field.min_x, field.min_y, 0, &field);
    assert(up == 0);
    int down = axe_max_distance(field.min_x, field.min_y, 180, &field);
    assert(down == field.max_y - field.min_y);
    int right = axe_max_distance(80, 120, 90, &field);
    int lx = 0;
    int ly = 0;
    axe_landing(80, 120, 90, right, &lx, &ly);
    assert(lx <= field.max_x && ly == 120);
    axe_landing(80, 120, 90, right + 1, &lx, &ly);
    assert(lx > field.max_x || right == 400);

    assert(axe_lerp(10, 20, 0, 8) == 10);
    assert(axe_lerp(10, 20, 8, 8) == 20);
    assert(axe_lerp(10, 50, 2, 8) == 20);
    assert(axe_lerp(50, 10, 4, 8) == 30);
}

static void test_distance_wave_and_preview(void) {
    assert(axe_oscillating_distance(20, 0, 10) == 0);
    assert(axe_oscillating_distance(20, 5, 10) == 20);
    assert(axe_oscillating_distance(20, 7, 10) == 12);
    assert(axe_oscillating_distance(20, 10, 10) == 0);
    assert(axe_oscillating_distance(0, 5, 10) == 0);
    assert(axe_oscillating_distance(20, -1, 10) == 0);
    assert(axe_oscillating_distance(20, 3, 1) == 0);

    assert(axe_preview_digit(0) == 5);
    assert(axe_preview_digit(999) == 5);
    assert(axe_preview_digit(1000) == 4);
    assert(axe_preview_digit(4000) == 1);
    assert(axe_preview_digit(4999) == 1);
    assert(axe_preview_digit(5000) == 0);
    assert(!axe_preview_done(4999));
    assert(axe_preview_done(5000));
    assert(axe_preview_digit(-10) == 5);
}

static void test_score_and_comments(void) {
    assert(axe_score_band(0) == AXE_BAND_HANDS);
    assert(axe_score_band(1) == AXE_BAND_CRY);
    assert(axe_score_band(3) == AXE_BAND_CRY);
    assert(axe_score_band(4) == AXE_BAND_CUTE);
    assert(axe_score_band(6) == AXE_BAND_CUTE);
    assert(axe_score_band(7) == AXE_BAND_SHADES);
    assert(axe_score_band(9) == AXE_BAND_SHADES);
    assert(axe_score_band(10) == AXE_BAND_TEETH);
    assert(axe_score_band(-1) == -1);
    assert(axe_score_band(11) == -1);

    static const char prefix[] = "飞哥，你";
    const size_t prefix_len = strlen(prefix);
    int seen = 0;
    const char *previous[32];
    for (int band = AXE_BAND_HANDS; band <= AXE_BAND_TEETH; band++) {
        assert(axe_comment_count(band) == AXE_COMMENT_VARIANTS);
        for (int i = 0; i < AXE_COMMENT_VARIANTS; i++) {
            const char *text = axe_comment(band, i);
            assert(text);
            assert(strncmp(text, prefix, prefix_len) == 0);
            assert(axe_comment(band, i + AXE_COMMENT_VARIANTS) == text);
            for (int p = 0; p < seen; p++) assert(strcmp(previous[p], text) != 0);
            previous[seen++] = text;
        }
    }
    assert(axe_comment(-1, 0) == NULL);
    assert(axe_comment_count(99) == 0);
    assert(axe_comment(AXE_BAND_TEETH, 0) != axe_comment(AXE_BAND_HANDS, 0));

    char buf[32];
    axe_format_hits(buf, sizeof(buf), 7);
    assert(strcmp(buf, "命中 7/10") == 0);
    axe_format_hits(buf, sizeof(buf), 0);
    assert(strcmp(buf, "命中 0/10") == 0);
    axe_format_hits(buf, sizeof(buf), 10);
    assert(strcmp(buf, "命中 10/10") == 0);
}

static void test_round_and_phase(void) {
    axe_field_t field = axe_default_field();
    axe_rng_t rng;
    axe_round_t first;
    axe_round_t second;
    axe_rng_seed(&rng, 1);
    axe_round_begin(&first, &rng, &field);
    assert(axe_round_valid(&first, &field));
    assert(first.angle_deg == 0);
    assert(first.hits == 0);
    bool varied = false;
    for (uint32_t seed = 1; seed <= 80; seed++) {
        axe_round_t round;
        axe_rng_seed(&rng, seed);
        axe_round_begin(&round, &rng, &field);
        assert(axe_round_valid(&round, &field));
        if (round.king.x != first.king.x || round.king.y != first.king.y ||
            round.camp.x != first.camp.x || round.camp.y != first.camp.y) {
            varied = true;
        }
    }
    assert(varied);

    axe_field_t impossible = field;
    impossible.min_center_gap = 10000;
    axe_rng_seed(&rng, 3);
    axe_round_begin(&second, &rng, &impossible);
    assert(second.king.x == impossible.min_x && second.king.y == impossible.min_y);
    assert(second.camp.x == impossible.max_x && second.camp.y == impossible.max_y);
    assert(!axe_round_valid(&second, &impossible));
    for (int i = 0; i < AXE_MINION_COUNT; i++) {
        assert(axe_point_in_circle(second.minions[i].x, second.minions[i].y,
                                   second.camp.x, second.camp.y, impossible.cluster_radius));
    }

    axe_round_t aimed = {0};
    aimed.king.x = 120;
    aimed.king.y = 160;
    aimed.minions[0].x = 120;
    aimed.minions[0].y = 115;
    aimed.minions[1].x = 10;
    aimed.minions[1].y = 10;
    for (int i = 2; i < AXE_MINION_COUNT; i++) {
        aimed.minions[i].x = 0;
        aimed.minions[i].y = 0;
    }
    axe_resolve(&aimed, 0, 45, &field);
    assert(aimed.landing_x == 120 && aimed.landing_y == 115);
    assert(aimed.hits == 1);
    axe_resolve(&aimed, 0, 0, &field);
    assert(aimed.landing_x == 120 && aimed.landing_y == 160);
    assert(aimed.hits == 0);

    assert(axe_next_phase(AXE_PHASE_COVER, AXE_INPUT_ANY) == AXE_PHASE_PREVIEW);
    assert(axe_next_phase(AXE_PHASE_COVER, AXE_INPUT_CONFIRM) == AXE_PHASE_COVER);
    assert(axe_next_phase(AXE_PHASE_PREVIEW, AXE_INPUT_CONFIRM) == AXE_PHASE_PREVIEW);
    assert(axe_next_phase(AXE_PHASE_AIM_ANGLE, AXE_INPUT_ROTATE_CW) == AXE_PHASE_AIM_ANGLE);
    assert(axe_next_phase(AXE_PHASE_AIM_ANGLE, AXE_INPUT_CONFIRM) == AXE_PHASE_AIM_DISTANCE);
    assert(axe_next_phase(AXE_PHASE_AIM_DISTANCE, AXE_INPUT_ROTATE_CCW) == AXE_PHASE_AIM_DISTANCE);
    assert(axe_next_phase(AXE_PHASE_AIM_DISTANCE, AXE_INPUT_CONFIRM) == AXE_PHASE_RESOLVE);
    assert(axe_next_phase(AXE_PHASE_RESOLVE, AXE_INPUT_CONFIRM) == AXE_PHASE_RESOLVE);
    assert(axe_next_phase(AXE_PHASE_RESULT, AXE_INPUT_ANY) == AXE_PHASE_RESULT);
    assert(axe_next_phase(AXE_PHASE_RESULT, AXE_INPUT_CONFIRM) == AXE_PHASE_COVER);

    int count = 0;
    const char *const *strings = axe_display_strings(&count);
    assert(count > 20);
    assert(strings[0] && strings[0][0] != '\0');
}

int main(void) {
    test_circle();
    test_jump();
    test_distance_wave_and_preview();
    test_score_and_comments();
    test_round_and_phase();
    puts("axe logic tests passed");
    return 0;
}
