package com.devz.sonicgenerations;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Turns a Storage Access Framework tree into something the native mod loader
 * can read with ordinary file APIs: a real path when the provider exposes one
 * (the common case - SD cards and primary storage), otherwise a staged copy in
 * the cache directory (cloud providers, SAF-only document sources).
 */
public final class SgUriPath {

    private SgUriPath() {
    }

    /** Real filesystem path for the picked tree, or null when not path-backed. */
    public static String resolve(Context ctx, Uri tree) {
        try {
            if (!DocumentsContract.isTreeUri(tree)) return null;
            String docId = DocumentsContract.getTreeDocumentId(tree);
            if (docId == null) return null;
            if (docId.startsWith("primary:")) {
                File base = android.os.Environment.getExternalStorageDirectory();
                String rest = docId.substring("primary:".length());
                File f = rest.isEmpty() ? base : new File(base, rest);
                return f.exists() ? f.getAbsolutePath() : null;
            }
            if (docId.startsWith("raw:")) {
                File f = new File(docId.substring("raw:".length()));
                return f.exists() ? f.getAbsolutePath() : null;
            }
            // Some file managers hand out absolute paths as document IDs.
            if (docId.startsWith("/")) {
                File f = new File(docId);
                return f.exists() ? f.getAbsolutePath() : null;
            }
        } catch (Exception ignored) {
        }
        return null;
    }

    /**
     * Copies the picked tree into <cache>/mod_import_<ts>/<top folder name> so
     * the native loader can read it as a plain directory. Returns the staged
     * folder holding mod.ini (the mod folder itself), or null on failure.
     */
    public static String stageTree(Context ctx, Uri tree) {
        ContentResolver cr = ctx.getContentResolver();
        File stageRoot = new File(ctx.getCacheDir(), "mod_import_" + System.currentTimeMillis());
        try {
            String docId = DocumentsContract.getTreeDocumentId(tree);
            String name = docId;
            int slash = name.lastIndexOf('/');
            if (slash >= 0 && slash + 1 < name.length()) name = name.substring(slash + 1);
            if (name.isEmpty()) name = "mod";
            File dest = new File(stageRoot, name.replaceAll("[^A-Za-z0-9._-]+", "_"));
            long[] counter = {0};
            copyTree(cr, DocumentsContract.buildDocumentUriUsingTree(tree, docId), dest, counter, 0);
            File mod = findModIni(dest, 0);
            return mod != null ? mod.getAbsolutePath() : null;
        } catch (Exception e) {
            deleteRecursively(stageRoot);
            return null;
        }
    }

    private static void copyTree(ContentResolver cr, Uri doc, File dest, long[] counter, int depth)
            throws IOException {
        if (depth > 12 || counter[0] > 20000) return;
        String mime = query(cr, doc, DocumentsContract.Document.COLUMN_MIME_TYPE);
        if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
            if (!dest.mkdirs() && !dest.isDirectory()) throw new IOException("mkdir " + dest);
            Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(
                    doc, DocumentsContract.getDocumentId(doc));
            try (Cursor c = cr.query(children,
                    new String[]{DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                            DocumentsContract.Document.COLUMN_MIME_TYPE},
                    null, null, null)) {
                while (c != null && c.moveToNext()) {
                    String id = c.getString(0);
                    String name = c.getString(1);
                    if (name == null || name.isEmpty()) continue;
                    Uri child = DocumentsContract.buildDocumentUriUsingTree(doc, id);
                    copyTree(cr, child, new File(dest, name), counter, depth + 1);
                }
            }
        } else {
            File parent = dest.getParentFile();
            if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
                throw new IOException("mkdir " + parent);
            }
            try (InputStream in = cr.openInputStream(doc);
                 OutputStream out = new FileOutputStream(dest)) {
                if (in == null) throw new IOException("open " + doc);
                byte[] buf = new byte[1 << 16];
                int n;
                while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            }
            counter[0]++;
        }
    }

    private static String query(ContentResolver cr, Uri doc, String column) {
        try (Cursor c = cr.query(doc, new String[]{column}, null, null, null)) {
            return (c != null && c.moveToFirst()) ? c.getString(0) : null;
        } catch (Exception e) {
            return null;
        }
    }

    /** The folder holding mod.ini (mod archives wrap the mod in extra folders). */
    private static File findModIni(File dir, int depth) {
        if (depth > 4) return null;
        File[] kids = dir.listFiles();
        if (kids == null) return null;
        File nested = null;
        for (File f : kids) {
            if (f.isFile() && f.getName().equalsIgnoreCase("mod.ini")) return dir;
            if (f.isDirectory()) nested = f;
        }
        return nested != null ? findModIni(nested, depth + 1) : null;
    }

    private static void deleteRecursively(File f) {
        File[] kids = f.listFiles();
        if (kids != null) for (File k : kids) deleteRecursively(k);
        //noinspection ResultOfMethodCallIgnored
        f.delete();
    }
}
