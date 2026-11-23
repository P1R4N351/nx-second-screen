// SPDX-License-Identifier: MIT
// Clean-room KTGL Warriors companion engine (Age of Calamity + Age of Imprisonment).
// Tracks engine/save/movie state, 3D inertial trail, tactical pins, and write-safe chords.
#ifndef NX_KTGL_COMPANION_H
#define NX_KTGL_COMPANION_H

#include "nx_companion/companion_seq.h"
#include "nx_companion/companion_state.h"
#include "nx_companion/companion_trail.h"
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { KTGL_GAME_UNKNOWN = 0, KTGL_GAME_AOC = 1, KTGL_GAME_AOI = 2 };

enum {
    KTGL_BTN_A = 1ULL << 0,     KTGL_BTN_B = 1ULL << 1,
    KTGL_BTN_X = 1ULL << 2,     KTGL_BTN_Y = 1ULL << 3,
    KTGL_BTN_LS = 1ULL << 4,    KTGL_BTN_RS = 1ULL << 5,
    KTGL_BTN_L = 1ULL << 6,     KTGL_BTN_R = 1ULL << 7,
    KTGL_BTN_ZL = 1ULL << 8,    KTGL_BTN_ZR = 1ULL << 9,
    KTGL_BTN_PLUS = 1ULL << 10, KTGL_BTN_MINUS = 1ULL << 11,
    KTGL_BTN_LEFT = 1ULL << 12, KTGL_BTN_UP = 1ULL << 13,
    KTGL_BTN_RIGHT = 1ULL << 14, KTGL_BTN_DOWN = 1ULL << 15,
};

enum {
    KTGL_ACT_PARTY_0 = 0, KTGL_ACT_PARTY_3 = 3,
    KTGL_ACT_R_BASE = 10, KTGL_ACT_R_LAST = 13,
    KTGL_ACT_L_BASE = 20, KTGL_ACT_L_LAST = 23,
    KTGL_ACT_LOCKON = 30, KTGL_ACT_SPECIAL = 31, KTGL_ACT_DODGE = 32,
    KTGL_ACT_COMBO_1 = 40, KTGL_ACT_COMBO_6 = 45,
};

enum {
    KTGL_DETAIL_INTS = 32,
    KTGL_DETAIL_FLOATS = 416,
    KTGL_FLT_POS_X = 0, KTGL_FLT_POS_Y = 1, KTGL_FLT_POS_Z = 2,
    KTGL_FLT_YAW = 3,   KTGL_FLT_FPS = 4,   KTGL_FLT_FRAME_MS = 5,
    KTGL_FLT_TRAIL_BASE = 6,
    KTGL_FLT_PIN_BASE = 390,
};

typedef struct KtglHostSample {
    int enabled;
    int game_kind;
    int main_loaded;
    int build_ok;
    int host_paused;
    int movie_playing;
    int save_busy;
    uint64_t save_commits;
    uint64_t now_ns;
    float fps;
    int32_t lx;
    int32_t ly;
    int32_t rx;
    int32_t ry;
    uint64_t phys_buttons;
} KtglHostSample;

typedef struct KtglCompanionCtx {
    CompanionState state;
    uint64_t state_ns;
    NxCompanionTrail trail;
    NxCompanionSeq seq;
    int32_t user_page;
    int32_t game_kind;
    int32_t active_party;
    int32_t movie_playing;
    int32_t host_paused;
    int32_t save_busy;
    int32_t input_active;
    int32_t initial_heading_up;
    int32_t initial_page;
    uint64_t presents;
    uint64_t last_tick_ns;
    uint64_t last_pose_ns;
    uint64_t prev_phys_buttons;
    uint64_t save_commits;
    uint64_t last_save_ns;
    float fps;
    float pos_x;
    float pos_y;
    float pos_z;
    float yaw_deg;
    uint64_t seq_buttons;
} KtglCompanionCtx;

