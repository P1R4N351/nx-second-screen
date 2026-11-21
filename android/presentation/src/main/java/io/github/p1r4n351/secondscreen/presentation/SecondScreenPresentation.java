// SPDX-License-Identifier: MIT
package io.github.p1r4n351.secondscreen.presentation;

import android.app.Presentation;
import android.content.Context;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.view.Display;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import io.github.p1r4n351.secondscreen.core.AmoledBurnInGuard;
import io.github.p1r4n351.secondscreen.core.DisplayClassifier;
import java.lang.reflect.Method;

/**
 * Secondary-display Presentation window that preserves Display 0 gamepad focus
 * (FLAG_NOT_FOCUSABLE), keeps the secondary panel awake while visible, and applies
 * bounded AMOLED pixel-orbit translation.
 */
public abstract class SecondScreenPresentation extends Presentation {
    private static final long ORBIT_STEP_MS = 15000L;
    private static final int ORBIT_RADIUS_PX = 3;

    private View contentRoot;
    private long startTimeMs;

    protected SecondScreenPresentation(Context outerContext, Display display) {
        super(outerContext, display);
    }

    /** Subclasses build their View hierarchy here without overriding focus flags. */
    protected abstract View onCreateCompanionView();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Window w = getWindow();
        if (w != null) {
            w.addFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                    | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }
        contentRoot = onCreateCompanionView();
        setContentView(contentRoot);
        startTimeMs = System.currentTimeMillis();
    }

    /** Call from host Activity.onStart() / onStop() so the secondary window tracks lifecycle. */
    public void setHostVisible(boolean started) {
        if (started) {
            if (!isShowing()) show();
        } else if (isShowing()) {
            hide();
        }
    }

    /** Apply bounded pixel-orbit shift and idle dimming to the root container. */
    public void applyBurnInGuard(long nowMs, long idleMs, long dimAfterMs, long offAfterMs) {
        if (contentRoot == null) return;
        int[] off = AmoledBurnInGuard.orbitOffset(nowMs - startTimeMs, ORBIT_STEP_MS, ORBIT_RADIUS_PX);
        contentRoot.setTranslationX((float)off[0]);
        contentRoot.setTranslationY((float)off[1]);
        contentRoot.setAlpha(AmoledBurnInGuard.brightnessScale(idleMs, dimAfterMs, offAfterMs));
    }

    /** Pick the first eligible secondary Display from DisplayManager without brand strings. */
    public static Display selectSecondaryDisplay(DisplayManager dm, boolean externalAllowed) {
        if (dm == null) return null;
        for (Display d : dm.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION)) {
            int dtype = queryDisplayType(d);
            DisplayClassifier.Role role = DisplayClassifier.classify(
                    d.getDisplayId(), dtype, d.getFlags(), externalAllowed);
            if (DisplayClassifier.shouldShow(role)) return d;
        }
        return null;
    }

    private static int queryDisplayType(Display display) {
        try {
            Method m = Display.class.getMethod("getType");
            Object v = m.invoke(display);
            if (v instanceof Integer) return ((Integer)v).intValue();
        } catch (ReflectiveOperationException ignored) {
            // Fail closed as external if hidden Display.getType() is unavailable on the host API level.
        }
        return DisplayClassifier.TYPE_EXTERNAL;
    }
}
