package devmain;

import client.Character;
import client.Client;
import net.packet.Packet;
import net.server.Server;
import scripting.npc.NPCScriptManager;
import scripting.quest.QuestScriptManager;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;
import java.util.regex.*;

/**
 * Walks every branch of an NPC or quest script on the dev server, as the
 * logged-in character: starts the conversation, answers each dialog with
 * every possible answer (next / yes / no / accept / decline / each #L
 * option), and replays the answer path from the start for each branch.
 *
 * Commands, one per line in /tmp/devcmd (the file is consumed):
 *   npc <id>
 *   qstart <quest> <npc>
 *   qend <quest> <npc>
 * Output: /tmp/explore/<kind>_<id>.txt, one line per dialog reached:
 *   path | msgtype style | text   and END / STALL / ERROR lines.
 */
public final class Explorer implements Runnable {
    private static final int NPC_TALK = 0x130;
    private static final int MAX_PATHS = 400;
    private static final int MAX_DEPTH = 30;

    private final List<byte[]> talks = Collections.synchronizedList(new ArrayList<>());
    private static final PrintStream realErr = System.err;
    private static final ByteArrayOutputStream errBuf = new ByteArrayOutputStream();

    public static void install() {
        System.setErr(new PrintStream(new OutputStream() {
            @Override
            public void write(int b) {
                realErr.write(b);
                synchronized (errBuf) { errBuf.write(b); }
            }

            @Override
            public void write(byte[] b, int off, int len) {
                realErr.write(b, off, len);
                synchronized (errBuf) { errBuf.write(b, off, len); }
            }
        }, true));
        Thread t = new Thread(new Explorer(), "explorer");
        t.setDaemon(true);
        t.start();
    }

