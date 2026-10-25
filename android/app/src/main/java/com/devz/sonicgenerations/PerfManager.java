package com.devz.sonicgenerations;

import android.app.Activity;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;

/**
 * Android performance glue on the Java side:
 *
 *  * Game Mode (API 33+): request PERFORMANCE so the OS offers the game its
 *    sustained performance profile (thermals, GPU clocks).
 *  * Thermal listener (API 29+): the OS reports throttling as it happens;
 *    native (android_perf.cpp) turns it into a live framerate_limit drop
 *    (45 FPS warm, 30 FPS hot) and lifts it when the device cools down -
 *    a predictable lower rate beats a throttled slideshow mid-level.
 *  * Sustained performance mode (API 24+, set by MainActivity) and
 *    FLAG_KEEP_SCREEN_ON round it out.
 */
public final class PerfManager {

    private static final Handler MAIN = new Handler(Looper.getMainLooper());
    private static PowerManager sPower;
    private static PowerManager.OnThermalStatusChangedListener sThermal;
    private static android.app.GameManager sGameManager;
    private static int sLastStatus = PowerManager.THERMAL_STATUS_NONE;

    private PerfManager() {
    }

    public static void install(Activity activity) {
        if (Build.VERSION.SDK_INT < 29) return;
        MAIN.post(() -> {
            try {
                sPower = (PowerManager) activity.getSystemService(Activity.POWER_SERVICE);
                if (sPower == null) return;
                sThermal = status -> {
                    if (status != sLastStatus) {
                        sLastStatus = status;
                        forwardThermal(status);
                    }
                };
                sPower.addThermalStatusListener(sThermal);
                forwardThermal(sPower.getCurrentThermalStatus());
            } catch (Throwable ignored) {
            }
        });
        if (Build.VERSION.SDK_INT >= 33) {
            try {
                sGameManager = (android.app.GameManager) activity.getSystemService(Activity.GAME_SERVICE);
                if (sGameManager != null) {
                    sGameManager.setGameMode(activity.getPackageName(),
                            android.app.GameManager.GAME_MODE_PERFORMANCE);
                }
            } catch (Throwable ignored) {
            }
        }
    }

    private static void forwardThermal(int status) {
        try {
            nativeSetThermalStatus(status);
        } catch (Throwable ignored) {
        }
    }

    public static void onActivityPaused() {
        // Nothing: the thermal listener spans the process lifetime.
    }

    public static void onActivityResumed(Activity activity) {
        if (sGameManager != null) {
            try {
                sGameManager.setGameMode(activity.getPackageName(),
                        android.app.GameManager.GAME_MODE_PERFORMANCE);
            } catch (Throwable ignored) {
            }
        }
    }

    public static void onActivityDestroyed() {
        if (sPower != null && sThermal != null && Build.VERSION.SDK_INT >= 29) {
            try {
                sPower.removeThermalStatusListener(sThermal);
            } catch (Throwable ignored) {
            }
        }
        sThermal = null;
        sPower = null;
        sGameManager = null;
    }

    private static native void nativeSetThermalStatus(int status);
}