static inline int ktgl_build_matches(const char *build, const char *want32) {
    if (!build || !want32) return 0;
    for (size_t i = 0U; i < 32U; i++) {
        if (build[i] == '\0' || want32[i] == '\0') return 0;
        if (tolower((unsigned char)build[i]) != tolower((unsigned char)want32[i])) return 0;
    }
    return 1;
}

static inline void ktgl_companion_init(
    KtglCompanionCtx *c, int game_kind, int heading_up, int initial_page) {
    assert(c != NULL);
    memset(c, 0, sizeof(*c));
    companion_state_clear(&c->state);
    nx_trail_init(&c->trail, 1.5f, 3.0f);
    nx_seq_init(&c->seq);
    c->user_page = -1;
    c->game_kind = game_kind;
    c->initial_heading_up = heading_up ? 1 : 0;
    c->initial_page = (initial_page >= 0 && initial_page <= 3) ? initial_page : 0;
}

static inline int ktgl_companion_write_safe(const KtglCompanionCtx *c) {
    if (!c) return 0;
    return c->state.status == COMPANION_STATE_READY &&
           c->state.in_game && !c->host_paused && !c->movie_playing;
}

static inline float ktgl_norm_stick(int32_t raw) {
    float v = (float)raw / 32767.0f;
    if (v > 1.0f) v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    float mag = fabsf(v);
    if (mag <= 0.15f) return 0.0f;
    float scaled = (mag - 0.15f) / 0.85f;
    return (v < 0.0f) ? -scaled : scaled;
}

static inline float ktgl_clamp_coord(float v) {
    if (!isfinite(v)) return 0.0f;
    if (v > 9999.0f) return 9999.0f;
    if (v < -9999.0f) return -9999.0f;
    return v;
}

static inline void ktgl_companion_integrate_pose(
    KtglCompanionCtx *c, const KtglHostSample *s) {
    assert(c != NULL && s != NULL);
    if (!ktgl_companion_write_safe(c)) {
        c->last_pose_ns = s->now_ns;
        return;
    }
    if (c->last_pose_ns == 0ULL || s->now_ns <= c->last_pose_ns) {
        c->last_pose_ns = s->now_ns;
        (void)nx_trail_push(&c->trail, "battlefield", c->pos_x, c->pos_y, c->pos_z);
        return;
    }
    uint64_t dt_ns = s->now_ns - c->last_pose_ns;
    if (dt_ns < 50000000ULL) return;
    float dt = (float)dt_ns * 1e-9f;
    if (dt > 0.25f) dt = 0.25f;
    c->last_pose_ns = s->now_ns;

    float rx = ktgl_norm_stick(s->rx);
    float lx = ktgl_norm_stick(s->lx);
    float ly = ktgl_norm_stick(s->ly);
    c->yaw_deg = fmodf(c->yaw_deg + rx * 150.0f * dt + 360.0f, 360.0f);

    float rad = c->yaw_deg * (NX_TRAIL_PI_F / 180.0f);
    float si = sinf(rad), co = cosf(rad);
    float vx = (si * ly + co * lx) * 18.0f;
    float vz = (-co * ly + si * lx) * 18.0f;
    c->pos_x = ktgl_clamp_coord(c->pos_x + vx * dt);
    c->pos_z = ktgl_clamp_coord(c->pos_z + vz * dt);
    (void)nx_trail_push(&c->trail, "battlefield", c->pos_x, c->pos_y, c->pos_z);
}

