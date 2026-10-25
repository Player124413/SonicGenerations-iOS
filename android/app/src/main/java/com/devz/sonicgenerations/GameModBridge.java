package com.devz.sonicgenerations;

/**
 * Java -> native bridge for the mod manager (the natives live in libmain.so,
 * android/sonicgenerations/src/mod_manager_android.cpp).
 *
 * The MODS button calls {@link #modButton()}; the native side pauses the
 * game and calls back into MainActivity.openModManager(). When the manager
 * closes, MainActivity calls {@link #modManagerClosed(boolean)} so the
 * game resumes (and quits when the player chose "Quit Game").
 *
 * Every method is safe to call at any time from the UI thread: before the
 * native side is attached (or if libmain.so failed to load) they are no-ops.
 */
public final class GameModBridge {

    private GameModBridge() {
    }

    public static void modButton() {
        try {
            nativeModButton();
        } catch (Throwable ignored) {
        }
    }

    public static void modManagerClosed(boolean quit) {
        try {
            nativeModManagerClosed(quit);
        } catch (Throwable ignored) {
        }
    }

    private static native void nativeModButton();

    private static native void nativeModManagerClosed(boolean quit);
}
