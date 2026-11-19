package com.mapleserver

import android.content.Context
import android.content.Intent

/**
 * maple-offline: the native client keeps its state in C++ globals, so it can
 * run once per process. These flags reset when the process dies; when a
 * screen comes up in a state the client cannot handle (game screen restored
 * after the process was killed, or the game screen opened a second time in
 * the same process) the app restarts itself in a fresh process.
 */
object GameState {
    @Volatile var serverStarted = false
    @Volatile var nativeUsed = false

    fun restartApp(ctx: Context) {
        val intent = Intent(ctx, GameLauncherActivity::class.java)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
        ctx.startActivity(intent)
        Runtime.getRuntime().exit(0)
    }
}