    @Override
    public void run() {
        Path cmd = Paths.get("/tmp/devcmd");
        while (true) {
            try {
                Thread.sleep(500);
                if (!Files.exists(cmd)) continue;
                List<String> lines = Files.readAllLines(cmd);
                Files.delete(cmd);
                for (String line : lines) {
                    line = line.trim();
                    if (line.isEmpty()) continue;
                    try {
                        handle(line.split("\\s+"));
                    } catch (Throwable t) {
                        t.printStackTrace();
                    }
                }
                Files.writeString(Paths.get("/tmp/explore/done"), String.valueOf(System.currentTimeMillis()));
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    private Character player() {
        for (Character c : Server.getInstance().getWorld(0).getPlayerStorage().getAllCharacters()) {
            return c;
        }
        return null;
    }

    /** One dialog as the client would see it. */
    static final class Dialog {
        int msgtype;
        int b0;
        int b1;
        String text;
        List<Integer> options = new ArrayList<>();
    }

    private static Dialog parse(byte[] b) {
        java.nio.ByteBuffer bb = java.nio.ByteBuffer.wrap(b).order(java.nio.ByteOrder.LITTLE_ENDIAN);
        bb.getShort();
        bb.get();
        bb.getInt();
        Dialog d = new Dialog();
        d.msgtype = bb.get() & 0xFF;
        bb.get();
        if (d.msgtype == 14) bb.getInt();
        int len = bb.getShort() & 0xFFFF;
        byte[] s = new byte[len];
        bb.get(s);
        d.text = new String(s, StandardCharsets.UTF_8);
        if (d.msgtype == 0 && bb.remaining() >= 2) {
            d.b0 = bb.get() & 0xFF;
            d.b1 = bb.get() & 0xFF;
        }
        Matcher m = Pattern.compile("#L(\\d+)#").matcher(d.text);
        while (m.find()) d.options.add(Integer.parseInt(m.group(1)));
        return d;
    }

    /** Answers worth following from a dialog: {mode, type, selection}. */
    private static List<int[]> answers(Dialog d) {
        List<int[]> a = new ArrayList<>();
        switch (d.msgtype) {
            case 0 -> a.add(new int[]{1, 0, -1});                     // next / ok
            case 1 -> { a.add(new int[]{1, 1, -1}); a.add(new int[]{0, 1, -1}); }  // yes, no
            case 12 -> { a.add(new int[]{1, 12, -1}); a.add(new int[]{0, 12, -1}); }
            case 4, 14 -> { for (int o : d.options) a.add(new int[]{1, d.msgtype, o}); }
            case 3 -> a.add(new int[]{1, 3, 1});
            case 2 -> a.add(new int[]{1, 2, -1});
            case 7 -> a.add(new int[]{1, 7, 0});
            default -> a.add(new int[]{1, d.msgtype, -1});
        }
        return a;
    }

    private void handle(String[] arg) throws Exception {
        Character chr = player();
        if (chr == null) { realErr.println("[explore] nobody logged in"); return; }
        Client c = chr.getClient();
        String kind = arg[0];
        int id = Integer.parseInt(arg[1]);
        int npc = arg.length > 2 ? Integer.parseInt(arg[2]) : id;
        Files.createDirectories(Paths.get("/tmp/explore"));
        PrintWriter out = new PrintWriter(new OutputStreamWriter(new FileOutputStream("/tmp/explore/" + kind + "_" + id + ".txt"), StandardCharsets.UTF_8));

        Client.packetTap = (cl, p) -> {
            byte[] b = p.getBytes();
            if (cl == c && b.length > 2 && ((b[0] & 0xFF) | (b[1] & 0xFF) << 8) == NPC_TALK) talks.add(b);
        };

        Deque<List<int[]>> todo = new ArrayDeque<>();
        todo.add(new ArrayList<>());
        Set<String> seen = new HashSet<>();
        int paths = 0;
        int ends = 0, stalls = 0, errors = 0;

        try {
            while (!todo.isEmpty() && paths < MAX_PATHS) {
                List<int[]> path = todo.poll();
                paths++;
                reset(c);
                talks.clear();
                int errMark;
                synchronized (errBuf) { errMark = errBuf.size(); }

                begin(kind, c, id, npc, chr);
                Thread.sleep(30);
                String label = "";
                Dialog last = null;
                boolean ok = true;
                for (int[] step : path) {
                    if (talks.isEmpty()) { ok = false; break; }
                    last = parse(talks.get(talks.size() - 1));
                    talks.clear();
                    act(kind, c, step);
                    Thread.sleep(20);
                    label += "/" + step[0] + (step[2] >= 0 ? ":" + step[2] : "");
                }
                String err;
                synchronized (errBuf) {
                    err = new String(errBuf.toByteArray(), errMark, errBuf.size() - errMark, StandardCharsets.UTF_8);
                }
                boolean failed = err.contains("Exception") || err.contains("Error ");
                if (!ok) continue;
                if (talks.isEmpty()) {
                    boolean open = kind.equals("npc") ? NPCScriptManager.getInstance().getCM(c) != null
                                                      : QuestScriptManager.getInstance().getQM(c) != null;
                    if (failed) { errors++; out.println("ERROR " + label + " | " + firstLine(err)); }
                    else if (open) { stalls++; out.println("STALL " + label); }
                    else { ends++; out.println("END   " + label); }
                    continue;
                }
                Dialog d = parse(talks.get(talks.size() - 1));
                String key = d.msgtype + "|" + d.text;
                out.println("TALK  " + label + " | " + d.msgtype + (d.msgtype == 0 ? " " + d.b0 + d.b1 : "") + " | " + d.text.replace("\r", "").replace("\n", "\\n"));
                if (failed) { errors++; out.println("ERROR " + label + " | " + firstLine(err)); }
                if (path.size() >= MAX_DEPTH) continue;
                // a dialog seen before on another path: its branches are already queued
                if (!seen.add(key + "@" + path.size())) {
                    if (d.msgtype == 0 && d.b0 == 0 && d.b1 == 0) {
                        // still follow OK so the end of each path is checked
                    } else if (seen.contains(key)) {
                        continue;
                    }
                }
                seen.add(key);
                for (int[] a : answers(d)) {
                    List<int[]> next = new ArrayList<>(path);
                    next.add(a);
                    todo.add(next);
                }
            }
        } finally {
            reset(c);
            Client.packetTap = null;
            out.println("SUMMARY paths " + paths + " end " + ends + " stall " + stalls + " error " + errors
                        + (todo.isEmpty() ? "" : " (cut, " + todo.size() + " left)"));
            out.close();
        }
    }

    private static String firstLine(String s) {
        for (String l : s.split("\n")) if (l.contains("Exception") || l.contains("Error")) return l.trim();
        return s.trim();
    }

    private static void reset(Client c) {
        NPCScriptManager.getInstance().dispose(c);
        QuestScriptManager.getInstance().dispose(c);
    }

    private static void begin(String kind, Client c, int id, int npc, Character chr) {
        try {
            java.lang.reflect.Field f = Client.class.getDeclaredField("lastNpcClick");
            f.setAccessible(true);
            f.setLong(c, 0L);
        } catch (ReflectiveOperationException e) {
            throw new IllegalStateException(e);
        }
        switch (kind) {
            case "npc" -> NPCScriptManager.getInstance().start(c, id, chr);
            case "qstart" -> QuestScriptManager.getInstance().start(c, (short) id, npc);
            case "qend" -> QuestScriptManager.getInstance().end(c, (short) id, npc);
        }
    }

    private static void act(String kind, Client c, int[] s) {
        switch (kind) {
            case "npc" -> NPCScriptManager.getInstance().action(c, (byte) s[0], (byte) s[1], s[2]);
            case "qstart" -> QuestScriptManager.getInstance().start(c, (byte) s[0], (byte) s[1], s[2]);
            case "qend" -> QuestScriptManager.getInstance().end(c, (byte) s[0], (byte) s[1], s[2]);
        }
    }
}
