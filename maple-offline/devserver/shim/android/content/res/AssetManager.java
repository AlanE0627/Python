package android.content.res;

import java.io.*;
import java.util.Arrays;

// desktop stand-in: assets are files under a root directory
public final class AssetManager {
    private final File root;

    public AssetManager(File root) { this.root = root; }

    public InputStream open(String name) throws IOException {
        return new FileInputStream(new File(root, name));
    }

    public String[] list(String path) throws IOException {
        File dir = path.isEmpty() ? root : new File(root, path);
        String[] names = dir.list();
        if (names == null) return new String[0];
        Arrays.sort(names);
        return names;
    }
}
