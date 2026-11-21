package android.content;

import android.content.res.AssetManager;
import java.io.File;

// desktop stand-in for the app context of the dev server
public class Context {
    private final File assets;
    private final File files;
    private final File databases;

    public Context(File assets, File files, File databases) {
        this.assets = assets; this.files = files; this.databases = databases;
        files.mkdirs();
        databases.mkdirs();
    }

    public AssetManager getAssets() { return new AssetManager(assets); }

    public File getFilesDir() { return files; }

    public File getDatabasePath(String name) { return new File(databases, name); }

    public String getPackageName() { return "com.mapleserver.dev"; }
}
