// SPDX-License-Identifier: MIT
/* Unit tests for CompanionState v1/v2 layout, hold latches, and write-gate precedence. */
#include "nx_companion/companion_state.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
    abort(); } } while (0)

static void test_v1_v2_serialization(void) {
    CompanionState s;
    companion_state_clear(&s);
    CHECK(s.status == COMPANION_STATE_NONE);
    CHECK(s.in_game == 0);
    CHECK(s.life == -1 && s.max_life == -1);
    CHECK(s.stamina == -1 && s.max_stamina == -1);
    CHECK(companion_stamina_wheel(&s) == 0);

    s.status = COMPANION_STATE_READY;
    s.in_game = 1;
    s.menu_open = 1;
    s.tablet_owned = 1;
    s.life = 12;
    s.max_life = 24;
    s.page = COMPANION_PAGE_OCARINA;
    s.dim = 1;
    s.stamina = 600;
    s.max_stamina = 1000;
    s.gate_reason = COMPANION_GATE_MENU;

    int64_t v1[COMPANION_STATE_V1_FIELDS] = {0};
    companion_state_words_v1(&s, 42LL, v1);
    CHECK(v1[0] == COMPANION_STATE_READY && v1[1] == 1 && v1[2] == 1 && v1[3] == 1);
    CHECK(v1[4] == 12 && v1[5] == 24 && v1[6] == COMPANION_PAGE_OCARINA && v1[7] == 42LL);

    int64_t v2[COMPANION_STATE_FIELDS] = {0};
    companion_state_words(&s, 42LL, v2);
    for (int i = 0; i < COMPANION_STATE_V1_FIELDS; i++) CHECK(v2[i] == v1[i]);
    CHECK(v2[8] == 1 && v2[9] == 600 && v2[10] == 1000 && v2[11] == 1 && v2[12] == COMPANION_GATE_MENU);

    s.stamina = 1000;
    companion_state_words(&s, 10LL, v2);
    CHECK(v2[11] == 0);
}

static void test_latch_and_gate_precedence(void) {
    uint32_t ctr = 0U;
    CHECK(companion_latch_step(&ctr, 0, 3U) == 0 && ctr == 0U);
    CHECK(companion_latch_step(&ctr, 1, 3U) == 1 && ctr == 3U);
    CHECK(companion_latch_step(&ctr, 0, 3U) == 1 && ctr == 2U);
    CHECK(companion_latch_step(&ctr, 0, 3U) == 1 && ctr == 1U);
    CHECK(companion_latch_step(&ctr, 0, 3U) == 1 && ctr == 0U);
    CHECK(companion_latch_step(&ctr, 0, 3U) == 0 && ctr == 0U);

    CompanionState s;
    companion_state_clear(&s);
    s.status = COMPANION_STATE_UNSUPPORTED;
    CHECK(companion_compute_gate(&s, 1, 0LL, 500LL) == COMPANION_GATE_UNSUPPORTED);

    s.status = COMPANION_STATE_LOADING;
    s.in_game = 1;
    CHECK(companion_compute_gate(&s, 1, 0LL, 500LL) == COMPANION_GATE_LOADING);

    s.status = COMPANION_STATE_READY;
    s.menu_open = 1;
    s.dim = 1;
    CHECK(companion_compute_gate(&s, 1, 0LL, 500LL) == COMPANION_GATE_SAVING);
    CHECK(companion_compute_gate(&s, 0, 501LL, 500LL) == COMPANION_GATE_STALE);
    CHECK(companion_compute_gate(&s, 0, -1LL, 500LL) == COMPANION_GATE_STALE);
    CHECK(companion_compute_gate(&s, 0, 100LL, 500LL) == COMPANION_GATE_MENU);

    s.menu_open = 0;
    CHECK(companion_compute_gate(&s, 0, 100LL, 500LL) == COMPANION_GATE_EVENT);

    s.dim = 0;
    CHECK(companion_compute_gate(&s, 0, 100LL, 500LL) == COMPANION_GATE_NONE);
}

int main(void) {
    test_v1_v2_serialization();
    test_latch_and_gate_precedence();
    puts("t_companion_waist: ok");
    return 0;
}
