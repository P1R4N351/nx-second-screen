// SPDX-License-Identifier: MIT
/* Clean-room MIT KTGL adapter scaffold for Hyrule Warriors: Age of Imprisonment
   (04003F40232DE000, v196608) and Hyrule Warriors: Age of Calamity (01002B00111A2000).
   Provides verified build-ID gating, movie dim latch, and autosave gate signals. */
#ifndef NX_KTGL_ADAPTER_H
#define NX_KTGL_ADAPTER_H

#include "nx_companion/companion_state.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KTGL_AOI_MAIN_BUILD_V196608 "659cb288b5c5b2b49853add616c0d5bf"

enum KtglTitleKind {
    KTGL_TITLE_UNKNOWN = 0,
    KTGL_TITLE_AOI = 1,
    KTGL_TITLE_AOC = 2
};

typedef struct KtglHostSignals {
    int in_movie;
    int save_busy;
    int boot_ready;
    uint32_t dim_hold;
    int32_t user_page;
} KtglHostSignals;

/* Evaluate KTGL host signals into CompanionState v2 with optional build gate. */
static inline void ktgl_eval_state(
    int enabled, int title_kind, const char *main_build, const char *expected_build,
    const KtglHostSignals *sig, int64_t age_ms, int64_t max_age_ms, CompanionState *out) {
    assert(out != NULL);
    companion_state_clear(out);
    if (!enabled) return;
    if (!main_build || main_build[0] == '\0') { out->status = COMPANION_STATE_STARTING; return; }

    const char *req_build = expected_build;
    if (!req_build && title_kind == KTGL_TITLE_AOI) req_build = KTGL_AOI_MAIN_BUILD_V196608;
    if (req_build && strncmp(main_build, req_build, 32U) != 0) {
        out->status = COMPANION_STATE_UNSUPPORTED;
        out->gate_reason = COMPANION_GATE_UNSUPPORTED;
        return;
    }
    if (!sig || !sig->boot_ready) {
        out->status = COMPANION_STATE_STARTING;
        return;
    }
    out->status = COMPANION_STATE_READY;
    out->in_game = 1;
    out->dim = (sig->in_movie != 0 || sig->dim_hold > 0U) ? 1 : 0;
    out->page = sig->user_page;
    out->gate_reason = companion_compute_gate(out, sig->save_busy, age_ms, max_age_ms);
}

#ifdef __cplusplus
}
#endif

#endif
