// SPDX-License-Identifier: MIT
package io.github.p1r4n351.secondscreen.core;

/** Decodes native CompanionState long[] word arrays across v1 (8 words) and v2 (13 words). */
public final class CompanionStateWords {
    public static final int STATUS_NONE = 0;
    public static final int STATUS_STARTING = 1;
    public static final int STATUS_UNSUPPORTED = 2;
    public static final int STATUS_TITLE = 3;
    public static final int STATUS_LOADING = 4;
    public static final int STATUS_READY = 5;

    public static final int GATE_NONE = 0;
    public static final int GATE_MENU = 1;
    public static final int GATE_EVENT = 2;
    public static final int GATE_LOADING = 3;
    public static final int GATE_SAVING = 4;
    public static final int GATE_STALE = 5;
    public static final int GATE_UNSUPPORTED = 6;

    public static final int V1_FIELDS = 8;
    public static final int V2_FIELDS = 13;

    public final int status;
    public final boolean inGame;
    public final boolean menuOpen;
    public final boolean tabletOwned;
    public final int life;
    public final int maxLife;
    public final int page;
    public final long ageMs;
    public final boolean dim;
    public final int stamina;
    public final int maxStamina;
    public final boolean staminaWheel;
    public final int gateReason;

    private CompanionStateWords(long[] w) {
        if (w == null || w.length < V1_FIELDS) {
            this.status = STATUS_NONE;
            this.inGame = false;
            this.menuOpen = false;
            this.tabletOwned = true;
            this.life = -1;
            this.maxLife = -1;
            this.page = -1;
            this.ageMs = -1L;
            this.dim = false;
            this.stamina = -1;
            this.maxStamina = -1;
            this.staminaWheel = false;
            this.gateReason = GATE_NONE;
            return;
        }
        this.status = (int)w[0];
        this.inGame = w[1] != 0L;
        this.menuOpen = w[2] != 0L;
        this.tabletOwned = w[3] != 0L;
        this.life = (int)w[4];
        this.maxLife = (int)w[5];
        this.page = (int)w[6];
        this.ageMs = w[7];
        boolean hasV2 = w.length >= V2_FIELDS;
        this.dim = hasV2 && w[8] != 0L;
        this.stamina = hasV2 ? (int)w[9] : -1;
        this.maxStamina = hasV2 ? (int)w[10] : -1;
        this.staminaWheel = hasV2 && w[11] != 0L;
        this.gateReason = hasV2 ? (int)w[12] : GATE_NONE;
    }

    /** Decode a v1 (8-word) or v2 (13-word) JNI state array. */
    public static CompanionStateWords decode(long[] words) {
        return new CompanionStateWords(words);
    }

    /** True only when the game is running and no write-gate is active. */
    public boolean actionsAllowed() {
        return status == STATUS_READY && inGame && gateReason == GATE_NONE;
    }

    /** Human-readable label for CompanionGateReason values. */
    public static String gateLabel(int reason) {
        switch (reason) {
            case GATE_NONE: return "none";
            case GATE_MENU: return "menu";
            case GATE_EVENT: return "event";
            case GATE_LOADING: return "loading";
            case GATE_SAVING: return "saving";
            case GATE_STALE: return "stale";
            case GATE_UNSUPPORTED: return "unsupported";
            default: return "unknown";
        }
    }
}
