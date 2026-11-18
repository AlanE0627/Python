package com.mapleserver

import android.os.StatFs
import java.io.File
import java.io.FileOutputStream
import java.io.RandomAccessFile
import java.net.HttpURLConnection
import java.net.URL

/**
 * First-run game data setup, done entirely on the phone:
 * download the GMS v83 installer and the Cosmic custom WZ files, extract the installer,
 * overlay the custom WZ files, convert every WZ to NX (client format), then clean up.
 * Nothing is bundled in the APK; every step resumes if interrupted.
 */
class AssetSetup(private val dataDir: File, private val report: (String) -> Unit) {

    companion object {
        init { System.loadLibrary("OpenMapleClient") }

        private const val BASE = "https://media.githubusercontent.com/media/P0nk/Cosmic-client/main"
        private const val INSTALLER = "MapleGlobal-v83-setup.exe"
        private const val INSTALLER_SIZE = 1763606856L
        val CUSTOM_WZ = listOf("Character", "Etc", "Item", "List", "Map", "Mob", "Npc",
            "Quest", "Reactor", "Skill", "String", "UI")
        val CLIENT_WZ = listOf("Base", "Character", "Effect", "Etc", "Item", "Map", "Mob",
            "Morph", "Npc", "Quest", "Reactor", "Skill", "Sound", "String", "TamingMob", "UI")
        const val NEEDED_BYTES = 8L * 1024 * 1024 * 1024
    }

    private val work = File(dataDir, "setup")
    private var stageLabel = ""

    external fun nativeExtract(setupExe: String, outDir: String): String?
    external fun nativeConvert(wzPath: String): String?

    /** Called from native code during extraction. */
    @Suppress("unused")
    fun onProgress(stage: String, done: Long, total: Long) {
        val pct = if (total > 0) done * 100 / total else 0
        report("$stageLabel ${pct}%")
    }

    fun isDone(): Boolean = CLIENT_WZ.all { File(dataDir, "$it.nx").length() > 0 }

    /** Returns null on success, otherwise a message for the user. */
    fun run(): String? {
        if (isDone()) return null
        work.mkdirs()
        val free = StatFs(dataDir.absolutePath).availableBytes
        if (free < NEEDED_BYTES && !File(work, "extracted.ok").exists()) {
            return "저장 공간이 부족합니다. 약 8 GB가 필요합니다 (현재 여유 ${free / (1 shl 30)} GB)."
        }

        // 1) Installer
        val installer = File(work, INSTALLER)
        val extracted = File(work, "v83")
        if (!File(work, "extracted.ok").exists()) {
            stageLabel = "① 설치 파일 다운로드"
            download("$BASE/$INSTALLER", installer, INSTALLER_SIZE)?.let { return it }

            // 2) Extract
            stageLabel = "② 설치 파일 풀기"
            extracted.mkdirs()
            nativeExtract(installer.absolutePath, extracted.absolutePath)?.let { return "설치 파일 풀기 실패: $it" }
            File(work, "extracted.ok").writeText("ok")
            installer.delete()
        }

        // 3) Cosmic custom WZ overlay (server data expects these edits)
        for ((i, name) in CUSTOM_WZ.withIndex()) {
            stageLabel = "③ 추가 데이터 다운로드 (${i + 1}/${CUSTOM_WZ.size})"
            val marker = File(work, "custom-$name.ok")
            if (marker.exists()) continue
            val tmp = File(work, "custom-$name.wz")
            download("$BASE/cosmic-wz/$name.wz", tmp, -1)?.let { return it }
            val dst = File(extracted, "$name.wz")
            dst.delete()
            if (!tmp.renameTo(dst)) return "추가 데이터 적용 실패: $name"
            marker.writeText("ok")
        }

        // 4) WZ -> NX
        for ((i, name) in CLIENT_WZ.withIndex()) {
            val nx = File(dataDir, "$name.nx")
            if (nx.length() > 0) continue
            stageLabel = "④ 데이터 변환 (${i + 1}/${CLIENT_WZ.size}) $name"
            report(stageLabel)
            val wz = File(extracted, "$name.wz")
            if (!wz.exists()) return "변환할 파일이 없습니다: $name.wz"
            nativeConvert(wz.absolutePath)?.let { return "변환 실패 ($name): $it" }
            val out = File(extracted, "$name.nx")
            if (out.length() == 0L) return "변환 실패 ($name)"
            if (!out.renameTo(nx)) return "변환 결과 이동 실패 ($name)"
            wz.delete()
        }

        // 5) Clean up
        work.deleteRecursively()
        return null
    }

    private fun download(url: String, dst: File, expected: Long): String? {
        var attempt = 0
        while (true) {
            try {
                val have = if (dst.exists()) dst.length() else 0L
                if (expected > 0 && have == expected) return null
                val conn = URL(url).openConnection() as HttpURLConnection
                conn.connectTimeout = 20000
                conn.readTimeout = 60000
                conn.instanceFollowRedirects = true
                if (have > 0) conn.setRequestProperty("Range", "bytes=$have-")
                val code = conn.responseCode
                val resume = code == 206
                if (code != 200 && code != 206) throw RuntimeException("HTTP $code")
                val total = if (resume) have + conn.contentLengthLong else conn.contentLengthLong
                RandomAccessFile(dst, "rw").use { raf ->
                    if (resume) raf.seek(have) else raf.setLength(0)
                    conn.inputStream.use { input ->
                        val buf = ByteArray(1 shl 20)
                        var done = if (resume) have else 0L
                        var lastReport = 0L
                        while (true) {
                            val n = input.read(buf)
                            if (n < 0) break
                            raf.write(buf, 0, n)
                            done += n
                            if (done - lastReport > (8 shl 20)) {
                                lastReport = done
                                val pct = if (total > 0) done * 100 / total else 0
                                report("$stageLabel ${pct}% (${done shr 20}/${total shr 20} MB)")
                            }
                        }
                    }
                }
                if (expected > 0 && dst.length() != expected) throw RuntimeException("size mismatch")
                return null
            } catch (e: Exception) {
                attempt++
                if (attempt >= 5) return "다운로드 실패: ${e.message}. 인터넷 연결 확인 후 앱을 다시 실행해 주세요 (이어받기 됩니다)."
                report("$stageLabel 재시도 $attempt/5…")
                Thread.sleep(3000L * attempt)
            }
        }
    }
}
