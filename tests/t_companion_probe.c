// SPDX-License-Identifier: MIT
/* Unit tests for MurmurHash3_x86_32, NxGuestMem bounds, Project Owl GameDataMgr,
   and clean-room MIT OoT/KTGL adapters using synthetic guest memory. */
#include "nx_companion/companion_probe.h"
#include "ktgl/ktgl_adapter.h"
#include "oot_sw2/oot_sw2_adapter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
    abort(); } } while (0)

enum { MEM_LO = 0x100000ULL, MEM_SIZE = 0x10000ULL };

typedef struct FakeMem {
    uint8_t data[MEM_SIZE];
} FakeMem;

static int fake_read(void *ctx, uint64_t addr, void *dst, size_t len) {
    const FakeMem *f = (const FakeMem *)ctx;
    if (addr < MEM_LO || addr - MEM_LO + len > MEM_SIZE) return 0;
    memcpy(dst, f->data + (addr - MEM_LO), len);
    return 1;
}

static void wr8(FakeMem *f, uint64_t a, uint8_t v) { f->data[a - MEM_LO] = v; }
static void wr32(FakeMem *f, uint64_t a, uint32_t v) { memcpy(f->data + (a - MEM_LO), &v, 4); }
static void wr64(FakeMem *f, uint64_t a, uint64_t v) { memcpy(f->data + (a - MEM_LO), &v, 8); }
static void wrf32(FakeMem *f, uint64_t a, float v) { memcpy(f->data + (a - MEM_LO), &v, 4); }
static void wrstr(FakeMem *f, uint64_t a, const char *s) { memcpy(f->data + (a - MEM_LO), s, strlen(s) + 1U); }

static void put_gdm_slot(
    FakeMem *f, uint64_t table, uint32_t slots, uint32_t hash, uint32_t idx) {
    uint32_t s = hash % slots;
    for (uint32_t p = 0U; p < slots; p++) {
        uint32_t cur = 0U;
        memcpy(&cur, f->data + (table + (uint64_t)s * 8ULL - MEM_LO), 4);
        if (cur == 0U) {
            wr32(f, table + (uint64_t)s * 8ULL, hash);
            wr32(f, table + (uint64_t)s * 8ULL + 4ULL, idx);
            return;
        }
        s = (s + 1U) % slots;
    }
    CHECK(0 && "hash table full");
}

static void init_section(
    FakeMem *f, uint64_t mgr, uint32_t kind, uint32_t count,
    uint64_t entries, uint64_t table, uint32_t slots) {
    uint64_t sec = mgr + NX_GDM_LAYOUT_OWL.mgr_base_off +
                   (uint64_t)kind * NX_GDM_LAYOUT_OWL.section_stride;
    wr32(f, sec + NX_GDM_LAYOUT_OWL.off_count, count);
    wr64(f, sec + NX_GDM_LAYOUT_OWL.off_entries, entries);
    wr64(f, sec + NX_GDM_LAYOUT_OWL.off_table, table);
    wr32(f, sec + NX_GDM_LAYOUT_OWL.off_slots, slots);
}

static void test_murmur3_and_bounds(void) {
    CHECK(nx_murmur3_x86_32("Sequence_CurrentBanc", 20U, 0U) == OOT_KEY_CURRENT_BANC);
    CHECK(nx_murmur3_x86_32("Sequence_IsChildhood", 20U, 0U) == OOT_KEY_IS_CHILDHOOD);

    FakeMem f;
    memset(&f, 0, sizeof(f));
    const NxGuestMem mem = { fake_read, &f, MEM_LO, MEM_LO + MEM_SIZE };
    uint64_t main_base = MEM_LO;
    wr64(&f, main_base + 0x200ULL, main_base + 0x400ULL);
    wr64(&f, main_base + 0x410ULL, main_base + 0x600ULL);
    wr64(&f, main_base + 0x600ULL, main_base + 0x603ULL); /* misaligned pointer */

    const uint64_t good[2] = { 0x200ULL, 0x10ULL };
    uint64_t out = 0ULL;
    CHECK(nx_probe_chain(&mem, main_base, good, 2U, &out) == 1 && out == main_base + 0x600ULL);

    const uint64_t unaligned[3] = { 0x200ULL, 0x10ULL, 0x0ULL };
    CHECK(nx_probe_chain(&mem, main_base, unaligned, 3U, &out) == 0);

    const uint64_t oob[1] = { MEM_SIZE + 8ULL };
    CHECK(nx_probe_chain(&mem, main_base, oob, 1U, &out) == 0);
}

