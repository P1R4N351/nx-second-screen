// SPDX-License-Identifier: MIT
/* Narrow second-screen action waist (W-A): zero-allocation bounded ring for
   frame-end guarded actions submitted from the second screen UI. */
#ifndef NX_COMPANION_VERBS_H
#define NX_COMPANION_VERBS_H

#include "companion_state.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Standard host action verbs across adapters. */
enum CompanionVerb {
    COMPANION_VERB_NOP = 0,
    COMPANION_VERB_EQUIP_SLOT = 1,
    COMPANION_VERB_SET_PAGE = 2,
    COMPANION_VERB_PLAY_SONG_NOTE = 3,
    COMPANION_VERB_CUSTOM_BASE = 100
};

/* Execution outcome tracked per sequence number. */
enum CompanionActionStatus {
    COMPANION_ACT_PENDING = 0,
    COMPANION_ACT_APPLIED = 1,
    COMPANION_ACT_REFUSED_GATE = 2,
    COMPANION_ACT_REFUSED_INVALID = 3
};

enum {
    COMPANION_VERB_CAP = 8
};

typedef struct CompanionVerbReq {
    uint32_t seq;
    int32_t verb;
    int32_t slot;
    uint64_t item_token;
} CompanionVerbReq;

typedef struct CompanionVerbRes {
    uint32_t seq;
    int32_t status;
    int32_t gate_reason;
} CompanionVerbRes;

typedef struct CompanionVerbQueue {
    CompanionVerbReq ring[COMPANION_VERB_CAP];
    CompanionVerbRes res[COMPANION_VERB_CAP];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t next_seq;
} CompanionVerbQueue;

/* Initialize queue and sequence counter (1..UINT32_MAX, never 0). */
static inline void companion_verb_init(CompanionVerbQueue *q) {
    assert(q != NULL);
    memset(q, 0, sizeof(*q));
    q->next_seq = 1U;
}

static inline void companion_verb_store_res(
    CompanionVerbQueue *q, uint32_t seq, int32_t status, int32_t gate_reason) {
    assert(q != NULL);
    assert(seq != 0U);
    uint32_t idx = seq % (uint32_t)COMPANION_VERB_CAP;
    q->res[idx].seq = seq;
    q->res[idx].status = status;
    q->res[idx].gate_reason = gate_reason;
}

/* Enqueue an action request. Returns 1 on success, 0 if queue is full or invalid. */
static inline int companion_verb_submit(
    CompanionVerbQueue *q, int32_t verb, int32_t slot, uint64_t item_token, uint32_t *out_seq) {
    assert(q != NULL);
    assert(out_seq != NULL);
    *out_seq = 0U;
    if (q->count >= (uint32_t)COMPANION_VERB_CAP) return 0;

    uint32_t seq = q->next_seq;
    if (seq == 0U) seq = 1U;
    q->next_seq = (seq == UINT32_MAX) ? 1U : (seq + 1U);

    CompanionVerbReq *dst = &q->ring[q->tail];
    dst->seq = seq;
    dst->verb = verb;
    dst->slot = slot;
    dst->item_token = item_token;
    q->tail = (q->tail + 1U) % (uint32_t)COMPANION_VERB_CAP;
    q->count++;
    companion_verb_store_res(q, seq, COMPANION_ACT_PENDING, COMPANION_GATE_NONE);
    *out_seq = seq;
    return 1;
}

/* Pop one request at frame-end. Automatically refuses if active_gate_reason != 0. */
static inline int companion_verb_drain_one(
    CompanionVerbQueue *q, int32_t active_gate_reason, CompanionVerbReq *out_req) {
    assert(q != NULL);
    assert(out_req != NULL);
    memset(out_req, 0, sizeof(*out_req));
    if (q->count == 0U) return 0;

    CompanionVerbReq req = q->ring[q->head];
    q->head = (q->head + 1U) % (uint32_t)COMPANION_VERB_CAP;
    q->count--;

    if (active_gate_reason != COMPANION_GATE_NONE) {
        companion_verb_store_res(q, req.seq, COMPANION_ACT_REFUSED_GATE, active_gate_reason);
        return 0;
    }
    *out_req = req;
    return 1;
}

/* Record adapter completion status for a drained request sequence number. */
static inline void companion_verb_complete(
    CompanionVerbQueue *q, uint32_t seq, int32_t status, int32_t gate_reason) {
    assert(q != NULL);
    if (seq == 0U) return;
    companion_verb_store_res(q, seq, status, gate_reason);
}

/* Query status for seq. Returns 1 if seq still in bounded history, 0 if evicted/unknown. */
static inline int companion_verb_result(
    const CompanionVerbQueue *q, uint32_t seq, CompanionVerbRes *out_res) {
    assert(q != NULL);
    assert(out_res != NULL);
    memset(out_res, 0, sizeof(*out_res));
    if (seq == 0U) return 0;
    uint32_t idx = seq % (uint32_t)COMPANION_VERB_CAP;
    if (q->res[idx].seq != seq) return 0;
    *out_res = q->res[idx];
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif
