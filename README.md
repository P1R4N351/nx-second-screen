# nx-second-screen

Clean-room **MIT-licensed** C11 game-state waist (`W-S`, `W-A`, `W-R`), bounded guest-memory adapters, and Android secondary-display `Presentation` shell for native Switch / Switch 2 handheld ports (targeting dual-screen devices such as the **AYN Thor**).

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE) [![Support with Bitcoin](https://img.shields.io/badge/Support_with-Bitcoin-F7931A?style=for-the-badge&logo=bitcoin&logoColor=white)](#support)

## Why This Exists

Existing Switch 1 second-screen stacks (`eden-duo`, `eden-duo-companions`, `peanuts-native`) are `GPL-3.0-or-later`. MIT-licensed Switch / Switch 2 native hosts (`BananaNative`, `oot-android`, `hwaoi-android`, `totk-android`) need a standalone **clean-room MIT** boundary so:

1. **All games** get a universal, zero-allocation second-screen telemetry + quick-controls `Presentation` without stealing Display 0 gamepad focus (`FLAG_NOT_FOCUSABLE | FLAG_KEEP_SCREEN_ON`) or burning in AMOLED panels (`AmoledBurnInGuard`).
2. **Games with mapped state readers** feed numeric words and guarded frame-end action verbs through one narrow append-only ABI (`CompanionState`, v1 `8`-word and v2 `13`-word compatible).

See [docs/WAIST_SPEC.md](docs/WAIST_SPEC.md) for field layouts and gate precedence.

## Repository Layout

- **`include/nx_companion/companion_state.h`** — Append-only `CompanionState` (`v1`/`v2`), derived `stamina_wheel`, bounded `companion_latch_step`, and `companion_compute_gate`.
- **`include/nx_companion/companion_verbs.h`** — Fixed 8-slot fail-closed frame-end action queue (`CompanionVerbQueue`).
- **`include/nx_companion/companion_probe.h`** — `[lo, hi)` bounds-checked `NxGuestMem` reads, `8`-byte-aligned `nx_probe_chain`, `nx_murmur3_x86_32`, and Project Owl `gmd::GameDataMgr` lookup helpers.
- **`adapters/oot_sw2/oot_sw2_adapter.h`** — Clean-room MIT adapter for Switch 2 *Ocarina of Time* (`0400486024DEC000`, `v131072` build `e0f3ccfaf50065ae4ea01eb5aeb13b1a`, MIT offsets from `NaviScout`, `OOT-SW2SE`, `Links-Toolbox`, `BananaNative`).
- **`adapters/ktgl/ktgl_adapter.h`** — Clean-room MIT KTGL adapter scaffold (`04003F40232DE000` *Hyrule Warriors: Age of Imprisonment* `v196608` / *Age of Calamity*), gated by `main` build ID, host `save_busy`, and `in_movie` dim state.
- **`android/core/`** — Pure-Java secondary-display core (`DisplayClassifier`, `AmoledBurnInGuard`, `CompanionStateWords`, `TelemetryRing`) with zero Android framework dependencies so hosts can compile and unit-test on desktop JDK.
- **`android/presentation/`** — Thin Android SDK `SecondScreenPresentation` helper (`FLAG_NOT_FOCUSABLE`, `onStart`/`onStop` visibility sync, AMOLED orbit transform). Note: compiling `android/presentation/` requires Android SDK `android.jar`; `make test` compiles and verifies all C11 headers/adapters and `android/core/` standalone.
- **`tests/` & `scripts/verify_repo.py`** — C11 ASan/UBSan tests (`t_companion_waist`, `t_companion_verbs`, `t_companion_probe`), `JvmShellTest`, SPDX MIT header verifier, and leak check.

## Building and Testing

```bash
make test
python3 scripts/verify_repo.py
```

Both commands require `clang` (with ASan/UBSan), `javac`/`java`, and `python3`.

## BYOG & Legal

This repository contains **zero Nintendo binaries, keys, firmware, fonts, images, or extracted game assets**. Any per-game layout or font rendering in consumer apps must load assets exclusively from the user's own legally dumped copy.

## Support

- Bitcoin: `bc1q4a3lqcg87rvw7vxulausdqhhuwkwwa6w6v4mxx`
- VitaDB profile: [PiSCES](https://www.rinnegatamante.eu/vitadb/#/user/PiSCES)
