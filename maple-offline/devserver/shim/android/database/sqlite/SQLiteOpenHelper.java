package android.database.sqlite;

import android.content.Context;

public abstract class SQLiteOpenHelper implements AutoCloseable {
    private final Context context;
    private final String name;
    private SQLiteDatabase db;

    public SQLiteOpenHelper(Context context, String name, SQLiteDatabase.CursorFactory factory, int version) {
        this.context = context;
        this.name = name;
    }

    public synchronized SQLiteDatabase getWritableDatabase() {
        if (db == null) {
            boolean fresh = !context.getDatabasePath(name).exists();
            db = new SQLiteDatabase(context.getDatabasePath(name).getPath());
            if (fresh) onCreate(db);
            onOpen(db);
        }
        return db;
    }

    public SQLiteDatabase getReadableDatabase() { return getWritableDatabase(); }

    public String getDatabaseName() { return name; }

    public abstract void onCreate(SQLiteDatabase db);

    public abstract void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion);

    public void onOpen(SQLiteDatabase db) {}

    public void onConfigure(SQLiteDatabase db) {}

    @Override
    public synchronized void close() {}
}
