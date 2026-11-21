// SPDX-License-Identifier: MIT
package io.github.p1r4n351.secondscreen.core;

import java.util.Arrays;

/** Fixed-capacity zero-allocation frame-time ring for fallback second-screen stats. */
public final class TelemetryRing {
    public static final int CAPACITY = 120;

    private final float[] ring = new float[CAPACITY];
    private final float[] scratch = new float[CAPACITY];
    private int head;
    private int count;

    /** Record one frame duration in milliseconds; ignores non-positive or non-finite samples. */
    public void pushFrameMs(float frameMs) {
        if (!Float.isFinite(frameMs) || frameMs <= 0.0f) return;
        float clamped = Math.min(5000.0f, Math.max(0.1f, frameMs));
        ring[head] = clamped;
        head = (head + 1) % CAPACITY;
        if (count < CAPACITY) count++;
    }

    /** Clear all recorded samples. */
    public void reset() {
        head = 0;
        count = 0;
        Arrays.fill(ring, 0.0f);
    }

    /** Number of valid samples stored in the ring (0..CAPACITY). */
    public int count() {
        return count;
    }

    /** Compute harmonic mean FPS across stored samples, or NaN when empty. */
    public float fps() {
        if (count == 0) return Float.NaN;
        double sumMs = 0.0;
        int start = (head - count + CAPACITY) % CAPACITY;
        for (int i = 0; i < count; i++) {
            sumMs += ring[(start + i) % CAPACITY];
        }
        return sumMs > 0.0 ? (float)(count * 1000.0 / sumMs) : Float.NaN;
    }

    /** Return median frame duration in milliseconds, or NaN when empty. */
    public float medianMs() {
        int n = copySorted();
        if (n == 0) return Float.NaN;
        if ((n & 1) == 1) return scratch[n / 2];
        return 0.5f * (scratch[n / 2 - 1] + scratch[n / 2]);
    }

    /** Return 1% low FPS (from 99th-percentile frame time), or NaN when empty. */
    public float onePercentLowFps() {
        int n = copySorted();
        if (n == 0) return Float.NaN;
        int idx = Math.min(n - 1, (n * 99) / 100);
        float worstMs = scratch[idx];
        return worstMs > 0.0f ? (1000.0f / worstMs) : Float.NaN;
    }

    /** Return percentage (0..100) of frames within targetMs * (1 + toleranceRatio). */
    public float onPacePercent(float targetMs, float toleranceRatio) {
        if (count == 0 || !Float.isFinite(targetMs) || targetMs <= 0.0f) return Float.NaN;
        float limit = targetMs * (1.0f + Math.max(0.0f, toleranceRatio));
        int onPace = 0;
        int start = (head - count + CAPACITY) % CAPACITY;
        for (int i = 0; i < count; i++) {
            if (ring[(start + i) % CAPACITY] <= limit) onPace++;
        }
        return (onPace * 100.0f) / (float)count;
    }

    private int copySorted() {
        if (count == 0) return 0;
        int start = (head - count + CAPACITY) % CAPACITY;
        for (int i = 0; i < count; i++) {
            scratch[i] = ring[(start + i) % CAPACITY];
        }
        Arrays.sort(scratch, 0, count);
        return count;
    }
}
