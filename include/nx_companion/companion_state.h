// SPDX-License-Identifier: MIT
/* Narrow second-screen waist (W-S): append-only numeric game state shared
   between per-game native adapters and UI/LED consumers above them. */
#ifndef NX_COMPANION_STATE_H
#define NX_COMPANION_STATE_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Coarse execution states published in CompanionState.status. */
enum CompanionStatus {
    COMPANION_STATE_NONE = 0,
    COMPANION_STATE_STARTING = 1,
    COMPANION_STATE_UNSUPPORTED = 2,
    COMPANION_STATE_TITLE = 3,
    COMPANION_STATE_LOADING = 4,
    COMPANION_STATE_READY = 5
};

/* Well-known second-screen page indices (-1 = unchanged / default). */
enum {
    COMPANION_PAGE_OCARINA = 4
};

/* Canonical write-gate reasons (W-A action refusal vocabulary). */
enum CompanionGateReason {
    COMPANION_GATE_NONE = 0,
    COMPANION_GATE_MENU = 1,
    COMPANION_GATE_EVENT = 2,
    COMPANION_GATE_LOADING = 3,
    COMPANION_GATE_SAVING = 4,
    COMPANION_GATE_STALE = 5,
    COMPANION_GATE_UNSUPPORTED = 6
};

enum {
    COMPANION_STATE_V1_FIELDS = 8,
    COMPANION_STATE_FIELDS = 13
};

typedef struct CompanionState {
    /* v1 fields (indices 0..7 in JNI long[] layout). */
    int32_t status;
    int32_t in_game;
    int32_t menu_open;
    int32_t tablet_owned;
    int32_t life;
    int32_t max_life;
    int32_t page;
    /* v2 appended fields (indices 8..12; index 11 stamina_wheel is derived). */
    int32_t dim;
    int32_t stamina;
    int32_t max_stamina;
    int32_t gate_reason;
} CompanionState;

/* Reset s to deterministic safe defaults. */
static inline void companion_state_clear(CompanionState *s) {
    assert(s != NULL);
    s->status = COMPANION_STATE_NONE;
    s->in_game = 0;
    s->menu_open = 0;
    s->tablet_owned = 1;
    s->life = -1;
    s->max_life = -1;
    s->page = -1;
    s->dim = 0;
    s->stamina = -1;
    s->max_stamina = -1;
    s->gate_reason = COMPANION_GATE_NONE;
}

/* Sets *counter = hold_frames while active; decrements on inactive calls,
   staying held (1) for up to hold_frames consecutive inactive frames. */
static inline int companion_latch_step(uint32_t *counter, int active, uint32_t hold_frames) {
    assert(counter != NULL);
    if (active != 0) {
        *counter = hold_frames;
        return 1;
    }
    if (*counter > 0U) {
        (*counter)--;
        return 1;
    }
    return 0;
}

/* Derive boolean wheel visibility once above adapters so games cannot drift. */
static inline int32_t companion_stamina_wheel(const CompanionState *s) {
    assert(s != NULL);
    if (s->stamina >= 0 && s->max_stamina > 0 && s->stamina < s->max_stamina) {
        return 1;
    }
    return 0;
}

/* Compute deterministic write-gate reason from state, save flag, and sample age. */
static inline int32_t companion_compute_gate(
    const CompanionState *s, int saving, int64_t age_ms, int64_t max_age_ms) {
    if (!s || s->status == COMPANION_STATE_UNSUPPORTED) return COMPANION_GATE_UNSUPPORTED;
    if (s->status == COMPANION_STATE_LOADING) return COMPANION_GATE_LOADING;
    if (saving != 0) return COMPANION_GATE_SAVING;
    if (s->status != COMPANION_STATE_READY || s->in_game == 0) return COMPANION_GATE_STALE;
    if (age_ms < 0 || (max_age_ms > 0 && age_ms > max_age_ms)) return COMPANION_GATE_STALE;
    if (s->menu_open != 0) return COMPANION_GATE_MENU;
    if (s->dim != 0) return COMPANION_GATE_EVENT;
    return COMPANION_GATE_NONE;
}

/* Serialize v1 8-word array for backwards compatibility with existing JNI callers. */
static inline void companion_state_words_v1(
    const CompanionState *s, int64_t age_ms, int64_t out[COMPANION_STATE_V1_FIELDS]) {
    assert(s != NULL);
    assert(out != NULL);
    out[0] = s->status;
    out[1] = s->in_game;
    out[2] = s->menu_open;
    out[3] = s->tablet_owned;
    out[4] = s->life;
    out[5] = s->max_life;
    out[6] = s->page;
    out[7] = age_ms;
}

/* Serialize full append-only v2 13-word array (v1 prefix at indices 0..7). */
static inline void companion_state_words(
    const CompanionState *s, int64_t age_ms, int64_t out[COMPANION_STATE_FIELDS]) {
    assert(s != NULL);
    assert(out != NULL);
    companion_state_words_v1(s, age_ms, out);
    out[8] = s->dim;
    out[9] = s->stamina;
    out[10] = s->max_stamina;
    out[11] = companion_stamina_wheel(s);
    out[12] = s->gate_reason;
}

#ifdef __cplusplus
}
#endif

#endif
