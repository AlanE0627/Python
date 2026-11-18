package com.mapleserver

import android.app.Activity
import android.app.PendingIntent
import android.content.Intent
import android.graphics.Color
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.widget.TextView
import openMapleClient.MapleActivity
import java.io.File
import java.net.InetSocketAddress
import java.net.Socket

/**
 * Single-APK entry point: starts the local server as a foreground service,
 * waits until it accepts connections on 127.0.0.1:8484, then opens the game client.
 */
class GameLauncherActivity : Activity() {
    private lateinit var status: TextView
    private val main = Handler(Looper.getMainLooper())
    @Volatile private var cancelled = false

    private val requiredNx = listOf(
        "Base.nx", "Character.nx", "Effect.nx", "Etc.nx", "Item.nx", "Map.nx",
        "Mob.nx", "Morph.nx", "Npc.nx", "Quest.nx", "Reactor.nx", "Skill.nx",
        "Sound.nx", "String.nx", "TamingMob.nx", "UI.nx"
    )

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        status = TextView(this).apply {
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.BLACK)
            gravity = Gravity.CENTER
            textSize = 18f
        }
        setContentView(status)

        val dataDir = getExternalFilesDir(null) ?: filesDir
        val missing = requiredNx.filter { !File(dataDir, it).isFile }
        if (missing.isNotEmpty()) {
            status.text = "게임 데이터(NX)가 없습니다.\n\n아래 폴더에 NX 파일을 넣어 주세요:\n${dataDir.absolutePath}\n\n없는 파일: ${missing.joinToString(", ")}"
            return
        }
        // Client reads the server address from hostip.txt; always local.
        File(dataDir, "hostip.txt").writeText("127.0.0.1")
        // The client resolves Map001/MapLatest/MapPretty; on v83 they are all Map.nx.
        for (alias in listOf("Map001.nx", "MapLatest.nx", "MapPretty.nx")) {
            val f = File(dataDir, alias)
            if (!f.exists()) {
                try {
                    java.nio.file.Files.createSymbolicLink(f.toPath(), File(dataDir, "Map.nx").toPath())
                } catch (e: Exception) {
                    // Fallback handled in native code (alias -> Map.nx).
                }
            }
        }

        status.text = "서버 시작 중…"
        startServer()
        waitForServer()
    }

    override fun onDestroy() {
        cancelled = true
        super.onDestroy()
    }

    private fun startServer() {
        val intent = Intent(this, ServerService::class.java)
        intent.putExtra("channel_id", "maple_offline_server")
        intent.putExtra(
            "pending_intent",
            PendingIntent.getActivity(this, 0, Intent(this, GameLauncherActivity::class.java),
                PendingIntent.FLAG_IMMUTABLE)
        )
        startForegroundService(intent)
    }

    private fun waitForServer() {
        Thread {
            val start = System.currentTimeMillis()
            while (!cancelled) {
                if (portOpen()) {
                    main.post {
                        startActivity(Intent(this, MapleActivity::class.java))
                        finish()
                    }
                    return@Thread
                }
                val sec = (System.currentTimeMillis() - start) / 1000
                if (sec > 600) {
                    main.post { status.text = "서버가 시작되지 않았습니다 (10분 초과).\n앱을 다시 실행해 주세요." }
                    return@Thread
                }
                main.post { status.text = "서버 시작 중… ${sec}초" }
                Thread.sleep(1000)
            }
        }.start()
    }

    private fun portOpen(): Boolean = try {
        Socket().use { it.connect(InetSocketAddress("127.0.0.1", 8484), 500); true }
    } catch (e: Exception) {
        false
    }
}
