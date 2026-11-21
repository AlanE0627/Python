package android.database.sqlite;

import android.content.ContentValues;
import android.database.Cursor;
import android.database.MemoryCursor;

import java.sql.*;
import java.util.*;

// desktop stand-in for android.database.sqlite.SQLiteDatabase on sqlite-jdbc,
// for running the phone server on a PC (maple-offline dev server)
public final class SQLiteDatabase implements AutoCloseable {
    public static final int CONFLICT_NONE = 0;
    public static final int CONFLICT_ROLLBACK = 1;
    public static final int CONFLICT_ABORT = 2;
    public static final int CONFLICT_FAIL = 3;
    public static final int CONFLICT_IGNORE = 4;
    public static final int CONFLICT_REPLACE = 5;
    private static final String[] CONFLICT = {"", " OR ROLLBACK", " OR ABORT", " OR FAIL", " OR IGNORE", " OR REPLACE"};

    public interface CursorFactory {}

    final Connection conn;
    private int txDepth = 0;
    private boolean txFailed = false;
    private final Deque<Boolean> txSuccess = new ArrayDeque<>();

    SQLiteDatabase(String path) {
        try {
            conn = DriverManager.getConnection("jdbc:sqlite:" + path);
            conn.setAutoCommit(true);
            try (Statement s = conn.createStatement()) {
                s.execute("PRAGMA busy_timeout=5000");
            }
        } catch (java.sql.SQLException e) {
            throw new SQLiteException("open " + path, e);
        }
    }

    public static SQLiteDatabase openOrCreateDatabase(String path, CursorFactory f) { return new SQLiteDatabase(path); }

    static void bind(PreparedStatement ps, Object[] args) throws java.sql.SQLException {
        if (args == null) return;
        for (int i = 0; i < args.length; i++) {
            Object a = args[i];
            if (a == null) ps.setNull(i + 1, Types.NULL);
            else if (a instanceof Boolean) ps.setLong(i + 1, ((Boolean) a) ? 1 : 0);
            else if (a instanceof byte[]) ps.setBytes(i + 1, (byte[]) a);
            else if (a instanceof Double || a instanceof Float) ps.setDouble(i + 1, ((Number) a).doubleValue());
            else if (a instanceof Number) ps.setLong(i + 1, ((Number) a).longValue());
            else ps.setString(i + 1, a.toString());
        }
    }

    static Cursor read(ResultSet rs) throws java.sql.SQLException {
        ResultSetMetaData md = rs.getMetaData();
        int n = md.getColumnCount();
        String[] cols = new String[n];
        for (int i = 0; i < n; i++) cols[i] = md.getColumnLabel(i + 1);
        List<Object[]> rows = new ArrayList<>();
        while (rs.next()) {
            Object[] row = new Object[n];
            for (int i = 0; i < n; i++) row[i] = rs.getObject(i + 1);
            rows.add(row);
        }
        return new MemoryCursor(cols, rows);
    }

    private static SQLiteException wrap(String sql, java.sql.SQLException e) {
        String m = e.getMessage() == null ? "" : e.getMessage();
        if (m.contains("CONSTRAINT") || m.contains("constraint")) return new SQLiteConstraintException(m + " in: " + sql);
        return new SQLiteException(m + " in: " + sql, e);
    }

