// SPDX-License-Identifier: MIT
package io.github.p1r4n351.secondscreen.core;

/** Bounded 1-pixel AMOLED burn-in pixel orbit and idle dim/off policy. */
public final class AmoledBurnInGuard {
    public static final int MAX_RADIUS_PX = 8;

    private AmoledBurnInGuard() {}

    /** Step around a [-radius, +radius] square perimeter by at most 1 px per step. */
    public static int[] orbitOffset(long elapsedMs, long stepMs, int radiusPx) {
        int r = Math.max(0, Math.min(MAX_RADIUS_PX, radiusPx));
        if (r == 0 || stepMs <= 0L) return new int[] { 0, 0 };
        long clampedMs = Math.max(0L, elapsedMs);
        int period = 8 * r;
        int step = (int)((clampedMs / stepMs) % period);

        if (step <= r) return new int[] { r, step };
        step -= r;
        if (step <= 2 * r) return new int[] { r - step, r };
        step -= 2 * r;
        if (step <= 2 * r) return new int[] { -r, r - step };
        step -= 2 * r;
        if (step <= 2 * r) return new int[] { -r + step, -r };
        step -= 2 * r;
        return new int[] { r, -r + step };
    }

    /** Return UI brightness multiplier (1.0 normal, 0.25 dimmed, 0.0 blanked). */
    public static float brightnessScale(long idleMs, long dimAfterMs, long offAfterMs) {
        if (dimAfterMs <= 0L || offAfterMs <= dimAfterMs) return 1.0f;
        if (idleMs >= offAfterMs) return 0.0f;
        if (idleMs >= dimAfterMs) return 0.25f;
        return 1.0f;
    }
}
