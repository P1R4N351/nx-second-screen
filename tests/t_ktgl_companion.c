// SPDX-License-Identifier: MIT
// Host unit tests for adapters/ktgl/ktgl_companion.h.
#include "ktgl/ktgl_companion.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static KtglHostSample base_sample(uint64_t now_ns) {
    KtglHostSample s = {0};
    s.enabled = 1;
    s.game_kind = KTGL_GAME_AOI;
    s.main_loaded = 1;
    s.build_ok = 1;
    s.now_ns = now_ns;
    s.fps = 30.0f;
    return s;
}

static void test_lifecycle_and_fail_closed(void) {
    KtglCompanionCtx c;
    ktgl_companion_init(&c, KTGL_GAME_AOI, 0, 0);
    assert(ktgl_build_matches("0123456789ABCDEF0123456789ABCDEF00112233", "0123456789abcdef0123456789abcdef"));
    assert(!ktgl_build_matches("fedcba9876543210fedcba9876543210", "0123456789abcdef0123456789abcdef"));

    uint64_t t = 1000000000ULL;
    KtglHostSample s = base_sample(t);
    s.main_loaded = 0; s.build_ok = 0;
    ktgl_companion_present(&c, &s);
    assert(c.state.status == COMPANION_STATE_STARTING && !ktgl_companion_write_safe(&c));

    s.main_loaded = 1; s.build_ok = 0;
    ktgl_companion_present(&c, &s);
    assert(c.state.status == COMPANION_STATE_UNSUPPORTED);

    s.build_ok = 1;
    ktgl_companion_present(&c, &s);
    assert(c.state.status == COMPANION_STATE_LOADING);
    ktgl_companion_present(&c, &s);
    assert(c.state.status == COMPANION_STATE_READY && ktgl_companion_write_safe(&c));

    s.movie_playing = 1;
    ktgl_companion_present(&c, &s);
    assert(!ktgl_companion_write_safe(&c));
    assert(ktgl_companion_fire(&c, KTGL_ACT_R_BASE, t + 1000ULL) == 0);
}

static void test_inertial_trail_pins_and_detail(void) {
    KtglCompanionCtx c;
    ktgl_companion_init(&c, KTGL_GAME_AOC, 1, 1);
    uint64_t t = 1000000000ULL;
    for (int i = 0; i < 6; i++) {
        KtglHostSample s = base_sample(t + (uint64_t)i * 60000000ULL);
        s.game_kind = KTGL_GAME_AOC;
        s.ly = 32767;
        s.save_commits = (i >= 4) ? 2ULL : 0ULL;
        ktgl_companion_present(&c, &s);
    }
    assert(c.pos_z < -1.5f && fabsf(c.pos_x) < 1e-3f);
    assert(c.trail.count >= 2U);
    assert(ktgl_companion_pin(&c, 0, -1, "Keep") == 0);
    assert(ktgl_companion_pin(&c, 0, -1, "Boss") == 1);
    assert(ktgl_companion_pin(&c, 1, 0, NULL) == 1);

    int32_t ints[KTGL_DETAIL_INTS] = {0};
    float flts[KTGL_DETAIL_FLOATS] = {0};
    char txt[512] = "";
    assert(ktgl_companion_pack_detail(&c, ints, KTGL_DETAIL_INTS, flts, KTGL_DETAIL_FLOATS, txt, sizeof(txt)));
    assert(ints[0] == COMPANION_STATE_READY && ints[2] == 1 && ints[6] == KTGL_GAME_AOC);
    assert(ints[13] == 2 && ints[14] >= 0 && ints[16] == 2 && ints[18] == 1 && ints[19] == 1);
    assert(strstr(txt, "\nBoss\n") != NULL);
    assert(ktgl_companion_pin(&c, 2, -1, NULL) == 1 && c.trail.pins[1].active == 0U);
}

static void test_chords_party_and_abort(void) {
    KtglCompanionCtx c;
    ktgl_companion_init(&c, KTGL_GAME_AOI, 0, 0);
    uint64_t t = 1000000000ULL;
    for (int i = 0; i < 4; i++) {
        KtglHostSample s = base_sample(t + (uint64_t)i * 20000000ULL);
        ktgl_companion_present(&c, &s);
    }
    t += 100000000ULL;
    assert(ktgl_companion_fire(&c, 2, t));
    assert(c.active_party == 2 && c.seq_buttons == KTGL_BTN_DOWN);
    KtglHostSample step = base_sample(t + 120000000ULL);
    ktgl_companion_present(&c, &step);
    assert(c.seq_buttons == 0ULL);

    t += 200000000ULL;
    assert(ktgl_companion_fire(&c, KTGL_ACT_R_BASE + 1, t));
    assert(c.seq_buttons == KTGL_BTN_R);
    step = base_sample(t + 90000000ULL);
    ktgl_companion_present(&c, &step);
    assert(c.seq_buttons == (KTGL_BTN_R | KTGL_BTN_X));
    step = base_sample(t + 180000000ULL);
    ktgl_companion_present(&c, &step);
    assert(c.seq_buttons == 0ULL);

    t += 300000000ULL;
    assert(ktgl_companion_fire(&c, KTGL_ACT_COMBO_1 + 3, t));
    assert(c.seq_buttons == KTGL_BTN_Y);
    step = base_sample(t + 20000000ULL);
    step.phys_buttons = KTGL_BTN_X;
    ktgl_companion_present(&c, &step);
    assert(!c.seq.active && c.seq_buttons == 0ULL);
}

int main(void) {
    test_lifecycle_and_fail_closed();
    test_inertial_trail_pins_and_detail();
    test_chords_party_and_abort();
    printf("t_ktgl_companion: PASS\n");
    return 0;
}
