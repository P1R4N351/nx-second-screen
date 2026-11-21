// SPDX-License-Identifier: MIT
package io.github.p1r4n351.secondscreen.core;

/**
 * Pure-JDK fail-closed button/melody step state machine mirroring {@code companion_seq.h}.
 * Releases all buttons immediately when {@code gateOk} drops or the sequence completes.
 */
public final class MelodySequencer {
    public static final int MAX_STEPS = 16;

    private final long[] chords = new long[MAX_STEPS];
    private int count;
    private int index;
    private boolean inGap;
    private int tag = -1;
    private long phaseMs;
    private long holdMs;
    private long gapMs;
    private boolean active;

    /** Start a chord sequence at {@code nowMs}. Rejects empty sequences or zero chords. */
    public boolean start(long[] steps, int len, int songTag, long holdDurationMs, long gapDurationMs, long nowMs) {
        if (steps == null || len <= 0 || len > MAX_STEPS || steps.length < len || holdDurationMs <= 0L) {
            return false;
        }
        for (int i = 0; i < len; i++) {
            if (steps[i] == 0L) return false;
        }
        abort();
        System.arraycopy(steps, 0, chords, 0, len);
        count = len;
        tag = songTag;
        phaseMs = nowMs;
        holdMs = holdDurationMs;
        gapMs = Math.max(0L, gapDurationMs);
        active = true;
        return true;
    }

    /** Immediately stop playback and clear all queued steps. */
    public void abort() {
        for (int i = 0; i < MAX_STEPS; i++) chords[i] = 0L;
        count = 0;
        index = 0;
        inGap = false;
        tag = -1;
        active = false;
    }

    /** Advance state at {@code nowMs}. Aborts fail-closed when {@code !gateOk}. */
    public long tick(boolean gateOk, long nowMs) {
        if (!active) return 0L;
        if (!gateOk || nowMs < phaseMs) {
            abort();
            return 0L;
        }
        for (int iter = 0; iter < MAX_STEPS * 2; iter++) {
            long span = inGap ? gapMs : holdMs;
            if (span <= 0L) span = 1L;
            if ((nowMs - phaseMs) < span) break;
            phaseMs += span;
            if (!inGap) {
                if (index + 1 >= count) {
                    abort();
                    return 0L;
                }
                if (gapMs > 0L) inGap = true;
                else index++;
            } else {
                inGap = false;
                index++;
            }
        }
        return inGap ? 0L : chords[index];
    }

    public boolean isActive() { return active; }
    public int currentStep() { return active ? index : -1; }
    public int currentTag() { return active ? tag : -1; }
}
