package com.mapleserver

import android.content.Context
import java.io.File
import java.io.PrintWriter
import java.io.StringWriter

/**
 * maple-offline: records an uncaught error to crash.txt in the app data
 * folder so the launcher can show it on the next start (no logcat needed).
 */
object CrashLog {
    @Volatile private var installed = false

    fun file(ctx: Context): File = File(ctx.getExternalFilesDir(null) ?: ctx.filesDir, "crash.txt")

    fun write(ctx: Context, where: String, t: Throwable) {
        try {
            val sw = StringWriter()
            t.printStackTrace(PrintWriter(sw))
            file(ctx).writeText("[$where] ${java.util.Date()}\n$sw")
        } catch (_: Throwable) {
        }
    }

    fun install(ctx: Context) {
        if (installed) return
        installed = true
        val app = ctx.applicationContext
        val prev = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { th, e ->
            write(app, "thread ${th.name}", e)
            prev?.uncaughtException(th, e)
        }
    }
}