static inline void ktgl_companion_sync_inputs(
    KtglCompanionCtx *c, const KtglHostSample *s) {
    assert(c != NULL && s != NULL);
    uint64_t edge = s->phys_buttons & ~c->prev_phys_buttons;
    c->prev_phys_buttons = s->phys_buttons;
    uint64_t dpad_mask = KTGL_BTN_UP | KTGL_BTN_RIGHT | KTGL_BTN_DOWN | KTGL_BTN_LEFT;
    c->input_active = (s->phys_buttons != 0ULL || ktgl_norm_stick(s->lx) != 0.0f ||
                       ktgl_norm_stick(s->ly) != 0.0f) ? 1 : 0;
    if (!ktgl_companion_write_safe(c)) return;
    if ((edge & dpad_mask) != 0ULL) {
        uint64_t d = edge & dpad_mask;
        if (d == KTGL_BTN_UP) c->active_party = 0;
        else if (d == KTGL_BTN_RIGHT) c->active_party = 1;
        else if (d == KTGL_BTN_DOWN) c->active_party = 2;
        else if (d == KTGL_BTN_LEFT) c->active_party = 3;
    }
    uint64_t combat_mask = KTGL_BTN_A | KTGL_BTN_B | KTGL_BTN_X | KTGL_BTN_Y |
                           KTGL_BTN_L | KTGL_BTN_R | KTGL_BTN_ZL | KTGL_BTN_ZR;
    if (c->seq.active && c->seq.tag >= KTGL_ACT_COMBO_1 && (edge & combat_mask) != 0ULL) {
        nx_seq_abort(&c->seq);
    }
}

static inline void ktgl_companion_present(
    KtglCompanionCtx *c, const KtglHostSample *s) {
    assert(c != NULL && s != NULL);
    if (!s->enabled) {
        companion_state_clear(&c->state);
        nx_seq_abort(&c->seq);
        c->seq_buttons = 0ULL;
        return;
    }
    c->presents++;
    c->game_kind = s->game_kind;
    c->movie_playing = s->movie_playing ? 1 : 0;
    c->host_paused = s->host_paused ? 1 : 0;
    c->save_busy = s->save_busy ? 1 : 0;
    c->fps = (isfinite(s->fps) && s->fps >= 0.0f && s->fps <= 240.0f) ? s->fps : 0.0f;
    if (s->save_commits > c->save_commits) {
        c->save_commits = s->save_commits;
        c->last_save_ns = s->now_ns;
    }
    c->state_ns = s->now_ns;
    c->last_tick_ns = s->now_ns;
    companion_state_clear(&c->state);
    c->state.page = (c->user_page >= 0 && c->user_page <= 3) ? c->user_page : c->initial_page;
    if (!s->main_loaded) {
        c->state.status = COMPANION_STATE_STARTING;
    } else if (!s->build_ok) {
        c->state.status = COMPANION_STATE_UNSUPPORTED;
    } else if (c->presents < 4ULL) {
        c->state.status = COMPANION_STATE_LOADING;
    } else {
        c->state.status = COMPANION_STATE_READY;
        c->state.in_game = 1;
        c->state.menu_open = c->host_paused;
        c->state.tablet_owned = 1;
    }
    ktgl_companion_sync_inputs(c, s);
    ktgl_companion_integrate_pose(c, s);
    c->seq_buttons = nx_seq_tick(&c->seq, ktgl_companion_write_safe(c), s->now_ns);
}

static inline uint64_t ktgl_slot_face_btn(int slot) {
    static const uint64_t faces[4] = { KTGL_BTN_Y, KTGL_BTN_X, KTGL_BTN_B, KTGL_BTN_A };
    return (slot >= 0 && slot < 4) ? faces[slot] : 0ULL;
}

