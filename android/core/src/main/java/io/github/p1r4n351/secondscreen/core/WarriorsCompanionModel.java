// SPDX-License-Identifier: MIT
package io.github.p1r4n351.secondscreen.core;

import java.util.Locale;

/**
 * Pure-JVM model for the converged Age of Calamity / Age of Imprisonment second screen.
 * Holds telemetry unpacked from ktgl_companion.h, tactical radar state, stopwatch,
 * KO counter, chord/combo catalogs, cutscene auto-dimming, and AMOLED pixel orbit.
 */
public final class WarriorsCompanionModel {
    public static final int DETAIL_INTS = 32, DETAIL_FLOATS = 416;
    public static final int TRAIL_CAP = 128, PIN_CAP = 8, FPS_CAP = 64;
    public static final int TAB_TACTICAL = 0, TAB_COMMAND = 1, TAB_GRIMOIRE = 2, TAB_LEDGER = 3, TAB_COUNT = 4;
    public static final int GAME_AOC = 1, GAME_AOI = 2;

    public static final int ACT_PARTY_BASE = 0, ACT_R_BASE = 10, ACT_L_BASE = 20;
    public static final int ACT_LOCKON = 30, ACT_SPECIAL = 31, ACT_DODGE = 32, ACT_COMBO_BASE = 40;

    public int status, gameKind = GAME_AOI, activeParty, presents, saveCommits, lastSaveAgeS = -1;
    public int trailCount, seqTag = -1, seqStep, seqCount, keepCount;
    public boolean inGame, writeSafe, moviePlaying, hostPaused, saveBusy, seqActive, inputActive;
    public boolean headingUp = true, macroMode = true, autoDim = true;
    public float posX, posY, posZ, yawDeg, fps, frameMs;
    public final float[] trailX = new float[TRAIL_CAP], trailY = new float[TRAIL_CAP], trailZ = new float[TRAIL_CAP];
    public final boolean[] pinActive = new boolean[PIN_CAP];
    public final float[] pinX = new float[PIN_CAP], pinY = new float[PIN_CAP], pinZ = new float[PIN_CAP];
    public final String[] pinLabels = new String[PIN_CAP];
    public final float[] fpsHist = new float[FPS_CAP];
    public int fpsHistCount, fpsHistHead, pinPresetIdx;

    private static final float[] ZOOM_METERS = {40f, 80f, 160f};
    private int tab = TAB_TACTICAL, zoomIdx = 1, orbitStep;
    private boolean seedApplied, timerRunning;
    private long lastOrbitMs = -1L, timerAccumMs, timerStartMs;

    public boolean update(int[] ints, float[] flts, String txt, long nowMs) {
        updateOrbit(nowMs);
        if (ints == null || flts == null || ints.length < DETAIL_INTS || flts.length < DETAIL_FLOATS) {
            clearTelemetry();
            return false;
        }
        unpackScalarInts(ints);
        unpackScalarFloats(flts);
        unpackTrail(flts);
        unpackPins(ints[16], flts, txt);
        recordFps(fps);
        if (!seedApplied) {
            seedApplied = true;
            if (ints[18] == 1) headingUp = true;
            if (ints[19] >= 0 && ints[19] < TAB_COUNT) tab = ints[19];
        }
        return true;
    }

    private void clearTelemetry() {
        status = CompanionStateWords.STATUS_NONE;
        inGame = false; writeSafe = false; moviePlaying = false; hostPaused = false;
        saveBusy = false; seqActive = false; inputActive = false;
        trailCount = 0; seqTag = -1; seqStep = 0; seqCount = 0; lastSaveAgeS = -1;
        fps = 0f; frameMs = 0f;
        for (int i = 0; i < PIN_CAP; i++) { pinActive[i] = false; pinLabels[i] = ""; }
    }

    private void unpackScalarInts(int[] ints) {
        status = ints[0];
        inGame = ints[1] != 0;
        writeSafe = ints[2] != 0;
        moviePlaying = ints[3] != 0;
        hostPaused = ints[4] != 0;
        saveBusy = ints[5] != 0;
        gameKind = (ints[6] == GAME_AOC) ? GAME_AOC : GAME_AOI;
        activeParty = clamp(ints[7], 0, 3);
        seqActive = ints[8] != 0;
        seqTag = ints[9];
        seqStep = ints[10];
        seqCount = ints[11];
        presents = ints[12];
        saveCommits = ints[13];
        lastSaveAgeS = ints[14];
        trailCount = clamp(ints[15], 0, TRAIL_CAP);
        inputActive = ints[21] != 0;
    }

