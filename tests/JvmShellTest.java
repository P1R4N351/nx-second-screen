// SPDX-License-Identifier: MIT
import io.github.p1r4n351.secondscreen.core.AmoledBurnInGuard;
import io.github.p1r4n351.secondscreen.core.CompanionStateWords;
import io.github.p1r4n351.secondscreen.core.DisplayClassifier;
import io.github.p1r4n351.secondscreen.core.DisplayClassifier.Role;
import io.github.p1r4n351.secondscreen.core.MelodySequencer;
import io.github.p1r4n351.secondscreen.core.RadarMath;
import io.github.p1r4n351.secondscreen.core.TelemetryRing;

public final class JvmShellTest {
    private static void check(boolean cond, String msg) {
        if (!cond) throw new AssertionError("FAILED: " + msg);
    }

    private static void testDisplayClassifier() {
        check(DisplayClassifier.classify(0, DisplayClassifier.TYPE_INTERNAL, 0, true) == Role.NONE,
                "default display 0 must be NONE");
        check(DisplayClassifier.classify(4, DisplayClassifier.TYPE_INTERNAL, DisplayClassifier.FLAG_PRIVATE, true) == Role.NONE,
                "private display must be NONE");
        check(DisplayClassifier.classify(4, DisplayClassifier.TYPE_INTERNAL, 0, false) == Role.BUILT_IN_SECONDARY,
                "internal secondary display classified as BUILT_IN_SECONDARY");
        check(DisplayClassifier.classify(2, DisplayClassifier.TYPE_EXTERNAL, 0, false) == Role.NONE,
                "external display blocked when opt-out");
        check(DisplayClassifier.classify(2, DisplayClassifier.TYPE_EXTERNAL, 0, true) == Role.EXTERNAL_SECONDARY,
                "external display allowed when opt-in");
    }

    private static void testBurnInGuard() {
        int r = 3;
        int steps = 8 * r;
        long stepMs = 15_000L;
        for (int i = 0; i < steps; i++) {
            int[] p0 = AmoledBurnInGuard.orbitOffset(i * stepMs, stepMs, r);
            int[] p1 = AmoledBurnInGuard.orbitOffset((i + 1) * stepMs, stepMs, r);
            int manhattan = Math.abs(p1[0] - p0[0]) + Math.abs(p1[1] - p0[1]);
            check(manhattan <= 1, "orbit step must move <= 1 px on a single axis at step " + i);
            check(Math.abs(p0[0]) <= r && Math.abs(p0[1]) <= r, "orbit bounded within radius");
        }
        check(AmoledBurnInGuard.brightnessScale(10_000L, 60_000L, 300_000L) == 1.0f, "active brightness");
        check(AmoledBurnInGuard.brightnessScale(60_000L, 60_000L, 300_000L) == 0.25f, "dim threshold");
        check(AmoledBurnInGuard.brightnessScale(300_000L, 60_000L, 300_000L) == 0.0f, "off threshold");
    }

    private static void testCompanionStateWords() {
        long[] v1 = { CompanionStateWords.STATUS_READY, 1, 0, 1, 16, 24, 4, 30 };
        CompanionStateWords w1 = CompanionStateWords.decode(v1);
        check(w1.actionsAllowed(), "v1 READY defaults to actionsAllowed");
        check(!w1.dim && w1.stamina == -1 && w1.gateReason == CompanionStateWords.GATE_NONE, "v1 v2 defaults");

        long[] v2 = {
            CompanionStateWords.STATUS_READY, 1, 1, 1, 16, 24, -1, 50,
            1, 400, 1000, 1, CompanionStateWords.GATE_MENU
        };
        CompanionStateWords w2 = CompanionStateWords.decode(v2);
        check(!w2.actionsAllowed(), "menu gate blocks actions");
        check(w2.dim && w2.stamina == 400 && w2.maxStamina == 1000 && w2.staminaWheel, "v2 stamina decoded");
        check("menu".equals(CompanionStateWords.gateLabel(w2.gateReason)), "gate label matches");
    }

    private static void testTelemetryRing() {
        TelemetryRing ring = new TelemetryRing();
        ring.pushFrameMs(Float.NaN);
        ring.pushFrameMs(-5.0f);
        check(ring.count() == 0 && Float.isNaN(ring.fps()), "invalid frames ignored");

        for (int i = 0; i < 99; i++) ring.pushFrameMs(16.6667f);
        ring.pushFrameMs(50.0f);
        check(ring.count() == 100, "ring count 100");
        check(Math.abs(ring.medianMs() - 16.6667f) < 0.01f, "median matches steady 60 Hz");
        check(Math.abs(ring.onPacePercent(16.6667f, 0.15f) - 99.0f) < 0.1f, "99% on pace");
        check(Math.abs(ring.onePercentLowFps() - 20.0f) < 0.2f, "1% low tracks 50ms spike");

        for (int i = 0; i < 150; i++) ring.pushFrameMs(33.3333f);
        check(ring.count() == TelemetryRing.CAPACITY, "capacity bounded");
        check(Math.abs(ring.fps() - 30.0f) < 0.1f, "steady 30 Hz FPS");
    }

    private static void testRadarAndSequencer() {
        float[] proj = new float[3];
        check(RadarMath.projectXZ(0.0f, 0.0f, 90.0f, true, 50.0f, 25.0f, 0.0f, proj), "heading-up project");
        check(Math.abs(proj[0]) < 1e-4f && Math.abs(proj[1] + 0.5f) < 1e-4f && proj[2] == 0.0f, "forward maps to top");
        check(RadarMath.projectXZ(0.0f, 0.0f, 0.0f, false, 10.0f, 30.0f, 40.0f, proj) && proj[2] == 1.0f, "rim clamp flag");
        check(RadarMath.floorBand(10.0f, 15.0f, 3.0f) == RadarMath.BAND_ABOVE, "above band");
        check("10m ▲".equals(RadarMath.formatPinDelta(0f, 10f, 0f, 6f, 15f, 8f, 3f)), "pin delta formatting");

        MelodySequencer seq = new MelodySequencer();
        check(seq.start(new long[]{1L, 4L}, 2, 0, 100L, 40L, 1000L), "sequencer starts");
        check(seq.tick(true, 1050L) == 1L && seq.isActive(), "step 0 held");
        check(seq.tick(true, 1110L) == 0L && seq.isActive(), "inter-step gap");
        check(seq.tick(false, 1120L) == 0L && !seq.isActive(), "fail-closed abort when gate drops");
    }

    public static void main(String[] args) {
        testDisplayClassifier();
        testBurnInGuard();
        testCompanionStateWords();
        testTelemetryRing();
        testRadarAndSequencer();
        System.out.println("JvmShellTest: ok");
    }
}