static inline int ktgl_build_action_chords(
    int id, uint64_t chords[NX_SEQ_MAX_STEPS], uint32_t *count,
    uint64_t *hold_ns, uint64_t *gap_ns, int *party_out) {
    assert(chords != NULL && count != NULL && hold_ns != NULL && gap_ns != NULL);
    static const uint64_t party[4] = { KTGL_BTN_UP, KTGL_BTN_RIGHT, KTGL_BTN_DOWN, KTGL_BTN_LEFT };
    *party_out = -1;
    if (id >= KTGL_ACT_PARTY_0 && id <= KTGL_ACT_PARTY_3) {
        chords[0] = party[id]; *count = 1U; *hold_ns = 110000000ULL; *gap_ns = 0ULL;
        *party_out = id;
        return 1;
    }
    if (id >= KTGL_ACT_R_BASE && id <= KTGL_ACT_R_LAST) {
        uint64_t f = ktgl_slot_face_btn(id - KTGL_ACT_R_BASE);
        chords[0] = KTGL_BTN_R; chords[1] = KTGL_BTN_R | f;
        *count = 2U; *hold_ns = 80000000ULL; *gap_ns = 0ULL;
        return 1;
    }
    if (id >= KTGL_ACT_L_BASE && id <= KTGL_ACT_L_LAST) {
        uint64_t f = ktgl_slot_face_btn(id - KTGL_ACT_L_BASE);
        chords[0] = KTGL_BTN_L; chords[1] = KTGL_BTN_L | f;
        *count = 2U; *hold_ns = 80000000ULL; *gap_ns = 0ULL;
        return 1;
    }
    if (id >= KTGL_ACT_LOCKON && id <= KTGL_ACT_DODGE) {
        static const uint64_t util[3] = { KTGL_BTN_RS, KTGL_BTN_A, KTGL_BTN_B };
        chords[0] = util[id - KTGL_ACT_LOCKON]; *count = 1U; *hold_ns = 110000000ULL; *gap_ns = 0ULL;
        return 1;
    }
    if (id >= KTGL_ACT_COMBO_1 && id <= KTGL_ACT_COMBO_6) {
        uint32_t y_cnt = (uint32_t)(id - KTGL_ACT_COMBO_1);
        for (uint32_t i = 0U; i < y_cnt; i++) chords[i] = KTGL_BTN_Y;
        chords[y_cnt] = KTGL_BTN_X;
        *count = y_cnt + 1U; *hold_ns = 110000000ULL; *gap_ns = 90000000ULL;
        return 1;
    }
    return 0;
}

static inline int ktgl_companion_fire(
    KtglCompanionCtx *c, int action_id, uint64_t now_ns) {
    if (!c || !ktgl_companion_write_safe(c)) return 0;
    uint64_t chords[NX_SEQ_MAX_STEPS] = {0};
    uint32_t count = 0U;
    uint64_t hold_ns = 0ULL, gap_ns = 0ULL;
    int party_out = -1;
    if (!ktgl_build_action_chords(action_id, chords, &count, &hold_ns, &gap_ns, &party_out)) return 0;
    if (!nx_seq_start(&c->seq, chords, count, (int32_t)action_id, hold_ns, gap_ns, now_ns)) return 0;
    if (party_out >= 0) c->active_party = (int32_t)party_out;
    c->seq_buttons = nx_seq_tick(&c->seq, 1, now_ns);
    return 1;
}

static inline void ktgl_companion_abort(KtglCompanionCtx *c) {
    if (!c) return;
    nx_seq_abort(&c->seq);
    c->seq_buttons = 0ULL;
}

static inline int ktgl_companion_pin(
    KtglCompanionCtx *c, int op, int slot, const char *label) {
    if (!c) return -1;
    if (op == 0) {
        if (c->state.status != COMPANION_STATE_READY) return -1;
        return nx_trail_pin_add(&c->trail, c->pos_x, c->pos_y, c->pos_z, label);
    }
    if (op == 1) return (slot >= 0) ? nx_trail_pin_remove(&c->trail, (uint32_t)slot) : 0;
    if (op == 2) { nx_trail_pins_clear(&c->trail); return 1; }
    return -1;
}

static inline size_t ktgl_append_line(char *dst, size_t cap, size_t off, const char *s) {
    if (!dst || off + 2U > cap) return off;
    int n = snprintf(dst + off, cap - off, "%s\n", (s && s[0]) ? s : "");
    if (n <= 0) return off;
    size_t add = (size_t)n;
    return (off + add < cap) ? (off + add) : (cap - 1U);
}

