package com.devz.sonicgenerations;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;

import com.devz.sonicgenerations.gamepad.PadSettings;
import com.devz.sonicgenerations.gamepad.VirtualPadView;

import org.libsdl.app.SDLActivity;

/** SDL3 activity hosting the recompiled game (SDL is linked statically into libmain.so). */
public class MainActivity extends SDLActivity {
    private static final int REQ_MODS = 4001;

    private VirtualPadView mGamepad;
    private android.widget.TextView mModsButton;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        if (!GameFiles.hasValidGameRoot(this)) {
            startActivity(new Intent(this, SetupActivity.class));
            finish();
            return;
        }
        super.onCreate(savedInstanceState);
        applyOrientation();
        // Ask the OS for the sustained (thermally stable) clock profile instead of
        // the burst-then-throttle default, and never dim while playing.
        android.view.Window w = getWindow();
        w.addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if (android.os.Build.VERSION.SDK_INT >= 24) w.setSustainedPerformanceMode(true);
        if (android.os.Build.VERSION.SDK_INT >= 31) {
            android.view.WindowManager.LayoutParams lp = w.getAttributes();
            lp.preferredRefreshRate = 0;
            w.setAttributes(lp);
        }
        // Game Mode (performance) + thermal listener -> adaptive frame cap.
        PerfManager.install(this);
        if (PadSettings.get(this).enabled()) {
            mGamepad = VirtualPadView.install(this);
        }
        installModsButton();
    }

    /**
     * The MODS pill, top left, beside the on-screen controls' EDIT - the same
     * place the iOS port puts it. Opens the HedgeModManager mod manager over
     * the game (the game pauses; a changed selection offers Quit Game).
     */
    private void installModsButton() {
        mModsButton = new android.widget.TextView(this);
        mModsButton.setText(R.string.mods_button);
        mModsButton.setTextSize(13);
        mModsButton.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
        mModsButton.setTextColor(0xFFFFFFFF);
        mModsButton.setAlpha(0.85f);
        float dp = getResources().getDisplayMetrics().density;
        int pad = (int) (8 * dp);
        mModsButton.setPadding(pad * 2, pad, pad * 2, pad);
        mModsButton.setBackground(new android.graphics.drawable.GradientDrawable() {{
            setColor(0x66000000);
            setCornerRadius(999);
        }});
        mModsButton.setOnClickListener(v -> GameModBridge.modButton());
        android.widget.FrameLayout.LayoutParams lp = new android.widget.FrameLayout.LayoutParams(
                android.widget.FrameLayout.LayoutParams.WRAP_CONTENT,
                android.widget.FrameLayout.LayoutParams.WRAP_CONTENT);
        lp.topMargin = (int) (10 * dp);
        lp.leftMargin = (int) (10 * dp);
        addContentView(mModsButton, lp);
    }

    /** Called from native (SgRunModManager) when the MODS button runs. */
    @SuppressWarnings("unused")
    public void openModManager(String root, boolean inGame) {
        Intent i = new Intent(this, ModManagerActivity.class);
        i.putExtra(ModManagerActivity.EXTRA_ROOT, root);
        i.putExtra(ModManagerActivity.EXTRA_IN_GAME, inGame);
        startActivityForResult(i, REQ_MODS);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQ_MODS) {
            // RESULT_QUIT = the player chose "Quit Game" after changing the
            // selection: shut the runtime down cleanly (SDL_EVENT_QUIT) and
            // leave; the new selection is in on the next launch.
            boolean quit = resultCode == ModManagerActivity.RESULT_QUIT;
            GameModBridge.modManagerClosed(quit);
            if (quit) {
                finishAffinity();
            }
        }
    }

    /** Orientation chosen in the launcher (Graphics dialog); landscape by default. */
    private void applyOrientation() {
        String o = new GraphicsSettings(this).orientation();
        int req;
        switch (o) {
            case "portrait": req = android.content.pm.ActivityInfo.SCREEN_ORIENTATION_PORTRAIT; break;
            // "auto": the two landscape sides only - the game never flips into
            // portrait under the player's hands.
            case "auto": req = android.content.pm.ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE; break;
            // Fixed landscape (the default): no rotation at all while playing.
            default: req = android.content.pm.ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
        }
        setRequestedOrientation(req);
    }

    @Override
    protected void onPause() {
        if (mGamepad != null) mGamepad.onHostPause();
        PerfManager.onActivityPaused();
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        PerfManager.onActivityResumed(this);
    }

    @Override
    protected void onDestroy() {
        PerfManager.onActivityDestroyed();
        super.onDestroy();
    }

    @Override
    protected String[] getLibraries() {
        return new String[]{"main"};
    }

    public static ParcelFileDescriptor openContentFd(String uri, String mode) {
        SDLActivity self = mSingleton;
        if (self == null || uri == null) return null;
        try {
            return self.getContentResolver().openFileDescriptor(Uri.parse(uri), mode == null ? "r" : mode);
        } catch (Exception e) {
            return null;
        }
    }
}
