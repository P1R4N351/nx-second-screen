// SPDX-License-Identifier: MIT
package io.github.p1r4n351.secondscreen.core;

/**
 * Pure-JDK 2D radar projection, elevation-band classification, and distance
 * formatting mirroring {@code companion_trail.h}. Zero Android imports.
 */
public final class RadarMath {
    public static final int BAND_BELOW = -1;
    public static final int BAND_LEVEL = 0;
    public static final int BAND_ABOVE = 1;

    private RadarMath() {}

    /** Classify vertical delta against {@code bandM} for multi-floor dungeons. */
    public static int floorBand(float refY, float ptY, float bandM) {
        float b = bandM > 0.0f ? bandM : 3.0f;
        float dy = ptY - refY;
        if (dy > b) return BAND_ABOVE;
        if (dy < -b) return BAND_BELOW;
        return BAND_LEVEL;
    }

    /** Horizontal XZ distance in world units (meters). */
    public static float horizontalDistance(float refX, float refZ, float ptX, float ptZ) {
        return (float) Math.hypot(ptX - refX, ptZ - refZ);
    }

    /**
     * Project world (ptX, ptZ) into normalized radar coordinates in [-1, 1]^2.
     * Writes out[0]=nx, out[1]=ny, out[2]=clamped (0 inside disc, 1 on rim).
     */
    public static boolean projectXZ(
            float refX, float refZ, float yawDeg, boolean headingUp, float radiusM,
            float ptX, float ptZ, float[] out) {
        if (out == null || out.length < 3 || !Float.isFinite(radiusM) || radiusM <= 1e-3f) {
            return false;
        }
        if (!Float.isFinite(refX) || !Float.isFinite(refZ)
                || !Float.isFinite(ptX) || !Float.isFinite(ptZ)) {
            return false;
        }
        float dx = ptX - refX;
        float dz = ptZ - refZ;
        float rx = dx;
        float rz = dz;
        if (headingUp) {
            double rad = Math.toRadians(Float.isFinite(yawDeg) ? yawDeg : 0.0f);
            float c = (float) Math.cos(rad);
            float s = (float) Math.sin(rad);
            rx = dx * c + dz * s;
            rz = -dx * s + dz * c;
        }
        float nx = rx / radiusM;
        float ny = rz / radiusM;
        float mag = (float) Math.hypot(nx, ny);
        if (mag > 1.0f) {
            out[0] = nx / mag;
            out[1] = ny / mag;
            out[2] = 1.0f;
        } else {
            out[0] = nx;
            out[1] = ny;
            out[2] = 0.0f;
        }
        return true;
    }

    /** Compact pin distance and floor indicator, e.g. {@code "14m ▲"} or {@code "3m ●"}. */
    public static String formatPinDelta(float refX, float refY, float refZ,
                                        float ptX, float ptY, float ptZ, float bandM) {
        int dist = Math.round(horizontalDistance(refX, refZ, ptX, ptZ));
        int band = floorBand(refY, ptY, bandM);
        String mark = band > 0 ? "▲" : (band < 0 ? "▼" : "●");
        return dist + "m " + mark;
    }
}
