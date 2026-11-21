package android.database.sqlite;

import java.sql.*;
import java.util.*;

public final class SQLiteStatement implements AutoCloseable {
    private final SQLiteDatabase db;
    private final String sql;
    private final Map<Integer, Object> binds = new TreeMap<>();

    SQLiteStatement(SQLiteDatabase db, String sql) { this.db = db; this.sql = sql; }

    public void bindNull(int index) { binds.put(index, null); }
    public void bindLong(int index, long value) { binds.put(index, value); }
    public void bindDouble(int index, double value) { binds.put(index, value); }
    public void bindString(int index, String value) { binds.put(index, value); }
    public void bindBlob(int index, byte[] value) { binds.put(index, value); }
    public void clearBindings() { binds.clear(); }
    public void bindAllArgsAsStrings(String[] args) { if (args != null) for (int i = args.length; i != 0; i--) bindString(i, args[i - 1]); }

    private Object[] args() {
        int max = binds.isEmpty() ? 0 : Collections.max(binds.keySet());
        Object[] a = new Object[max];
        for (Map.Entry<Integer, Object> e : binds.entrySet()) a[e.getKey() - 1] = e.getValue();
        return a;
    }

    public void execute() {
        synchronized (db) {
            try (PreparedStatement ps = db.conn.prepareStatement(sql)) { SQLiteDatabase.bind(ps, args()); ps.execute(); }
            catch (java.sql.SQLException e) { throw new SQLiteException(e.getMessage() + " in: " + sql, e); }
        }
    }

    public int executeUpdateDelete() {
        synchronized (db) {
            try (PreparedStatement ps = db.conn.prepareStatement(sql)) { SQLiteDatabase.bind(ps, args()); return ps.executeUpdate(); }
            catch (java.sql.SQLException e) { throw new SQLiteException(e.getMessage() + " in: " + sql, e); }
        }
    }

    public long executeInsert() {
        synchronized (db) {
            try (PreparedStatement ps = db.conn.prepareStatement(sql)) {
                SQLiteDatabase.bind(ps, args());
                if (ps.executeUpdate() == 0) return -1;
                try (Statement s = db.conn.createStatement(); ResultSet rs = s.executeQuery("SELECT last_insert_rowid()")) {
                    return rs.next() ? rs.getLong(1) : -1;
                }
            } catch (java.sql.SQLException e) { throw new SQLiteException(e.getMessage() + " in: " + sql, e); }
        }
    }

    public long simpleQueryForLong() {
        synchronized (db) {
            try (PreparedStatement ps = db.conn.prepareStatement(sql)) {
                SQLiteDatabase.bind(ps, args());
                try (ResultSet rs = ps.executeQuery()) {
                    if (!rs.next()) throw new SQLiteDoneException();
                    return rs.getLong(1);
                }
            } catch (java.sql.SQLException e) { throw new SQLiteException(e.getMessage() + " in: " + sql, e); }
        }
    }

    public String simpleQueryForString() {
        synchronized (db) {
            try (PreparedStatement ps = db.conn.prepareStatement(sql)) {
                SQLiteDatabase.bind(ps, args());
                try (ResultSet rs = ps.executeQuery()) {
                    if (!rs.next()) throw new SQLiteDoneException();
                    return rs.getString(1);
                }
            } catch (java.sql.SQLException e) { throw new SQLiteException(e.getMessage() + " in: " + sql, e); }
        }
    }

    @Override
    public void close() {}
}