static void populate_oot_guest(FakeMem *f, uint64_t mgr, uint64_t *next_addr) {
    uint64_t b_ent = 0x104000ULL, b_tab = 0x104200ULL;
    uint64_t i_ent = 0x104400ULL, i_tab = 0x104C00ULL;
    uint64_t s_ent = 0x105200ULL, s_tab = 0x105600ULL;
    uint64_t scene_str = 0x105800ULL, loc_str = 0x105840ULL;
    init_section(f, mgr, 0U, 2U, b_ent, b_tab, 16U);
    init_section(f, mgr, 2U, 10U, i_ent, i_tab, 64U);
    init_section(f, mgr, 16U, 2U, s_ent, s_tab, 16U);

    wrstr(f, scene_str, "MIZUsin");
    wr64(f, s_ent + NX_GDM_LAYOUT_OWL.str64_ptr_off, scene_str);
    put_gdm_slot(f, s_tab, 16U, OOT_KEY_CURRENT_BANC, 0U);
    wrstr(f, loc_str, "Central Pillar");
    wr64(f, s_ent + NX_GDM_LAYOUT_OWL.str64_stride + NX_GDM_LAYOUT_OWL.str64_ptr_off, loc_str);
    put_gdm_slot(f, s_tab, 16U, OOT_KEY_RADAR_LOCATION, 1U);

    wr8(f, b_ent + NX_GDM_LAYOUT_OWL.bool_val_off, 1U);
    put_gdm_slot(f, b_tab, 16U, OOT_KEY_IS_CHILDHOOD, 0U);
    wr8(f, b_ent + NX_GDM_LAYOUT_OWL.bool_stride + NX_GDM_LAYOUT_OWL.bool_val_off, 0U);
    put_gdm_slot(f, b_tab, 16U, OOT_SW2_TRANSITION_KEYS[0], 1U);

    static const uint32_t kIntKeys[10] = {
        OOT_KEY_LIFE, OOT_KEY_MAX_LIFE, OOT_KEY_MAGIC, OOT_KEY_MAX_MAGIC,
        OOT_KEY_ARROWS, OOT_KEY_ARROWS_CAP, OOT_KEY_WALLET_CAP,
        0x877BDCEEU, /* Water Temple small keys */
        0xD53CE3A7U, /* Bottle 0 slot 0 (non-content) */
        0x7D48DF76U  /* Bottle 0 slot 2 (content hash) */
    };
    static const uint32_t kIntVals[10] = {
        24U, 48U, 120U, 240U, 18U, 40U, 500U, 3U, 1U, 0x6F49E357U /* Fairy */
    };
    for (uint32_t i = 0U; i < 10U; i++) {
        wr32(f, i_ent + i * NX_GDM_LAYOUT_OWL.s32_stride + NX_GDM_LAYOUT_OWL.s32_val_off, kIntVals[i]);
        put_gdm_slot(f, i_tab, 64U, kIntKeys[i], i);
    }
    *next_addr = b_ent;
}

typedef struct MockOotCtx {
    FakeMem mem;
    uint64_t main_base;
    uint64_t mgr;
    uint64_t player_obj;
    uint64_t actor;
} MockOotCtx;

static int mock_oot_read(void *ctx, uint64_t addr, void *dst, size_t len) {
    const MockOotCtx *mc = (const MockOotCtx *)ctx;
    if (addr >= MEM_LO && addr - MEM_LO + len <= MEM_SIZE) {
        memcpy(dst, mc->mem.data + (addr - MEM_LO), len);
        return 1;
    }
    if (len == 8U) {
        uint64_t v = 0ULL;
        if (addr == mc->main_base + OOT_SW2_OFF_GDM_ROOT) v = mc->mgr;
        else if (addr == mc->main_base + OOT_SW2_OFF_PLAYER_PTR) v = mc->player_obj;
        else if (addr == mc->player_obj + OOT_SW2_OFF_PLAYER_SLOT) v = mc->actor;
        else return 0;
        memcpy(dst, &v, 8U);
        return 1;
    }
    return 0;
}

static void test_oot_and_ktgl_adapters(void) {
    MockOotCtx *mc = (MockOotCtx *)calloc(1U, sizeof(*mc));
    CHECK(mc != NULL);
    mc->main_base = 0x10000000ULL;
    mc->mgr = 0x102000ULL;
    mc->player_obj = 0x106000ULL;
    mc->actor = 0x107000ULL;

    uint64_t b_ent = 0ULL;
    populate_oot_guest(&mc->mem, mc->mgr, &b_ent);
    wr64(&mc->mem, mc->player_obj + OOT_SW2_OFF_PLAYER_SLOT, mc->actor);
    wrstr(&mc->mem, mc->actor + OOT_SW2_OFF_ACTOR_NAME, "Player");
    wrf32(&mc->mem, mc->actor + OOT_SW2_OFF_ACTOR_XYZ + 0ULL, 125.5f);
    wrf32(&mc->mem, mc->actor + OOT_SW2_OFF_ACTOR_XYZ + 4ULL, 12.0f);
    wrf32(&mc->mem, mc->actor + OOT_SW2_OFF_ACTOR_XYZ + 8ULL, -42.25f);

    const NxGuestMem mem = { mock_oot_read, mc, MEM_LO, mc->main_base + 0x10000000ULL };
    OotSw2Snapshot snap;
    CHECK(oot_sw2_sample(&mem, mc->main_base, &snap) == 1);
    CHECK(snap.valid == 1 && snap.life == 24 && snap.max_life == 48 && snap.magic == 120);
    CHECK(snap.have_player_xyz == 1 && fabsf(snap.player_xyz[0] - 125.5f) < 0.01f);
    CHECK(strcmp(snap.scene, "MIZUsin") == 0);
    CHECK(strcmp(snap.radar_loc, "Central Pillar") == 0);
    CHECK(snap.ammo[OOT_AMMO_ARROWS] == 18 && snap.ammo_cap[OOT_AMMO_ARROWS] == 40);
    CHECK(snap.wallet_cap == 500 && snap.small_keys[3] == 3);
    CHECK(snap.bottles[0] == OOT_BOTTLE_FAIRY && strcmp(oot_sw2_bottle_label(snap.bottles[0]), "Fairy") == 0);
    int32_t d_idx = -1;
    CHECK(strcmp(oot_sw2_resolve_scene(snap.scene, &d_idx), "Water Temple") == 0 && d_idx == 3);
    free(mc);

    snap.playing_ocarina = 1;
    CompanionState st;
    oot_sw2_eval_snapshot(1, "wrongbuild", &snap, 0U, 0, 0, 10LL, 500LL, &st);
    CHECK(st.status == COMPANION_STATE_UNSUPPORTED && st.gate_reason == COMPANION_GATE_UNSUPPORTED);

    oot_sw2_eval_snapshot(1, OOT_SW2_MAIN_BUILD_V131072, &snap, 0U, 1, 0, 10LL, 500LL, &st);
    CHECK(st.status == COMPANION_STATE_READY && st.page == COMPANION_PAGE_OCARINA);
    CHECK(companion_stamina_wheel(&st) == 1 && st.gate_reason == COMPANION_GATE_NONE);

    snap.in_transition = 1;
    oot_sw2_eval_snapshot(1, OOT_SW2_MAIN_BUILD_V131072, &snap, 0U, 1, 0, 10LL, 500LL, &st);
    CHECK(st.status == COMPANION_STATE_LOADING && st.gate_reason == COMPANION_GATE_LOADING);

    KtglHostSignals ksig = { 1, 0, 1, 0U, 2 };
    ktgl_eval_state(1, KTGL_TITLE_AOI, KTGL_AOI_MAIN_BUILD_V196608, NULL, &ksig, 5LL, 500LL, &st);
    CHECK(st.status == COMPANION_STATE_READY && st.dim == 1 && st.gate_reason == COMPANION_GATE_EVENT);
    ksig.in_movie = 0; ksig.save_busy = 1;
    ktgl_eval_state(1, KTGL_TITLE_AOI, KTGL_AOI_MAIN_BUILD_V196608, NULL, &ksig, 5LL, 500LL, &st);
    CHECK(st.gate_reason == COMPANION_GATE_SAVING);
}

static int mock_gdm_read(void *ctx, uint64_t addr, void *dst, size_t len) {
    const uint8_t *buf = (const uint8_t *)ctx;
    uint64_t rel = (addr >= 0x100000ULL) ? (addr - 0x100000ULL) : UINT64_MAX;
    if (rel + len <= 0x10000ULL) {
        memcpy(dst, buf + rel, len);
        return 1;
    }
    return 0;
}

static void test_gdm_lookup(void) {
    FakeMem *f = (FakeMem *)calloc(1U, sizeof(*f));
    CHECK(f != NULL);
    const NxGuestMem mem = { mock_gdm_read, f->data, MEM_LO, MEM_LO + MEM_SIZE };
    uint64_t mgr = 0x102000ULL, b_ent = 0ULL;
    populate_oot_guest(f, mgr, &b_ent);

    char scene[64] = {0};
    int32_t life = 0;
    int is_child = 0;
    CHECK(nx_gdm_read_str64(&mem, mgr, &NX_GDM_LAYOUT_OWL, OOT_KEY_CURRENT_BANC, scene, sizeof(scene)) == 1);
    CHECK(strcmp(scene, "MIZUsin") == 0);
    CHECK(nx_gdm_read_s32(&mem, mgr, &NX_GDM_LAYOUT_OWL, OOT_KEY_LIFE, &life) == 1 && life == 24);
    CHECK(nx_gdm_read_bool(&mem, mgr, &NX_GDM_LAYOUT_OWL, OOT_KEY_IS_CHILDHOOD, &is_child) == 1 && is_child == 1);
    CHECK(nx_gdm_read_s32(&mem, mgr, &NX_GDM_LAYOUT_OWL, 0xDEADBEEFU, &life) == 0);
    CHECK(b_ent == 0x104000ULL);
    free(f);
}

int main(void) {
    test_murmur3_and_bounds();
    test_gdm_lookup();
    test_oot_and_ktgl_adapters();
    puts("t_companion_probe: ok");
    return 0;
}
