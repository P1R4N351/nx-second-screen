// SPDX-License-Identifier: MIT
/* Clean-room MIT adapter for Switch 2 Ocarina of Time (0400486024DEC000, v131072).
   Offsets and MurmurHash3_x86_32 keys trace solely to MIT sources: NaviScout,
   OOT-SW2SE, Links-Toolbox, and BananaNative (E0F3CCFAF50065AE). Zero GPL code. */
#ifndef NX_OOT_SW2_ADAPTER_H
#define NX_OOT_SW2_ADAPTER_H

#include "nx_companion/companion_probe.h"
#include "nx_companion/companion_state.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OOT_SW2_MAIN_BUILD_V131072 "e0f3ccfaf50065ae4ea01eb5aeb13b1a"
#define OOT_SW2_PAUSE_HOLD_FRAMES 3U

/* Verified MIT singleton offsets (v131072 main base). */
#define OOT_SW2_OFF_GDM_ROOT        0x04C7E180ULL
#define OOT_SW2_OFF_GDM_ALT         0x04E842A8ULL
#define OOT_SW2_OFF_PLAYER_PTR      0x04E371D0ULL
#define OOT_SW2_OFF_PLAYER_SLOT     0x480ULL
#define OOT_SW2_OFF_ACTOR_NAME      0x20ULL
#define OOT_SW2_OFF_ACTOR_XYZ       0x31CULL
#define OOT_SW2_OFF_VIEWS_PTR       0x04C7E888ULL
#define OOT_SW2_OFF_VIEW_CAM        0x248ULL
#define OOT_SW2_OFF_CAM_VTABLE      0x04C5DE98ULL
#define OOT_SW2_OFF_CAM_LOOK        0x08ULL
#define OOT_SW2_OFF_CAM_POS6D       0x60ULL
#define OOT_SW2_OFF_PAUSE_CANVAS_VT 0x04BE4FB0ULL

/* Verified MIT MurmurHash3_x86_32 (seed 0) keys in gmd::GameDataMgr. */
#define OOT_KEY_CURRENT_BANC        0x1D6189DAU
#define OOT_KEY_RADAR_LOCATION      0xF74E0E8EU
#define OOT_KEY_IS_CHILDHOOD        0xCEA220D3U
#define OOT_KEY_EVENT_GUARD_0       0xF5DEEF0AU
#define OOT_KEY_EVENT_GUARD_1       0x9A9506E7U
#define OOT_KEY_EVENT_GUARD_2       0xF8633C7FU
#define OOT_KEY_EVENT_GUARD_3       0xF96989B6U
#define OOT_KEY_EVENT_GUARD_4       0xE19DFDD6U
#define OOT_KEY_LIFE                0x31AB5580U
#define OOT_KEY_MAX_LIFE            0xFBE01DA1U
#define OOT_KEY_MAGIC               0x76B18200U
#define OOT_KEY_MAX_MAGIC           0x12D63A97U
#define OOT_KEY_RUPEES              0xA77921D7U
#define OOT_KEY_GOLD_TOKENS         0x04735978U
#define OOT_KEY_EQUIP_0             0xA95A3521U
#define OOT_KEY_EQUIP_1             0xEE463235U
#define OOT_KEY_EQUIP_2             0xA1F7EE2BU
#define OOT_KEY_EQUIP_3             0x2342B619U

static const uint32_t OOT_SW2_TRANSITION_KEYS[5] = {
    OOT_KEY_EVENT_GUARD_0,
    OOT_KEY_EVENT_GUARD_1,
    OOT_KEY_EVENT_GUARD_2,
    OOT_KEY_EVENT_GUARD_3,
    OOT_KEY_EVENT_GUARD_4
};

static const uint32_t OOT_SW2_STONE_KEYS[3] = {
    0xCBD1E58CU, 0x7D69AD47U, 0xB8E3BF45U
};

