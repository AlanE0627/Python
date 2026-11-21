package android.database;

import java.util.ArrayList;
import java.util.List;

// a cursor over rows already read from JDBC (desktop dev server)
public final class MemoryCursor implements Cursor {
    private final String[] columns;
    private final List<Object[]> rows;
    private int pos = -1;
    private boolean closed;

    public MemoryCursor(String[] columns, List<Object[]> rows) {
        this.columns = columns;
        this.rows = rows == null ? new ArrayList<>() : rows;
    }

    private Object val(int i) {
        if (pos < 0 || pos >= rows.size()) {
            throw new CursorIndexOutOfBoundsException("Index " + pos + " requested, with a size of " + rows.size());
        }
        if (i < 0 || i >= columns.length) {
            throw new IllegalStateException("Couldn't read row " + pos + ", col " + i + " from CursorWindow");
        }
        return rows.get(pos)[i];
    }

    public int getCount() { return rows.size(); }
    public int getPosition() { return pos; }
    public boolean move(int offset) { return moveToPosition(pos + offset); }
    public boolean moveToPosition(int p) {
        if (p >= rows.size()) { pos = rows.size(); return false; }
        if (p < 0) { pos = -1; return false; }
        pos = p; return true;
    }
    public boolean moveToFirst() { return moveToPosition(0); }
    public boolean moveToLast() { return moveToPosition(rows.size() - 1); }
    public boolean moveToNext() { return moveToPosition(pos + 1); }
    public boolean moveToPrevious() { return moveToPosition(pos - 1); }
    public boolean isFirst() { return pos == 0 && !rows.isEmpty(); }
    public boolean isLast() { return !rows.isEmpty() && pos == rows.size() - 1; }
    public boolean isBeforeFirst() { return rows.isEmpty() || pos == -1; }
    public boolean isAfterLast() { return rows.isEmpty() || pos == rows.size(); }
    public int getColumnIndex(String name) {
        int dot = name.lastIndexOf('.');
        String n = dot >= 0 ? name.substring(dot + 1) : name;
        for (int i = 0; i < columns.length; i++) {
            if (columns[i].equalsIgnoreCase(n)) return i;
        }
        return -1;
    }
    public int getColumnIndexOrThrow(String name) {
        int i = getColumnIndex(name);
        if (i < 0) throw new IllegalArgumentException("column '" + name + "' does not exist");
        return i;
    }
    public String getColumnName(int i) { return columns[i]; }
    public String[] getColumnNames() { return columns.clone(); }
    public int getColumnCount() { return columns.length; }
    public byte[] getBlob(int i) {
        Object v = val(i);
        if (v == null) return null;
        if (v instanceof byte[]) return (byte[]) v;
        return v.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8);
    }
    public String getString(int i) {
        Object v = val(i);
        if (v == null) return null;
        if (v instanceof byte[]) return new String((byte[]) v, java.nio.charset.StandardCharsets.UTF_8);
        if (v instanceof Double || v instanceof Float) {
            double d = ((Number) v).doubleValue();
            return d == Math.rint(d) && !Double.isInfinite(d) ? String.valueOf(d) : String.valueOf(d);
        }
        return v.toString();
    }
    private long num(Object v) {
        if (v == null) return 0;
        if (v instanceof Number) return ((Number) v).longValue();
        String s = v.toString().trim();
        try { return Long.parseLong(s); } catch (NumberFormatException e) {
            try { return (long) Double.parseDouble(s); } catch (NumberFormatException e2) { return 0; }
        }
    }
    private double dnum(Object v) {
        if (v == null) return 0;
        if (v instanceof Number) return ((Number) v).doubleValue();
        try { return Double.parseDouble(v.toString().trim()); } catch (NumberFormatException e) { return 0; }
    }
    public short getShort(int i) { return (short) num(val(i)); }
    public int getInt(int i) { return (int) num(val(i)); }
    public long getLong(int i) { return num(val(i)); }
    public float getFloat(int i) { return (float) dnum(val(i)); }
    public double getDouble(int i) { return dnum(val(i)); }
    public int getType(int i) {
        Object v = val(i);
        if (v == null) return FIELD_TYPE_NULL;
        if (v instanceof Integer || v instanceof Long || v instanceof Short || v instanceof Byte) return FIELD_TYPE_INTEGER;
        if (v instanceof Double || v instanceof Float) return FIELD_TYPE_FLOAT;
        if (v instanceof byte[]) return FIELD_TYPE_BLOB;
        return FIELD_TYPE_STRING;
    }
    public boolean isNull(int i) { return val(i) == null; }
    public void close() { closed = true; }
    public boolean isClosed() { return closed; }
}