static inline void ktgl_pack_ints(
    const KtglCompanionCtx *c, int32_t *ints, int max_ints, uint32_t pin_mask) {
    ints[0] = c->state.status;
    ints[1] = c->state.in_game;
    ints[2] = ktgl_companion_write_safe(c);
    ints[3] = c->movie_playing;
    ints[4] = c->host_paused;
    ints[5] = c->save_busy;
    ints[6] = c->game_kind;
    ints[7] = c->active_party;
    ints[8] = c->seq.active;
    ints[9] = c->seq.tag;
    ints[10] = (int32_t)c->seq.idx;
    ints[11] = (int32_t)c->seq.count;
    ints[12] = (c->presents > (uint64_t)INT32_MAX) ? INT32_MAX : (int32_t)c->presents;
    ints[13] = (c->save_commits > (uint64_t)INT32_MAX) ? INT32_MAX : (int32_t)c->save_commits;
    int32_t age_s = -1;
    if (c->last_save_ns > 0ULL && c->last_tick_ns >= c->last_save_ns) {
        uint64_t ds = (c->last_tick_ns - c->last_save_ns) / 1000000000ULL;
        age_s = (ds > (uint64_t)INT32_MAX) ? INT32_MAX : (int32_t)ds;
    }
    ints[14] = age_s;
    ints[15] = (int32_t)c->trail.count;
    ints[16] = (int32_t)pin_mask;
    ints[17] = c->user_page;
    ints[18] = c->initial_heading_up;
    ints[19] = c->initial_page;
    ints[20] = (c->state.status == COMPANION_STATE_READY) ? 1 : 0;
    ints[21] = c->input_active;
    (void)max_ints;
}

static inline int ktgl_companion_pack_detail(
    const KtglCompanionCtx *c, int32_t *ints, int max_ints,
    float *flts, int max_flts, char *txt, int max_txt) {
    if (!c || !ints || !flts || max_ints < KTGL_DETAIL_INTS || max_flts < KTGL_DETAIL_FLOATS) {
        return 0;
    }
    memset(ints, 0, sizeof(int32_t) * KTGL_DETAIL_INTS);
    memset(flts, 0, sizeof(float) * KTGL_DETAIL_FLOATS);
    flts[KTGL_FLT_POS_X] = c->pos_x;
    flts[KTGL_FLT_POS_Y] = c->pos_y;
    flts[KTGL_FLT_POS_Z] = c->pos_z;
    flts[KTGL_FLT_YAW] = c->yaw_deg;
    flts[KTGL_FLT_FPS] = c->fps;
    flts[KTGL_FLT_FRAME_MS] = (c->fps > 0.5f) ? (1000.0f / c->fps) : 0.0f;

    uint32_t tcnt = (c->trail.count <= NX_TRAIL_CAP) ? c->trail.count : NX_TRAIL_CAP;
    for (uint32_t i = 0U; i < tcnt; i++) {
        NxTrailPoint pt = {0};
        if (nx_trail_get_oldest(&c->trail, i, &pt)) {
            uint32_t base = KTGL_FLT_TRAIL_BASE + i * 3U;
            flts[base] = pt.x; flts[base + 1U] = pt.y; flts[base + 2U] = pt.z;
        }
    }
    uint32_t pin_mask = 0U;
    size_t off = 0U;
    size_t cap = (txt && max_txt > 0) ? (size_t)max_txt : 0U;
    if (cap > 0U) txt[0] = '\0';
    for (uint32_t i = 0U; i < NX_WAYPOINT_CAP; i++) {
        if (c->trail.pins[i].active) {
            pin_mask |= (1U << i);
            uint32_t base = KTGL_FLT_PIN_BASE + i * 3U;
            flts[base] = c->trail.pins[i].x;
            flts[base + 1U] = c->trail.pins[i].y;
            flts[base + 2U] = c->trail.pins[i].z;
        }
        if (cap > 0U) off = ktgl_append_line(txt, cap, off, c->trail.pins[i].active ? c->trail.pins[i].label : "");
    }
    ktgl_pack_ints(c, ints, max_ints, pin_mask);
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif
