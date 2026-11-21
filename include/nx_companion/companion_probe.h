// SPDX-License-Identifier: MIT
/* Narrow second-screen runtime probe waist (W-R): bounded read-only guest
   memory primitives, MurmurHash3_x86_32, and Project Owl GameDataMgr helpers. */
#ifndef NX_COMPANION_PROBE_H
#define NX_COMPANION_PROBE_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    NX_PROBE_MAX_CHAIN = 8U,
    NX_PROBE_MAX_STR = 256U,
    NX_PROBE_MAX_READ = 256U,
    NX_GDM_MAX_ENTRIES = 16384U
};

typedef struct NxGuestMem {
    int (*read)(void *ctx, uint64_t addr, void *dst, size_t len);
    void *ctx;
    uint64_t lo;
    uint64_t hi;
} NxGuestMem;

typedef struct NxGdmLayout {
    uint32_t mgr_base_off;
    uint32_t section_stride;
    uint32_t off_count;
    uint32_t off_entries;
    uint32_t off_table;
    uint32_t off_slots;
    uint32_t bool_stride;
    uint32_t bool_val_off;
    uint32_t s32_stride;
    uint32_t s32_val_off;
    uint32_t str64_stride;
    uint32_t str64_ptr_off;
} NxGdmLayout;

static const NxGdmLayout NX_GDM_LAYOUT_OWL = {
    0x008U, 0x060U,
    0x008U, 0x010U, 0x018U, 0x024U,
    0x018U, 0x016U,
    0x028U, 0x01CU,
    0x108U, 0x068U
};

static inline uint32_t nx_rotl32(uint32_t v, uint32_t r) {
    return (v << r) | (v >> (32U - r));
}

/* Read len bytes safely inside [m->lo, m->hi). Returns 1 on success, 0 on fault. */
static inline int nx_guest_read(const NxGuestMem *m, uint64_t addr, void *dst, size_t len) {
    assert(dst != NULL);
    if (!m || !m->read || len == 0U || len > (size_t)NX_PROBE_MAX_READ) return 0;
    if (addr < m->lo || addr > m->hi) return 0;
    if ((uint64_t)len > m->hi - addr) return 0;
    return m->read(m->ctx, addr, dst, len) != 0;
}

static inline int nx_guest_read8(const NxGuestMem *m, uint64_t addr, uint8_t *out) {
    assert(out != NULL);
    return nx_guest_read(m, addr, out, sizeof(*out));
}

static inline int nx_guest_read32(const NxGuestMem *m, uint64_t addr, uint32_t *out) {
    assert(out != NULL);
    return nx_guest_read(m, addr, out, sizeof(*out));
}

static inline int nx_guest_read64(const NxGuestMem *m, uint64_t addr, uint64_t *out) {
    assert(out != NULL);
    return nx_guest_read(m, addr, out, sizeof(*out));
}

/* Dereference up to NX_PROBE_MAX_CHAIN aligned 64-bit guest pointers safely. */
static inline int nx_probe_chain(
    const NxGuestMem *m, uint64_t base, const uint64_t *offsets, size_t depth, uint64_t *out_addr) {
    assert(out_addr != NULL);
    *out_addr = 0ULL;
    if (!m || !offsets || depth == 0U || depth > (size_t)NX_PROBE_MAX_CHAIN) return 0;

    uint64_t cur = base;
    for (size_t i = 0U; i < depth; i++) {
        if (offsets[i] > UINT64_MAX - cur) return 0;
        uint64_t slot_addr = cur + offsets[i];
        uint64_t next = 0ULL;
        if (!nx_guest_read64(m, slot_addr, &next)) return 0;
        if (next == 0ULL || (next & 7ULL) != 0ULL) return 0;
        cur = next;
    }
    *out_addr = cur;
    return 1;
}

/* Bounded NUL-terminated ASCII/UTF-8 read. Rejects control bytes below 0x20. */
static inline int nx_guest_read_cstr(
    const NxGuestMem *m, uint64_t addr, char *dst, size_t cap) {
    assert(dst != NULL);
    if (cap == 0U) return 0;
    dst[0] = '\0';
    size_t lim = cap - 1U;
    if (lim > (size_t)NX_PROBE_MAX_STR) lim = (size_t)NX_PROBE_MAX_STR;
    for (size_t i = 0U; i < lim; i++) {
        uint8_t ch = 0U;
        if (!nx_guest_read8(m, addr + (uint64_t)i, &ch)) { dst[0] = '\0'; return 0; }
        if (ch == 0U) { dst[i] = '\0'; return 1; }
        if (ch < 0x20U && ch != '\t' && ch != '\n') { dst[0] = '\0'; return 0; }
        dst[i] = (char)ch;
    }
    dst[lim] = '\0';
    return 1;
}