    private void unpackScalarFloats(float[] flts) {
        posX = flts[0]; posY = flts[1]; posZ = flts[2];
        yawDeg = flts[3]; fps = flts[4]; frameMs = flts[5];
    }

    private void unpackTrail(float[] flts) {
        for (int i = 0; i < trailCount; i++) {
            int b = 6 + i * 3;
            trailX[i] = flts[b]; trailY[i] = flts[b + 1]; trailZ[i] = flts[b + 2];
        }
    }

    private void unpackPins(int mask, float[] flts, String txt) {
        String[] lines = (txt != null && !txt.isEmpty()) ? txt.split("\n", -1) : new String[0];
        for (int i = 0; i < PIN_CAP; i++) {
            pinActive[i] = (mask & (1 << i)) != 0;
            int b = 390 + i * 3;
            pinX[i] = flts[b]; pinY[i] = flts[b + 1]; pinZ[i] = flts[b + 2];
            String lbl = (i < lines.length && lines[i] != null) ? lines[i].trim() : "";
            pinLabels[i] = lbl.isEmpty() ? ("Pin " + (i + 1)) : lbl;
        }
    }

    private void recordFps(float val) {
        if (val <= 0f || Float.isNaN(val)) return;
        fpsHist[fpsHistHead] = val;
        fpsHistHead = (fpsHistHead + 1) % FPS_CAP;
        if (fpsHistCount < FPS_CAP) fpsHistCount++;
    }

    public void updateOrbit(long nowMs) {
        if (lastOrbitMs < 0L) lastOrbitMs = nowMs;
        if (nowMs - lastOrbitMs >= 60_000L) {
            orbitStep = (orbitStep + 1) & 7;
            lastOrbitMs = nowMs;
        }
    }

    public int orbitDx() {
        switch (orbitStep) {
            case 1: case 2: case 3: return 1;
            case 5: case 6: case 7: return -1;
            default: return 0;
        }
    }

    public int orbitDy() {
        switch (orbitStep) {
            case 3: case 4: case 5: return 1;
            case 0: case 1: case 7: return -1;
            default: return 0;
        }
    }

    public int activeTab() { return tab; }
    public void selectTab(int t) { if (t >= 0 && t < TAB_COUNT) tab = t; }
    public void toggleHeadingUp() { headingUp = !headingUp; }
    public void toggleMacroMode() { macroMode = !macroMode; }
    public void toggleAutoDim() { autoDim = !autoDim; }
    public int cycleZoom() { zoomIdx = (zoomIdx + 1) % ZOOM_METERS.length; return zoomIdx; }
    public float radiusMeters() { return ZOOM_METERS[zoomIdx]; }
    public int cyclePreset() { pinPresetIdx = (pinPresetIdx + 1) % 5; return pinPresetIdx; }
    public boolean dimmed() { return autoDim && moviePlaying; }
    public boolean touchBlocked() { return dimmed(); }

    public boolean timerRunning() { return timerRunning; }
    public void toggleTimer(long nowMs) {
        if (timerRunning) { timerAccumMs += Math.max(0L, nowMs - timerStartMs); timerRunning = false; }
        else { timerStartMs = nowMs; timerRunning = true; }
    }
    public void resetTimer(long nowMs) { timerAccumMs = 0L; timerStartMs = nowMs; timerRunning = false; }
    public String formatTimer(long nowMs) {
        long ms = timerAccumMs + (timerRunning ? Math.max(0L, nowMs - timerStartMs) : 0L);
        long s = ms / 1000L;
        return String.format(Locale.ROOT, "%02d:%02d.%d", s / 60L, s % 60L, (ms % 1000L) / 100L);
    }

    public void adjustKeep(int delta) { keepCount = clamp(keepCount + delta, 0, 9999); }
    public void resetKeep() { keepCount = 0; }

    public int pinCount() {
        int n = 0;
        for (int i = 0; i < PIN_CAP; i++) if (pinActive[i]) n++;
        return n;
    }

    public boolean projectXZ(float x, float z, float[] out) {
        if (out == null || out.length < 3 || !Float.isFinite(posX) || !Float.isFinite(posZ)) return false;
        float dx = x - posX, dz = z - posZ;
        float rx = dx, rz = dz;
        if (headingUp) {
            double rad = Math.toRadians(yawDeg);
            float c = (float) Math.cos(rad), s = (float) Math.sin(rad);
            rx = dx * c + dz * s;
            rz = -dx * s + dz * c;
        }
        float r = radiusMeters();
        float nx = rx / r, ny = rz / r;
        float mag = (float) Math.hypot(nx, ny);
        out[2] = mag > 1f ? 1f : 0f;
        out[0] = mag > 1f ? (nx / mag) : nx;
        out[1] = mag > 1f ? (ny / mag) : ny;
        return true;
    }

