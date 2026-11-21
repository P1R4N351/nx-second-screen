// SPDX-License-Identifier: MIT
/* Unit tests for CompanionVerbQueue bounded ring and fail-closed write gating. */
#include "nx_companion/companion_verbs.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
    abort(); } } while (0)

static void test_ring_capacity_and_wrap(void) {
    CompanionVerbQueue q;
    companion_verb_init(&q);

    uint32_t seqs[COMPANION_VERB_CAP];
    for (int i = 0; i < COMPANION_VERB_CAP; i++) {
        CHECK(companion_verb_submit(&q, COMPANION_VERB_EQUIP_SLOT, i, (uint64_t)(100 + i), &seqs[i]) == 1);
        CHECK(seqs[i] == (uint32_t)(i + 1));
    }

    uint32_t extra = 99U;
    CHECK(companion_verb_submit(&q, COMPANION_VERB_SET_PAGE, 2, 0ULL, &extra) == 0);
    CHECK(extra == 0U);

    CompanionVerbReq req;
    CHECK(companion_verb_drain_one(&q, COMPANION_GATE_NONE, &req) == 1);
    CHECK(req.seq == 1U && req.verb == COMPANION_VERB_EQUIP_SLOT && req.slot == 0);
    companion_verb_complete(&q, req.seq, COMPANION_ACT_APPLIED, COMPANION_GATE_NONE);

    CompanionVerbRes res;
    CHECK(companion_verb_result(&q, 1U, &res) == 1 && res.status == COMPANION_ACT_APPLIED);
    CHECK(companion_verb_submit(&q, COMPANION_VERB_SET_PAGE, 4, 0ULL, &extra) == 1 && extra == 9U);
    CHECK(companion_verb_result(&q, 1U, &res) == 0); /* Slot 1%8 overwritten cleanly by seq 9. */
}

static void test_all_gates_refuse_drain(void) {
    static const int32_t gates[5] = {
        COMPANION_GATE_MENU, COMPANION_GATE_EVENT, COMPANION_GATE_LOADING,
        COMPANION_GATE_SAVING, COMPANION_GATE_STALE
    };
    CompanionVerbQueue q;
    companion_verb_init(&q);

    for (size_t i = 0U; i < 5U; i++) {
        uint32_t seq = 0U;
        CHECK(companion_verb_submit(&q, COMPANION_VERB_EQUIP_SLOT, (int32_t)i, 0xABCDULL, &seq) == 1);

        CompanionVerbReq req;
        CHECK(companion_verb_drain_one(&q, gates[i], &req) == 0);

        CompanionVerbRes res;
        CHECK(companion_verb_result(&q, seq, &res) == 1);
        CHECK(res.seq == seq);
        CHECK(res.status == COMPANION_ACT_REFUSED_GATE);
        CHECK(res.gate_reason == gates[i]);
    }
}

int main(void) {
    test_ring_capacity_and_wrap();
    test_all_gates_refuse_drain();
    puts("t_companion_verbs: ok");
    return 0;
}
