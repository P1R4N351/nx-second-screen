// SPDX-License-Identifier: MIT
/* Unit tests for fail-closed button/melody sequencer (companion_seq.h). */
#include "nx_companion/companion_seq.h"
#include "oot_sw2/oot_sw2_catalog.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
    abort(); } } while (0)

enum {
    BTN_A = 1ULL << 0,
    BTN_B = 1ULL << 1,
    BTN_X = 1ULL << 2,
    BTN_Y = 1ULL << 3,
    BTN_L = 1ULL << 6,
    BTN_R = 1ULL << 7
};

static void test_sequence_timing_and_completion(void) {
    NxCompanionSeq seq;
    nx_seq_init(&seq);
    const uint64_t notes[3] = { BTN_Y, BTN_X, BTN_A };
    CHECK(nx_seq_start(&seq, notes, 3U, 0, 100ULL, 50ULL, 1000ULL) == 1);

    CHECK(nx_seq_tick(&seq, 1, 1000ULL) == BTN_Y);
    CHECK(nx_seq_tick(&seq, 1, 1099ULL) == BTN_Y);
    CHECK(nx_seq_tick(&seq, 1, 1100ULL) == 0ULL && seq.active == 1); /* inter-note gap */
    CHECK(nx_seq_tick(&seq, 1, 1149ULL) == 0ULL && seq.active == 1);
    CHECK(nx_seq_tick(&seq, 1, 1150ULL) == BTN_X);
    CHECK(nx_seq_tick(&seq, 1, 1250ULL) == 0ULL && seq.active == 1);
    CHECK(nx_seq_tick(&seq, 1, 1300ULL) == BTN_A);
    CHECK(nx_seq_tick(&seq, 1, 1400ULL) == 0ULL && seq.active == 0); /* completed */
}

static void test_fail_closed_gate_and_validation(void) {
    NxCompanionSeq seq;
    nx_seq_init(&seq);
    const uint64_t bad_zero[2] = { BTN_A, 0ULL };
    CHECK(nx_seq_start(&seq, bad_zero, 2U, 0, 100ULL, 20ULL, 10ULL) == 0);

    uint64_t storm[6];
    const OotSongSpec *s = &OOT_SW2_SONGS[5]; /* Song of Storms */
    for (uint32_t i = 0U; i < s->length; i++) {
        storm[i] = oot_sw2_note_button(s->notes[i], 0, BTN_A, BTN_B, BTN_X, BTN_Y, BTN_L, BTN_R);
        CHECK(storm[i] != 0ULL);
    }
    CHECK(storm[0] == BTN_L && storm[1] == BTN_R && storm[2] == BTN_X);

    CHECK(nx_seq_start(&seq, storm, s->length, 5, 100ULL, 40ULL, 500ULL) == 1);
    CHECK(nx_seq_tick(&seq, 1, 510ULL) == BTN_L);
    /* Cutscene/transition/release gate drops -> immediate fail-closed abort! */
    CHECK(nx_seq_tick(&seq, 0, 520ULL) == 0ULL && seq.active == 0);
    /* Subsequent tick stays dead even if gate recovers. */
    CHECK(nx_seq_tick(&seq, 1, 530ULL) == 0ULL && seq.active == 0);

    /* Normal vs Classic verify for D4 / F4. */
    CHECK(oot_sw2_note_button(OOT_NOTE_D4, 1, BTN_A, BTN_B, BTN_X, BTN_Y, BTN_L, BTN_R) == BTN_R);
    CHECK(oot_sw2_note_button(OOT_NOTE_F4, 1, BTN_A, BTN_B, BTN_X, BTN_Y, BTN_L, BTN_R) == BTN_B);
}

int main(void) {
    test_sequence_timing_and_completion();
    test_fail_closed_gate_and_validation();
    puts("t_companion_seq: ok");
    return 0;
}
