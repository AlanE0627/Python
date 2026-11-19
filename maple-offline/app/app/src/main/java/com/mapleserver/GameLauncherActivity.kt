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
        // the game screen already ran in this process: start over clean
        if (GameState.nativeUsed) {
            GameState.restartApp(this)
            return
        }
        status = TextView(this).apply {
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.BLACK)
            gravity = Gravity.CENTER
            textSize = 18f
        }
        setContentView(status)

        window.addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        CrashLog.install(applicationContext)
        val dataDir = getExternalFilesDir(null) ?: filesDir

        // A crash from the previous run: show it once, start on tap.
        val crash = CrashLog.file(this)
        if (crash.exists()) {
            val text = crash.readText().lines().take(30).joinToString("\n")
            crash.renameTo(File(crash.parentFile, "crash_prev.txt"))
            status.gravity = Gravity.START or Gravity.TOP
            status.textSize = 11f
            status.setTextIsSelectable(true)
            status.text = "지난 실행 오류 (화면을 캡처해 보내 주세요, 화면을 누르면 계속)\n\n$text"
            status.setOnClickListener {
                status.setOnClickListener(null)
                status.gravity = Gravity.CENTER
                status.textSize = 18f
                begin(dataDir)
            }
            return
        }
        // Server warnings/errors from the previous run (no logcat on the
        // phone): show them once so they can be captured, start on tap.
        val log = File(filesDir, "server.log")
        if (log.exists()) {
            val lines = try {
                log.readLines().filter { (it.contains(" WARN ") || it.contains(" ERROR ") ||
                        it.contains("Exception")) &&
                        // harmless on phones: netty picks a random id without a MAC
                        !it.contains("MacAddressUtil") }.takeLast(25)
            } catch (e: Exception) { emptyList() }
            log.renameTo(File(filesDir, "server_prev.log"))
            if (lines.isNotEmpty()) {
                status.gravity = Gravity.START or Gravity.TOP
                status.textSize = 10f
                status.text = "지난 실행 서버 경고 (화면을 캡처해 보내 주세요, 화면을 누르면 계속)\n\n" +
                        lines.joinToString("\n")
                status.setOnClickListener {
                    status.setOnClickListener(null)
                    status.gravity = Gravity.CENTER
                    status.textSize = 18f
                    begin(dataDir)
                }
                return
            }
        }
        begin(dataDir)
    }

    private fun begin(dataDir: File) {
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

    private val serverMsg = object : android.content.BroadcastReceiver() {
        override fun onReceive(c: android.content.Context?, i: Intent?) {
            if (i?.getStringExtra("Status") != "ServerFailed") return
            cancelled = true
            val text = CrashLog.file(this@GameLauncherActivity).let { if (it.exists()) it.readText() else "" }
                .lines().take(30).joinToString("\n")
            status.gravity = Gravity.START or Gravity.TOP
            status.textSize = 11f
            status.setTextIsSelectable(true)
            status.text = "서버 시작 실패 (화면을 캡처해 보내 주세요)\n\n$text"
        }
    }

    private fun launchGame(dataDir: File) {
        registerReceiver(serverMsg, android.content.IntentFilter("MapleServerMessage"),
            android.content.Context.RECEIVER_NOT_EXPORTED)
        // Client reads the server address from hostip.txt; always local.
        File(dataDir, "hostip.txt").writeText("127.0.0.1")
        installServerConfig()
        installKoreanData(dataDir)
        status.text = "서버 시작 중…"
        startServer()
        waitForServer()
    }

    // maple-offline: the server reads files/config.yaml (the old server-only
    // MainActivity copied it); refresh it from the APK on every start so rate
    // and option changes in new builds take effect.
    private fun installServerConfig() {
        try {
            val dst = File(filesDir, "config.yaml")
            val bundled = assets.open("config.yaml").use { it.readBytes() }
            if (!dst.exists() || !dst.readBytes().contentEquals(bundled)) dst.writeBytes(bundled)
        } catch (e: Exception) {
            android.util.Log.w("maple-offline", "config.yaml copy failed", e)
        }
    }

    // maple-offline: Korean strings / quest text ship inside the APK
    // (assets/ko) and replace the English data whenever the bundled copy
    // differs (the client prefers String_ko.nx / Quest_ko.nx when present).
    private fun installKoreanData(dataDir: File) {
        for (name in listOf("String_ko.nx", "Quest_ko.nx", "UI_ko.nx", "Effect_ko.nx")) {
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
        try { unregisterReceiver(serverMsg) } catch (_: Exception) {}
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
                    GameState.serverStarted = true
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
