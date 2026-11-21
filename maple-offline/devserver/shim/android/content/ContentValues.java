package android.content;

import java.util.*;

public final class ContentValues {
    private final LinkedHashMap<String, Object> values = new LinkedHashMap<>();

    public ContentValues() {}
    public ContentValues(int size) {}
    public ContentValues(ContentValues from) { values.putAll(from.values); }

    public void put(String key, String value) { values.put(key, value); }
    public void put(String key, Byte value) { values.put(key, value); }
    public void put(String key, Short value) { values.put(key, value); }
    public void put(String key, Integer value) { values.put(key, value); }
    public void put(String key, Long value) { values.put(key, value); }
    public void put(String key, Float value) { values.put(key, value); }
    public void put(String key, Double value) { values.put(key, value); }
    public void put(String key, Boolean value) { values.put(key, value); }
    public void put(String key, byte[] value) { values.put(key, value); }
    public void putAll(ContentValues other) { values.putAll(other.values); }
    public void putNull(String key) { values.put(key, null); }
    public int size() { return values.size(); }
    public boolean isEmpty() { return values.isEmpty(); }
    public void remove(String key) { values.remove(key); }
    public void clear() { values.clear(); }
    public boolean containsKey(String key) { return values.containsKey(key); }
    public Object get(String key) { return values.get(key); }
    public String getAsString(String key) { Object v = values.get(key); return v == null ? null : v.toString(); }
    public Integer getAsInteger(String key) { Object v = values.get(key); return v instanceof Number ? ((Number) v).intValue() : v == null ? null : Integer.valueOf(v.toString()); }
    public Long getAsLong(String key) { Object v = values.get(key); return v instanceof Number ? ((Number) v).longValue() : v == null ? null : Long.valueOf(v.toString()); }
    public Set<String> keySet() { return values.keySet(); }
    public Set<Map.Entry<String, Object>> valueSet() { return values.entrySet(); }
    @Override public String toString() { return values.toString(); }
}