static const uint32_t OOT_SW2_MEDALLION_KEYS[6] = {
    0x235C7D9AU, 0x692D57CBU, 0x5CE254A6U, 0x0151A68FU, 0x204D2063U, 0x0C7A5FD0U
};

static const uint32_t OOT_SW2_SONG_KEYS[12] = {
    0x01C20002U, 0xB492AC33U, 0xD3F8BFCCU, 0x165DD1ADU,
    0x03B3450EU, 0x2B39DD4FU, 0xDC0A0B54U, 0x49041AB9U,
    0x3FA63986U, 0xC2FB95F4U, 0x802A36A8U, 0xD6320BA4U
};

typedef struct OotSw2Snapshot {
    int valid;
    int in_transition;
    int playing_ocarina;
    int is_child;
    int32_t life;
    int32_t max_life;
    int32_t magic;
    int32_t max_magic;
    int32_t rupees;
    int32_t gold_tokens;
    uint32_t stones_mask;
    uint32_t medallions_mask;
    uint32_t songs_mask;
    int have_player_xyz;
    float player_xyz[3];
    char scene[64];
} OotSw2Snapshot;

static inline uint32_t oot_sw2_read_bitmask(
    const NxGuestMem *m, uint64_t mgr, const uint32_t *keys, size_t n, int use_s32) {
    uint32_t mask = 0U;
    for (size_t i = 0U; i < n && i < 32U; i++) {
        int val = 0;
        int32_t iv = 0;
        if (use_s32) {
            if (nx_gdm_read_s32(m, mgr, &NX_GDM_LAYOUT_OWL, keys[i], &iv) && iv > 0) val = 1;
        } else {
            (void)nx_gdm_read_bool(m, mgr, &NX_GDM_LAYOUT_OWL, keys[i], &val);
        }
        if (val != 0) mask |= (1U << i);
    }
    return mask;
}

static inline void oot_sw2_sample_player(
    const NxGuestMem *m, uint64_t main_base, OotSw2Snapshot *out) {
    assert(out != NULL);
    const uint64_t pchain[2] = { OOT_SW2_OFF_PLAYER_PTR, OOT_SW2_OFF_PLAYER_SLOT };
    uint64_t actor = 0ULL;
    char aname[16] = {0};
    if (!nx_probe_chain(m, main_base, pchain, 2U, &actor)) return;
    if (!nx_guest_read_cstr(m, actor + OOT_SW2_OFF_ACTOR_NAME, aname, sizeof(aname))) return;
    if (strcmp(aname, "Player") != 0) return;

    float xyz[3] = {0.0f, 0.0f, 0.0f};
    if (nx_guest_read(m, actor + OOT_SW2_OFF_ACTOR_XYZ, xyz, sizeof(xyz)) &&
        isfinite(xyz[0]) && isfinite(xyz[1]) && isfinite(xyz[2]) &&
        fabsf(xyz[0]) < 1e5f && fabsf(xyz[1]) < 1e5f && fabsf(xyz[2]) < 1e5f) {
        memcpy(out->player_xyz, xyz, sizeof(xyz));
        out->have_player_xyz = 1;
    }
}

