// SPDX-License-Identifier: MIT
/* Unit tests for 3D spatial breadcrumb ring, elevation bands, scene reset,
   waypoint pins, and North-Up / Heading-Up radar projection. */
#include "nx_companion/companion_trail.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
    abort(); } } while (0)

static void test_trail_ring_and_scene_reset(void) {
    NxCompanionTrail tr;
    nx_trail_init(&tr, 2.0f, 3.5f);
    CHECK(nx_trail_push(&tr, "Spot04", 0.0f, 0.0f, 0.0f) == 1);
    CHECK(nx_trail_push(&tr, "Spot04", 1.0f, 0.0f, 1.0f) == 0); /* < 2.0 m skipped */
    CHECK(nx_trail_push(&tr, "Spot04", NAN, 0.0f, 0.0f) == 0);  /* NaN rejected */
    CHECK(tr.count == 1U);

    for (uint32_t i = 1U; i <= 140U; i++) {
        CHECK(nx_trail_push(&tr, "Spot04", (float)i * 3.0f, (float)(i % 5U) * 2.0f, 0.0f) == 1);
    }
    CHECK(tr.count == NX_TRAIL_CAP);
    NxTrailPoint oldest;
    CHECK(nx_trail_get_oldest(&tr, 0U, &oldest) == 1);
    /* Pushed 0, 3, 6, ..., 420 -> 141 points; oldest kept is index 13 = 39.0f */
    CHECK(fabsf(oldest.x - 39.0f) < 0.01f);

    CHECK(nx_trail_pin_add(&tr, 50.0f, 10.0f, -10.0f, "Chest") == 0);
    CHECK(tr.pins[0].active == 1U);

    /* Changing scene clears both breadcrumbs and scene-local pins. */
    CHECK(nx_trail_push(&tr, "MIZUsin", 5.0f, -6.0f, 1.0f) == 1);
    CHECK(tr.count == 1U && tr.pins[0].active == 0U);
    CHECK(strcmp(tr.scene, "MIZUsin") == 0);
}

static void test_floor_bands_and_pins(void) {
    NxCompanionTrail tr;
    nx_trail_init(&tr, 1.0f, 3.0f);
    CHECK(nx_trail_floor_band(&tr, 10.0f, 14.0f) == NX_BAND_ABOVE);
    CHECK(nx_trail_floor_band(&tr, 10.0f, 12.0f) == NX_BAND_LEVEL);
    CHECK(nx_trail_floor_band(&tr, 10.0f, 6.0f) == NX_BAND_BELOW);

    for (uint32_t i = 0U; i < NX_WAYPOINT_CAP; i++) {
        CHECK(nx_trail_pin_add(&tr, (float)i, 0.0f, 0.0f, NULL) == (int)i);
    }
    CHECK(nx_trail_pin_add(&tr, 99.0f, 0.0f, 0.0f, "Overflow") == -1);
    CHECK(nx_trail_pin_remove(&tr, 3U) == 1);
    CHECK(nx_trail_pin_add(&tr, 99.0f, 0.0f, 0.0f, "Reused") == 3);
    nx_trail_pins_clear(&tr);
    for (uint32_t i = 0U; i < NX_WAYPOINT_CAP; i++) CHECK(tr.pins[i].active == 0U);
}

static void test_projection_so2_and_clamping(void) {
    float nx = 0.0f, ny = 0.0f;
    /* North-up: +dx = right (+x), -dz = up (-y). */
    CHECK(nx_trail_project_xz(10.0f, 20.0f, 90.0f, 0, 50.0f, 35.0f, 20.0f, &nx, &ny) == 1);
    CHECK(fabsf(nx - 0.5f) < 1e-4f && fabsf(ny) < 1e-4f);

    /* Heading-up at yaw=90 deg (facing East): point due East maps straight ahead (0, -0.5). */
    CHECK(nx_trail_project_xz(10.0f, 20.0f, 90.0f, 1, 50.0f, 35.0f, 20.0f, &nx, &ny) == 1);
    CHECK(fabsf(nx) < 1e-4f && fabsf(ny + 0.5f) < 1e-4f);

    /* SO(2) isometry preserves radius under arbitrary yaw rotation. */
    float n1x = 0.0f, n1y = 0.0f, n2x = 0.0f, n2y = 0.0f;
    CHECK(nx_trail_project_xz(0.0f, 0.0f, 0.0f, 0, 100.0f, 18.0f, -24.0f, &n1x, &n1y) == 1);
    CHECK(nx_trail_project_xz(0.0f, 0.0f, 37.5f, 1, 100.0f, 18.0f, -24.0f, &n2x, &n2y) == 1);
    CHECK(fabsf(hypotf(n1x, n1y) - hypotf(n2x, n2y)) < 1e-4f);

    /* Out-of-radius point clamps cleanly to unit circle rim (return 2). */
    CHECK(nx_trail_project_xz(0.0f, 0.0f, 0.0f, 0, 10.0f, 30.0f, 40.0f, &nx, &ny) == 2);
    CHECK(fabsf(hypotf(nx, ny) - 1.0f) < 1e-4f);
}

int main(void) {
    test_trail_ring_and_scene_reset();
    test_floor_bands_and_pins();
    test_projection_so2_and_clamping();
    puts("t_companion_trail: ok");
    return 0;
}
