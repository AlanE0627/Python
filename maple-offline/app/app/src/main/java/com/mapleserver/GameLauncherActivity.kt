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

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        status = TextView(this).apply {
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.BLACK)
            gravity = Gravity.CENTER
            textSize = 18f
        }
        setContentView(status)

        window.addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        val dataDir = getExternalFilesDir(null) ?: filesDir
        val setup = AssetSetup(dataDir) { msg -> main.post { status.text = "첫 실행 데이터 준비 중\n\n$msg\n\n(Wi-Fi 연결 권장, 화면을 켜 둔 채로 기다려 주세요)" } }
        if (setup.isDone()) {
            launchGame(dataDir)
        } else {
            status.text = "첫 실행 데이터 준비를 시작합니다…"
            Thread {
                val err = setup.run()
                main.post {
                    if (err != null) status.text = err
                    else launchGame(dataDir)
                }
            }.start()
        }
    }

    private fun launchGame(dataDir: File) {
        // Client reads the server address from hostip.txt; always local.
        File(dataDir, "hostip.txt").writeText("127.0.0.1")
        installKoreanData(dataDir)
        status.text = "서버 시작 중…"
        startServer()
        waitForServer()
    }

    // maple-offline: Korean strings / quest text ship inside the APK
    // (assets/ko) and replace the English data whenever the bundled copy
    // differs (the client prefers String_ko.nx / Quest_ko.nx when present).
    private fun installKoreanData(dataDir: File) {
        for (name in listOf("String_ko.nx", "Quest_ko.nx", "UI_ko.nx")) {
            try {
                val dst = File(dataDir, name)
                val size = assets.openFd("ko/$name").use { it.length }
                if (dst.length() == size) continue
                val tmp = File(dataDir, "$name.tmp")
                assets.open("ko/$name").use { input ->
                    tmp.outputStream().use { output -> input.copyTo(output) }
                }
                tmp.renameTo(dst)
            } catch (e: Exception) {
                android.util.Log.w("maple-offline", "Korean data copy failed: $name", e)
            }
        }
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
