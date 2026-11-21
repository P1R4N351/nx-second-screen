// SPDX-License-Identifier: MIT
package io.github.p1r4n351.secondscreen.core;

/** Classifies Android Display descriptors without relying on device brand names. */
public final class DisplayClassifier {
    public static final int DEFAULT_DISPLAY = 0;
    public static final int TYPE_INTERNAL = 1;
    public static final int TYPE_EXTERNAL = 2;
    public static final int FLAG_PRIVATE = 1 << 2;

    public enum Role {
        NONE,
        BUILT_IN_SECONDARY,
        EXTERNAL_SECONDARY
    }

    private DisplayClassifier() {}

    /** Return whether displayId/type/flags should host a secondary companion surface. */
    public static Role classify(int displayId, int displayType, int flags, boolean externalAllowed) {
        if (displayId == DEFAULT_DISPLAY) return Role.NONE;
        if ((flags & FLAG_PRIVATE) != 0) return Role.NONE;
        if (displayType == TYPE_INTERNAL) return Role.BUILT_IN_SECONDARY;
        return externalAllowed ? Role.EXTERNAL_SECONDARY : Role.NONE;
    }

    /** True when the classified role warrants attaching a Presentation window. */
    public static boolean shouldShow(Role role) {
        return role != Role.NONE;
    }
}
