package devmain;

import android.content.Context;
import java.io.File;
import java.nio.file.*;

// Runs the phone build's server on a PC, with the app's own assets.
public final class DevMain {
    public static void main(String[] args) throws Exception {
        File assets = new File(System.getProperty("assets", "/home/claude/repos/maple-offline-app/app/src/main/assets"));
        File base = new File(System.getProperty("devdir", "/home/claude/devsrv"));
        Context ctx = new Context(assets, new File(base, "files"), new File(base, "db"));
        Files.copy(new File(assets, "config.yaml").toPath(), new File(ctx.getFilesDir(), "config.yaml").toPath(),
                   StandardCopyOption.REPLACE_EXISTING);
        Explorer.install();
        net.server.Server.getInstance(ctx);
        net.server.Server.main(new String[0]);
        System.err.println("[dev] server main returned");
        Thread.currentThread().join();
    }
}
