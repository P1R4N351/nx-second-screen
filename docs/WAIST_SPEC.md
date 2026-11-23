# Second-Screen Waist Specification (`v0.3.0`, MIT)

## 1. Layering and License Boundary

Switch 1 dual-screen companion frameworks (`igawa6/eden-duo`, `igawa6/eden-duo-companions`, `igawa6/peanuts-native`) are `GPL-3.0-or-later`. Permissioned MIT Switch/Switch 2 native ports such as `moceure/BananaNative`, `AurelioB/totk-android`, `P1R4N351/oot-android`, and `P1R4N351/hwaoi-android` require a clean-room MIT interface so they can embed secondary-display telemetry and per-game readers without changing host licensing.

```
  Consumers    Java Presentation shell ─ Core display/burn-in/ring ─ Telemetry / controls / HUD
                               ▲ CompanionStateWords (JNI long[8] or long[13])
  ═══════════ MIT Waist (include/nx_companion/) ═════════════════════════════════════════════════
    W-S        companion_state.h   Numeric state words, hold latch, write-gate precedence
    W-A        companion_verbs.h   Bounded 8-slot request/result ring drained at frame-end
    W-R        companion_probe.h   Read-only [lo, hi) guest probe, MurmurHash3, GameDataMgr
  ═══════════════════════════════════════════════════════════════════════════════════════════════
  Adapters     adapters/oot_sw2/ (OoT 0400486024DEC000 v131072)   adapters/ktgl/ (KTGL AoI/AoC)
```

## 2. State Words (`W-S`: `companion_state.h`)

Word indices are append-only (`-1` means unknown/unread):

| Index | Name | Units / Values | Version |
|---:|---|---|---|
| `0` | `status` | `0` NONE, `1` STARTING, `2` UNSUPPORTED, `3` TITLE, `4` LOADING, `5` READY | v1 |
| `1` | `in_game` | `0`/`1`: save loaded and active | v1 |
| `2` | `menu_open` | `0`/`1`: pause/menu canvas active (latched via `companion_latch_step`) | v1 |
| `3` | `tablet_owned` | `0`/`1`: whether companion slate/device is unlocked | v1 |
| `4` | `life` | Quarter-hearts, `-1` unknown | v1 |
| `5` | `max_life` | Quarter-hearts, `-1` unknown | v1 |
| `6` | `page` | Requested companion page (`-1` unchanged; `4` Ocarina) | v1 |
| `7` | `age_ms` | Sample age in ms | v1 |
| `8` | `dim` | `0`/`1`: movie/event dim latch | v2 |
| `9` | `stamina` | Current magic/stamina points, `-1` unknown | v2 |
| `10` | `max_stamina` | Max magic/stamina points, `-1` unknown | v2 |
| `11` | `stamina_wheel` | Derived (`1` iff `0 <= stamina < max_stamina`) | v2 |
| `12` | `gate_reason` | `0` none, `1` menu, `2` event, `3` loading, `4` saving, `5` stale, `6` unsupported | v2 |

## 3. Action Ring (`W-A`: `companion_verbs.h`)

- `companion_verb_submit()` enqueues a request into the fixed 8-slot ring and assigns a non-zero monotonic sequence ID (`seq`).
- `companion_verb_drain_one()` is invoked only on the host's frame-end boundary (`nvnQueuePresentTexture`). When `active_gate_reason != COMPANION_GATE_NONE`, it records `COMPANION_ACT_REFUSED_GATE` automatically and returns `0`, keeping all game write gates fail-closed above the adapter.

## 4. Runtime Probes (`W-R`: `companion_probe.h`)

- Every guest read passes `[m->lo, m->hi)` bounds checks before invoking `m->read`.
- `nx_probe_chain()` caps dereference depth at `8` and validates `8`-byte alignment on every intermediate pointer.
- `nx_gdm_find_entry()` probes Project Owl `gmd::GameDataMgr` typed sections (`section_stride = 0x60`) using `MurmurHash3_x86_32` with bounded linear probe limits (`<= 16,384`).


## 5. KTGL Warriors Companion Adapter (`adapters/ktgl/ktgl_companion.h`)

- **Build-independent host integration:** Consumes host-level signals (`main_loaded`, `build_ok`, `host_paused`, `movie_playing`, `save_busy`, `save_commits`, stick axes, and physical button mask) with zero guest-RAM offsets, so companion telemetry and macros work across game updates without per-build pointer tables.
- **Detail ABI (`32` `int32_t` + `416` `float` + newline-delimited pin labels):** Packs `CompanionState` words, active party slot, save commit count/age, active sequencer step, `[pos_x, pos_y, pos_z, yaw_deg, fps, frame_ms]`, `128` XYZ trail points (`6..389`), and `8` `[x, y, z]` tactical pins (`390..413`, gated by `pin_mask` in `ints[16]`).
- **Fail-closed chord sequencer:** Dispatches party swaps (`KTGL_ACT_PARTY_0..3`), `R + Y/X/B/A` (`10..13`), `L + Y/X/B/A` (`20..23`), lock-on / special / dodge (`30..32`), and `C1..C6` combos (`40..45`) through `NxCompanionSeq`, aborting immediately if `ktgl_companion_write_safe()` drops or physical face/shoulder buttons are pressed.