/* Sample MIT gmd::GameDataMgr + Player XYZ into out. Returns 1 when readable. */
static inline int oot_sw2_sample(
    const NxGuestMem *m, uint64_t main_base, OotSw2Snapshot *out) {
    assert(out != NULL);
    memset(out, 0, sizeof(*out));
    out->life = -1;
    out->max_life = -1;
    out->magic = -1;
    out->max_magic = -1;
    if (!m || main_base < 0x10000ULL) return 0;

    uint64_t mgr = 0ULL;
    const uint64_t gdm_chain[1] = { OOT_SW2_OFF_GDM_ROOT };
    if (!nx_probe_chain(m, main_base, gdm_chain, 1U, &mgr)) return 0;
    if (!nx_gdm_read_str64(m, mgr, &NX_GDM_LAYOUT_OWL, OOT_KEY_CURRENT_BANC,
                           out->scene, sizeof(out->scene))) return 0;

    (void)nx_gdm_read_s32(m, mgr, &NX_GDM_LAYOUT_OWL, OOT_KEY_LIFE, &out->life);
    (void)nx_gdm_read_s32(m, mgr, &NX_GDM_LAYOUT_OWL, OOT_KEY_MAX_LIFE, &out->max_life);
    (void)nx_gdm_read_s32(m, mgr, &NX_GDM_LAYOUT_OWL, OOT_KEY_MAGIC, &out->magic);
    (void)nx_gdm_read_s32(m, mgr, &NX_GDM_LAYOUT_OWL, OOT_KEY_MAX_MAGIC, &out->max_magic);
    (void)nx_gdm_read_s32(m, mgr, &NX_GDM_LAYOUT_OWL, OOT_KEY_RUPEES, &out->rupees);
    (void)nx_gdm_read_s32(m, mgr, &NX_GDM_LAYOUT_OWL, OOT_KEY_GOLD_TOKENS, &out->gold_tokens);
    (void)nx_gdm_read_bool(m, mgr, &NX_GDM_LAYOUT_OWL, OOT_KEY_IS_CHILDHOOD, &out->is_child);

    for (size_t i = 0U; i < 5U; i++) {
        int flag = 0;
        if (nx_gdm_read_bool(m, mgr, &NX_GDM_LAYOUT_OWL, OOT_SW2_TRANSITION_KEYS[i], &flag) && flag) {
            out->in_transition = 1;
        }
    }
    out->stones_mask = oot_sw2_read_bitmask(m, mgr, OOT_SW2_STONE_KEYS, 3U, 1);
    out->medallions_mask = oot_sw2_read_bitmask(m, mgr, OOT_SW2_MEDALLION_KEYS, 6U, 1);
    out->songs_mask = oot_sw2_read_bitmask(m, mgr, OOT_SW2_SONG_KEYS, 12U, 0);
    oot_sw2_sample_player(m, main_base, out);
    out->valid = 1;
    return 1;
}

/* Convert OoT snapshot and host signals into CompanionState v2 words. */
static inline void oot_sw2_eval_snapshot(
    int enabled, const char *main_build, const OotSw2Snapshot *snap,
    uint32_t pause_hold, int32_t user_page, int saving,
    int64_t age_ms, int64_t max_age_ms, CompanionState *out) {
    assert(out != NULL);
    companion_state_clear(out);
    if (!enabled) return;
    if (!main_build || main_build[0] == '\0') { out->status = COMPANION_STATE_STARTING; return; }
    if (strncmp(main_build, OOT_SW2_MAIN_BUILD_V131072, 32U) != 0) {
        out->status = COMPANION_STATE_UNSUPPORTED;
        out->gate_reason = COMPANION_GATE_UNSUPPORTED;
        return;
    }
    if (!snap || !snap->valid) { out->status = COMPANION_STATE_STARTING; return; }
    if (snap->in_transition) {
        out->status = COMPANION_STATE_LOADING;
        out->gate_reason = COMPANION_GATE_LOADING;
        return;
    }
    if (snap->max_life <= 0 || snap->scene[0] == '\0' || strcmp(snap->scene, "Title") == 0) {
        out->status = COMPANION_STATE_TITLE;
        return;
    }
    out->status = COMPANION_STATE_READY;
    out->in_game = 1;
    out->menu_open = (pause_hold > 0U) ? 1 : 0;
    out->tablet_owned = 1;
    out->life = snap->life;
    out->max_life = snap->max_life;
    out->stamina = snap->magic;
    out->max_stamina = snap->max_magic;
    out->page = snap->playing_ocarina ? COMPANION_PAGE_OCARINA : user_page;
    out->gate_reason = companion_compute_gate(out, saving, age_ms, max_age_ms);
}

#ifdef __cplusplus
}
#endif

#endif
