// SPDX-License-Identifier: MIT
import io.github.p1r4n351.secondscreen.core.AmoledBurnInGuard;
import io.github.p1r4n351.secondscreen.core.CompanionStateWords;
import io.github.p1r4n351.secondscreen.core.DisplayClassifier;
import io.github.p1r4n351.secondscreen.core.DisplayClassifier.Role;
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

    public static void main(String[] args) {
        testDisplayClassifier();
        testBurnInGuard();
        testCompanionStateWords();
        testTelemetryRing();
        System.out.println("JvmShellTest: ok");
    }
}
