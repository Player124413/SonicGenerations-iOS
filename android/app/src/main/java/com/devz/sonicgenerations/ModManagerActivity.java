package com.devz.sonicgenerations;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * HedgeModManager mod manager (the Android version of the iOS manager,
 * mod_manager_ios.mm): import, enable, order. Tap a mod to enable or disable
 * it, use the arrows to reorder (the top mod wins conflicts). Mods load when
 * the game starts, so in game a changed selection offers "Quit Game" (applies
 * on the next launch) or "Keep Playing".
 *
 * The mod database itself lives in native code (src/mods/mod_database.*), the
 * same one the game loads from; this activity only renders and edits it.
 */
public class ModManagerActivity extends Activity {

    public static final String EXTRA_ROOT = "root";
    public static final String EXTRA_IN_GAME = "inGame";
    /** The player changed the selection and chose to quit and relaunch. */
    public static final int RESULT_QUIT = 40;

    private static final int REQ_IMPORT = 1;

    private static final class ModRow {
        String title, author, version, issue, note;
        boolean enabled;
    }

    private final List<ModRow> mods = new ArrayList<>();
    private String selectionIssue = "";
    private String initialSnapshot = "";
    private boolean inGame;
    private ModAdapter adapter;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        inGame = getIntent().getBooleanExtra(EXTRA_IN_GAME, false)
                || ModManagerActivityBridgeInGame();
        float dp = getResources().getDisplayMetrics().density;
        int pad = (int) (16 * dp);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(Color.BLACK);
        root.setPadding(pad, pad, pad, pad);