    public String formatPinDelta(int slot) {
        if (slot < 0 || slot >= PIN_CAP || !pinActive[slot]) return "";
        float dx = pinX[slot] - posX, dy = pinY[slot] - posY, dz = pinZ[slot] - posZ;
        float d = (float) Math.hypot(dx, dz);
        String dist = d >= 999.5f ? String.format(Locale.ROOT, "%.1fkm", d / 1000f) : (Math.round(d) + "m");
        return dy > 3f ? (dist + " \u25b2") : dy < -3f ? (dist + " \u25bc") : dist;
    }

    public int bgColor() { return gameKind == GAME_AOC ? 0xFF0E131B : 0xFF141311; }
    public int cardColor() { return gameKind == GAME_AOC ? 0xFF151D29 : 0xFF1C1A17; }
    public int borderColor() { return gameKind == GAME_AOC ? 0xFF28374A : 0xFF332F28; }
    public int primaryColor() { return gameKind == GAME_AOC ? 0xFF38BDF8 : 0xFF5AC8B8; }
    public int secondaryColor() { return gameKind == GAME_AOC ? 0xFFEAB308 : 0xFFE8B84B; }
    public String saveFileName() { return gameKind == GAME_AOC ? "svdt" : "DATA.BIN"; }
    public String bifrostLookDesc() {
        return gameKind == GAME_AOC ? "Cyan / Gold" : "Jade / Amber";
    }

    public static String tabName(int t) {
        switch (t) {
            case TAB_TACTICAL: return "RADAR";
            case TAB_COMMAND: return "ACTIONS";
            case TAB_GRIMOIRE: return "COMBOS";
            case TAB_LEDGER: return "STATUS";
            default: return "";
        }
    }

    public static String pinPreset(int idx) {
        switch (idx) {
            case 0: return "KEEP";
            case 1: return "BOSS";
            case 2: return "KOROK";
            case 3: return "CHEST";
            case 4: return "RALLY";
            default: return "PIN";
        }
    }

    public static String rDeckTitle(int kind, int slot) {
        if (kind == GAME_AOC) {
            switch (slot) {
                case 0: return "Bombs"; case 1: return "Stasis";
                case 2: return "Cryonis"; default: return "Magnesis";
            }
        }
        switch (slot) {
            case 0: return "Sync"; case 1: return "Art X";
            case 2: return "Art B"; default: return "Art A";
        }
    }

    public static String lDeckTitle(int kind, int slot) {
        if (kind == GAME_AOC) {
            switch (slot) {
                case 0: return "Fire Rod"; case 1: return "Ice Rod";
                case 2: return "Lightning"; default: return "Heal";
            }
        }
        switch (slot) {
            case 0: return "Flame"; case 1: return "Frost";
            case 2: return "Shock"; default: return "Ration";
        }
    }

    public static String deckChord(String shoulder, int slot) {
        switch (slot) {
            case 0: return shoulder + " + Y"; case 1: return shoulder + " + X";
            case 2: return shoulder + " + B"; default: return shoulder + " + A";
        }
    }

    public static String partyDirLabel(int slot) {
        switch (slot) {
            case 0: return "\u2191 UP"; case 1: return "\u2192 RIGHT";
            case 2: return "\u2193 DOWN"; default: return "\u2190 LEFT";
        }
    }

    public static String comboTitle(int idx) {
        return "C" + (idx + 1);
    }

    public static String comboRole(int idx) {
        switch (idx) {
            case 0: return "STRONG"; case 1: return "LAUNCH";
            case 2: return "STUN"; case 3: return "SWEEP";
            case 4: return "VORTEX"; default: return "FINISHER";
        }
    }


    public static boolean tabSlotBounds(int idx, float width, float padX, float gapX, float[] out) {
        if (idx < 0 || idx >= TAB_COUNT || out == null || out.length < 2) return false;
        float usable = width - 2f * padX - (TAB_COUNT - 1) * gapX;
        if (usable <= 0f) return false;
        float slotW = usable / TAB_COUNT;
        float left = padX + idx * (slotW + gapX);
        out[0] = left;
        out[1] = left + slotW;
        return true;
    }

    public static int hitTestTab(float x, float width, float padX, float gapX) {
        float[] lr = new float[2];
        for (int i = 0; i < TAB_COUNT; i++) {
            if (tabSlotBounds(i, width, padX, gapX, lr) && x >= lr[0] && x <= lr[1]) return i;
        }
        return -1;
    }

    private static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
}