    public synchronized Cursor rawQuery(String sql, String[] selectionArgs) {
        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            bind(ps, selectionArgs);
            if (!ps.execute()) return new MemoryCursor(new String[0], null);
            try (ResultSet rs = ps.getResultSet()) { return read(rs); }
        } catch (java.sql.SQLException e) { throw wrap(sql, e); }
    }

    public Cursor rawQuery(String sql, String[] selectionArgs, Object cancel) { return rawQuery(sql, selectionArgs); }

    public synchronized void execSQL(String sql) {
        try (Statement s = conn.createStatement()) { s.execute(sql); }
        catch (java.sql.SQLException e) { throw wrap(sql, e); }
    }

    public synchronized void execSQL(String sql, Object[] bindArgs) {
        try (PreparedStatement ps = conn.prepareStatement(sql)) { bind(ps, bindArgs); ps.execute(); }
        catch (java.sql.SQLException e) { throw wrap(sql, e); }
    }

    public Cursor query(String table, String[] columns, String selection, String[] selectionArgs,
                        String groupBy, String having, String orderBy) {
        return query(false, table, columns, selection, selectionArgs, groupBy, having, orderBy, null);
    }

    public Cursor query(String table, String[] columns, String selection, String[] selectionArgs,
                        String groupBy, String having, String orderBy, String limit) {
        return query(false, table, columns, selection, selectionArgs, groupBy, having, orderBy, limit);
    }

    public Cursor query(boolean distinct, String table, String[] columns, String selection, String[] selectionArgs,
                        String groupBy, String having, String orderBy, String limit) {
        StringBuilder sb = new StringBuilder("SELECT ");
        if (distinct) sb.append("DISTINCT ");
        sb.append(columns == null || columns.length == 0 ? "*" : String.join(", ", columns));
        sb.append(" FROM ").append(table);
        if (selection != null && !selection.isEmpty()) sb.append(" WHERE ").append(selection);
        if (groupBy != null && !groupBy.isEmpty()) sb.append(" GROUP BY ").append(groupBy);
        if (having != null && !having.isEmpty()) sb.append(" HAVING ").append(having);
        if (orderBy != null && !orderBy.isEmpty()) sb.append(" ORDER BY ").append(orderBy);
        if (limit != null && !limit.isEmpty()) sb.append(" LIMIT ").append(limit);
        return rawQuery(sb.toString(), selectionArgs);
    }

    private synchronized long insertInternal(String table, ContentValues values, int conflict) {
        StringBuilder sb = new StringBuilder("INSERT").append(CONFLICT[conflict]).append(" INTO ").append(table);
        List<Object> args = new ArrayList<>();
        if (values == null || values.size() == 0) {
            sb.append(" DEFAULT VALUES");
        } else {
            sb.append(" (");
            StringBuilder q = new StringBuilder();
            boolean first = true;
            for (Map.Entry<String, Object> e : values.valueSet()) {
                if (!first) { sb.append(", "); q.append(", "); }
                sb.append(e.getKey());
                q.append('?');
                args.add(e.getValue());
                first = false;
            }
            sb.append(") VALUES (").append(q).append(')');
        }
        String sql = sb.toString();
        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            bind(ps, args.toArray());
            int n = ps.executeUpdate();
            if (n == 0) return -1;
            try (Statement s = conn.createStatement(); ResultSet rs = s.executeQuery("SELECT last_insert_rowid()")) {
                return rs.next() ? rs.getLong(1) : -1;
            }
        } catch (java.sql.SQLException e) { throw wrap(sql, e); }
    }

    public long insert(String table, String nullColumnHack, ContentValues values) {
        try { return insertInternal(table, values, CONFLICT_NONE); }
        catch (SQLiteException e) { System.err.println("[devdb] insert failed: " + e.getMessage()); return -1; }
    }

    public long insertOrThrow(String table, String nullColumnHack, ContentValues values) {
        return insertInternal(table, values, CONFLICT_NONE);
    }

    public long insertWithOnConflict(String table, String nullColumnHack, ContentValues values, int conflict) {
        try { return insertInternal(table, values, conflict); }
        catch (SQLiteException e) { System.err.println("[devdb] insert failed: " + e.getMessage()); return -1; }
    }

    public long replace(String table, String nullColumnHack, ContentValues values) {
        try { return insertInternal(table, values, CONFLICT_REPLACE); }
        catch (SQLiteException e) { System.err.println("[devdb] replace failed: " + e.getMessage()); return -1; }
    }

    public long replaceOrThrow(String table, String nullColumnHack, ContentValues values) {
        return insertInternal(table, values, CONFLICT_REPLACE);
    }

    public int update(String table, ContentValues values, String whereClause, String[] whereArgs) {
        return updateWithOnConflict(table, values, whereClause, whereArgs, CONFLICT_NONE);
    }

    public synchronized int updateWithOnConflict(String table, ContentValues values, String whereClause, String[] whereArgs, int conflict) {
        if (values == null || values.size() == 0) throw new IllegalArgumentException("Empty values");
        StringBuilder sb = new StringBuilder("UPDATE").append(CONFLICT[conflict]).append(' ').append(table).append(" SET ");
        List<Object> args = new ArrayList<>();
        boolean first = true;
        for (Map.Entry<String, Object> e : values.valueSet()) {
            if (!first) sb.append(", ");
            sb.append(e.getKey()).append("=?");
            args.add(e.getValue());
            first = false;
        }
        if (whereClause != null && !whereClause.isEmpty()) sb.append(" WHERE ").append(whereClause);
        if (whereArgs != null) args.addAll(Arrays.asList(whereArgs));
        String sql = sb.toString();
        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            bind(ps, args.toArray());
            return ps.executeUpdate();
        } catch (java.sql.SQLException e) { throw wrap(sql, e); }
    }

    public synchronized int delete(String table, String whereClause, String[] whereArgs) {
        String sql = "DELETE FROM " + table + (whereClause != null && !whereClause.isEmpty() ? " WHERE " + whereClause : "");
        try (PreparedStatement ps = conn.prepareStatement(sql)) {
            bind(ps, whereArgs);
            return ps.executeUpdate();
        } catch (java.sql.SQLException e) { throw wrap(sql, e); }
    }

    public SQLiteStatement compileStatement(String sql) { return new SQLiteStatement(this, sql); }

    public synchronized void beginTransaction() {
        try {
            if (txDepth == 0) { conn.setAutoCommit(false); txFailed = false; }
            txDepth++;
            txSuccess.push(false);
        } catch (java.sql.SQLException e) { throw new SQLiteException("begin", e); }
    }

    public void beginTransactionNonExclusive() { beginTransaction(); }

    public synchronized void setTransactionSuccessful() {
        if (!txSuccess.isEmpty()) { txSuccess.pop(); txSuccess.push(true); }
    }

    public synchronized void endTransaction() {
        if (txDepth == 0) throw new IllegalStateException("no transaction pending");
        boolean ok = txSuccess.pop();
        if (!ok) txFailed = true;
        txDepth--;
        if (txDepth == 0) {
            try {
                if (txFailed) conn.rollback(); else conn.commit();
                conn.setAutoCommit(true);
            } catch (java.sql.SQLException e) { throw new SQLiteException("end", e); }
        }
    }

    public synchronized boolean inTransaction() { return txDepth > 0; }

    public boolean isOpen() {
        try { return !conn.isClosed(); } catch (java.sql.SQLException e) { return false; }
    }

    public String getPath() { return ""; }

    public int getVersion() { return 1; }

    public void setVersion(int v) {}

    @Override
    public void close() {}

    public void releaseReference() {}
}