        // --- header: Import folder | Mods — top wins conflicts | Play/Done ---
        LinearLayout header = new LinearLayout(this);
        header.setOrientation(LinearLayout.HORIZONTAL);
        header.setGravity(Gravity.CENTER_VERTICAL);
        Button importBtn = new Button(this);
        importBtn.setText(R.string.mods_import);
        importBtn.setOnClickListener(v -> importFolder());
        header.addView(importBtn, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT));
        TextView title = new TextView(this);
        title.setText(R.string.mods_title);
        title.setTextColor(Color.WHITE);
        title.setTextSize(17);
        title.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams titleLp = new LinearLayout.LayoutParams(
                0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f);
        titleLp.leftMargin = pad / 2;
        titleLp.rightMargin = pad / 2;
        header.addView(title, titleLp);
        Button doneBtn = new Button(this);
        doneBtn.setText(inGame ? R.string.mods_done : R.string.mods_play);
        doneBtn.setOnClickListener(v -> finishFlow());
        header.addView(doneBtn, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT));
        root.addView(header, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));

        // --- the mod list ---
        ListView list = new ListView(this);
        adapter = new ModAdapter();
        list.setAdapter(adapter);
        LinearLayout.LayoutParams listLp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f);
        listLp.topMargin = pad / 2;
        root.addView(list, listLp);

        // --- footer ---
        TextView footer = new TextView(this);
        footer.setText(R.string.mods_footer);
        footer.setTextColor(0xFF999999);
        footer.setTextSize(12);
        footer.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams footerLp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        footerLp.topMargin = pad / 2;
        root.addView(footer, footerLp);

        setContentView(root);
        scan();
    }

    private boolean ModManagerActivityBridgeInGame() {
        try {
            return nativeInGame();
        } catch (Throwable t) {
            return false;
        }
    }

    private void scan() {
        String json;
        try {
            json = nativeScan();
        } catch (Throwable t) {
            Toast.makeText(this, R.string.mods_scan_failed, Toast.LENGTH_LONG).show();
            finish();
            return;
        }
        mods.clear();
        try {
            JSONObject root = new JSONObject(json);
            JSONArray arr = root.optJSONArray("mods");
            for (int i = 0; arr != null && i < arr.length(); i++) {
                JSONObject o = arr.getJSONObject(i);
                ModRow row = new ModRow();
                row.title = o.optString("title", "Untitled");
                row.author = o.optString("author", "");
                row.version = o.optString("version", "");
                row.issue = o.optString("issue", "");
                row.note = o.optString("note", "");
                row.enabled = o.optBoolean("enabled", false);
                mods.add(row);
            }
            selectionIssue = root.optString("issue", "");
        } catch (Exception ignored) {
        }
        initialSnapshot = snapshot();
        adapter.notifyDataSetChanged();
    }

    private String snapshot() {
        StringBuilder sb = new StringBuilder();
        for (ModRow r : mods) sb.append(r.title).append('=').append(r.enabled).append(';');
        return sb.toString();
    }

    private void importFolder() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        startActivityForResult(i, REQ_IMPORT);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQ_IMPORT && resultCode == RESULT_OK && data != null && data.getData() != null) {
            Uri tree = data.getData();
            try {
                getContentResolver().takePersistableUriPermission(tree,
                        Intent.FLAG_GRANT_READ_URI_PERMISSION);
            } catch (Exception ignored) {
            }
            String path = SgUriPath.resolve(this, tree);
            if (path == null) {
                Toast.makeText(this, R.string.mods_import_error, Toast.LENGTH_LONG).show();
                return;
            }
            String error;
            try {
                error = nativeImport(path);
            } catch (Throwable t) {
                error = getString(R.string.mods_import_error);
            }
            if (error != null && !error.isEmpty()) {
                Toast.makeText(this, error, Toast.LENGTH_LONG).show();
            }
            scan();
        }
    }

    /** Done / Play: save, then offer quit when the in-game selection changed. */
    private void finishFlow() {
        String issue;
        try {
            issue = nativeSave();
        } catch (Throwable t) {
            issue = "";
        }
        selectionIssue = issue == null ? "" : issue;
        boolean changed = !snapshot().equals(initialSnapshot);
        if (inGame && changed) {
            new AlertDialog.Builder(this)
                    .setTitle(R.string.mods_changed_title)
                    .setMessage(R.string.mods_changed_message)
                    .setNegativeButton(R.string.mods_keep, (d, w) -> {
                        setResult(RESULT_OK);
                        finish();
                    })
                    .setPositiveButton(R.string.mods_quit, (d, w) -> {
                        setResult(RESULT_QUIT);
                        finish();
                    })
                    .show();
            return;
        }
        if (!selectionIssue.isEmpty() && !isFinishing()) {
            Toast.makeText(this, selectionIssue, Toast.LENGTH_LONG).show();
        }
        setResult(RESULT_OK);
        finish();
    }

    // --- rows ---------------------------------------------------------------

    private final class ModAdapter extends BaseAdapter {
        @Override public int getCount() { return mods.size(); }
        @Override public Object getItem(int position) { return mods.get(position); }
        @Override public long getItemId(int position) { return position; }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            float dp = getResources().getDisplayMetrics().density;
            ModRow row = mods.get(position);

            LinearLayout layout = new LinearLayout(ModManagerActivity.this);
            layout.setOrientation(LinearLayout.HORIZONTAL);
            layout.setGravity(Gravity.CENTER_VERTICAL);
            layout.setPadding((int) (8 * dp), (int) (10 * dp), (int) (8 * dp), (int) (10 * dp));

            LinearLayout text = new LinearLayout(ModManagerActivity.this);
            text.setOrientation(LinearLayout.VERTICAL);
            text.setPadding(0, 0, (int) (8 * dp), 0);

            TextView name = new TextView(ModManagerActivity.this);
            String byline = row.version.isEmpty() ? row.author
                    : (row.author.isEmpty() ? row.version : row.author + " · " + row.version);
            name.setText(byline.isEmpty() ? row.title : row.title + "\n" + byline);
            name.setTextColor(row.issue.isEmpty() ? Color.WHITE : 0xFF888888);
            name.setTextSize(15);
            if (!row.issue.isEmpty() || !row.note.isEmpty()) {
                TextView warn = new TextView(ModManagerActivity.this);
                warn.setText(row.issue.isEmpty() ? row.note : row.issue);
                warn.setTextColor(row.issue.isEmpty() ? 0xFFCCAA66 : 0xFFCC6666);
                warn.setTextSize(12);
                text.addView(name);
                text.addView(warn);
            } else {
                text.addView(name);
            }
            LinearLayout.LayoutParams textLp = new LinearLayout.LayoutParams(
                    0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f);
            layout.addView(text, textLp);

            // Enabled lamp + tap anywhere on the row toggles.
            TextView lamp = new TextView(ModManagerActivity.this);
            lamp.setText(row.enabled ? "✓" : "○");
            lamp.setTextColor(row.enabled ? 0xFF66CC66 : 0xFF666666);
            lamp.setTextSize(22);
            lamp.setTypeface(Typeface.DEFAULT_BOLD);
            lamp.setGravity(Gravity.CENTER);
            lamp.setPadding((int) (6 * dp), 0, (int) (6 * dp), 0);
            layout.addView(lamp);

            if (!row.issue.isEmpty()) {
                // Cannot be enabled: lock the lamp.
                lamp.setAlpha(0.35f);
            }

            // Reorder arrows (top wins conflicts).
            TextView up = arrow("↑", dp);
            TextView down = arrow("↓", dp);
            up.setOnClickListener(v -> move(position, position - 1));
            down.setOnClickListener(v -> move(position, position + 1));
            layout.addView(up);
            layout.addView(down);

            layout.setOnClickListener(v -> {
                if (row.issue.isEmpty()) {
                    row.enabled = !row.enabled;
                    try { nativeSetEnabled(position, row.enabled); } catch (Throwable ignored) {}
                    notifyDataSetChanged();
                }
            });
            return layout;
        }

        private TextView arrow(String label, float dp) {
            TextView v = new TextView(ModManagerActivity.this);
            v.setText(label);
            v.setTextColor(Color.WHITE);
            v.setTextSize(18);
            v.setGravity(Gravity.CENTER);
            v.setPadding((int) (6 * dp), (int) (8 * dp), (int) (6 * dp), (int) (8 * dp));
            return v;
        }

        private void move(int from, int to) {
            if (to < 0 || to >= mods.size()) return;
            try { nativeMove(from, to); } catch (Throwable ignored) {}
            ModRow row = mods.remove(from);
            mods.add(to, row);
            notifyDataSetChanged();
        }
    }

    // --- native (mod_manager_android.cpp) ----------------------------------

    private static native String nativeScan();

    private static native String nativeImport(String path);

    private static native boolean nativeSetEnabled(int index, boolean on);

    private static native boolean nativeMove(int from, int to);

    private static native String nativeSave();

    private static native boolean nativeInGame();
}