/* Public-domain MurmurHash3_x86_32 with explicit upper input length bound. */
static inline uint32_t nx_murmur3_x86_32(const void *data, size_t len, uint32_t seed) {
    if ((!data && len > 0U) || len > (size_t)NX_PROBE_MAX_STR) return 0U;
    const uint8_t *p = (const uint8_t *)data;
    uint32_t h1 = seed;
    size_t i = 0U;
    for (; i + 4U <= len; i += 4U) {
        uint32_t k1 = (uint32_t)p[i] | ((uint32_t)p[i + 1U] << 8U) |
                      ((uint32_t)p[i + 2U] << 16U) | ((uint32_t)p[i + 3U] << 24U);
        k1 = nx_rotl32(k1 * 0xCC9E2D51U, 15U) * 0x1B873593U;
        h1 = nx_rotl32(h1 ^ k1, 13U) * 5U + 0xE6546B64U;
    }
    uint32_t tail = 0U;
    size_t rem = len - i;
    if (rem >= 3U) tail ^= (uint32_t)p[i + 2U] << 16U;
    if (rem >= 2U) tail ^= (uint32_t)p[i + 1U] << 8U;
    if (rem >= 1U) {
        tail ^= (uint32_t)p[i];
        h1 ^= nx_rotl32(tail * 0xCC9E2D51U, 15U) * 0x1B873593U;
    }
    h1 ^= (uint32_t)len;
    h1 ^= h1 >> 16U;
    h1 *= 0x85EBCA6BU;
    h1 ^= h1 >> 13U;
    h1 *= 0xC2B2AE35U;
    return h1 ^ (h1 >> 16U);
}

/* Look up typed entry address inside Project Owl gmd::GameDataMgr hash table. */
static inline uint64_t nx_gdm_find_entry(
    const NxGuestMem *m, uint64_t mgr, const NxGdmLayout *lay,
    uint32_t kind, uint32_t stride, uint32_t hash) {
    assert(lay != NULL);
    if (!m || mgr == 0ULL || hash == 0U || stride == 0U) return 0ULL;
    uint64_t sec = mgr + (uint64_t)lay->mgr_base_off + (uint64_t)kind * (uint64_t)lay->section_stride;
    uint32_t count = 0U, slots = 0U;
    uint64_t entries = 0ULL, table = 0ULL;
    if (!nx_guest_read32(m, sec + lay->off_count, &count) ||
        !nx_guest_read64(m, sec + lay->off_entries, &entries) ||
        !nx_guest_read64(m, sec + lay->off_table, &table) ||
        !nx_guest_read32(m, sec + lay->off_slots, &slots)) return 0ULL;
    if (count == 0U || count > (uint32_t)NX_GDM_MAX_ENTRIES || slots == 0U ||
        slots > (uint32_t)(NX_GDM_MAX_ENTRIES * 2U) || entries == 0ULL || table == 0ULL) return 0ULL;

    uint32_t slot = hash % slots;
    for (uint32_t probe = 0U; probe < slots && probe < (uint32_t)NX_GDM_MAX_ENTRIES; probe++) {
        uint32_t pair[2] = {0U, 0U};
        if (!nx_guest_read(m, table + (uint64_t)slot * 8ULL, pair, sizeof(pair)) || pair[0] == 0U) {
            return 0ULL;
        }
        if (pair[0] == hash) {
            return (pair[1] < count) ? (entries + (uint64_t)pair[1] * (uint64_t)stride) : 0ULL;
        }
        slot = (slot + 1U < slots) ? (slot + 1U) : 0U;
    }
    return 0ULL;
}

static inline int nx_gdm_read_bool(
    const NxGuestMem *m, uint64_t mgr, const NxGdmLayout *lay, uint32_t hash, int *out) {
    assert(out != NULL);
    uint64_t e = nx_gdm_find_entry(m, mgr, lay, 0U, lay->bool_stride, hash);
    uint8_t v = 0U;
    if (e == 0ULL || !nx_guest_read8(m, e + lay->bool_val_off, &v)) return 0;
    *out = (v != 0U) ? 1 : 0;
    return 1;
}

static inline int nx_gdm_read_s32(
    const NxGuestMem *m, uint64_t mgr, const NxGdmLayout *lay, uint32_t hash, int32_t *out) {
    assert(out != NULL);
    uint64_t e = nx_gdm_find_entry(m, mgr, lay, 2U, lay->s32_stride, hash);
    uint32_t raw = 0U;
    if (e == 0ULL || !nx_guest_read32(m, e + lay->s32_val_off, &raw)) return 0;
    *out = (int32_t)raw;
    return 1;
}

static inline int nx_gdm_read_str64(
    const NxGuestMem *m, uint64_t mgr, const NxGdmLayout *lay, uint32_t hash, char *dst, size_t cap) {
    assert(dst != NULL);
    if (cap == 0U) return 0;
    dst[0] = '\0';
    uint64_t e = nx_gdm_find_entry(m, mgr, lay, 16U, lay->str64_stride, hash);
    uint64_t ptr = 0ULL;
    if (e == 0ULL || !nx_guest_read64(m, e + lay->str64_ptr_off, &ptr) || ptr == 0ULL) return 0;
    return nx_guest_read_cstr(m, ptr, dst, cap);
}

#ifdef __cplusplus
}
#endif

#endif
