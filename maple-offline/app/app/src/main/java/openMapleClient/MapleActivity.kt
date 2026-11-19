package openMapleClient

import android.app.NativeActivity
import android.content.Context
import android.os.Bundle
import android.text.Editable
import android.text.InputType
import android.text.TextWatcher
import android.view.KeyEvent
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodManager
import android.widget.EditText
import net.server.Server

class MapleActivity : NativeActivity() {
    // maple-offline: NativeActivity has no InputConnection, so the Android
    // keyboard cannot compose Hangul into the game. A 1x1 transparent EditText
    // takes the keyboard while a game text field is focused and mirrors its
    // whole content (composition included) into that field.
    private var input: EditText? = null
    private var mirroring = false

    private external fun nativeSetText(text: String)
    private external fun nativeReturn()

    override fun onCreate(savedInstanceState: Bundle?) {
        // maple-offline: restored straight into the game screen after the
        // process was killed (no server running), or opened a second time in
        // one process (stale native state): restart through the launcher.
        if (!com.mapleserver.GameState.serverStarted || com.mapleserver.GameState.nativeUsed) {
            super.onCreate(null)
            com.mapleserver.GameState.restartApp(this)
            return
        }
        com.mapleserver.GameState.nativeUsed = true
        super.onCreate(savedInstanceState)

        val edit = EditText(this).apply {
            alpha = 0f
            isFocusable = true
            isFocusableInTouchMode = true
            setSingleLine(true)
            inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
            imeOptions = EditorInfo.IME_ACTION_DONE or
                    EditorInfo.IME_FLAG_NO_EXTRACT_UI or
                    EditorInfo.IME_FLAG_NO_FULLSCREEN
            addTextChangedListener(object : TextWatcher {
                override fun beforeTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
                override fun onTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
                override fun afterTextChanged(s: Editable?) {
                    if (!mirroring) nativeSetText(s?.toString() ?: "")
                }
            })
            setOnEditorActionListener { _, actionId, event ->
                val enter = actionId == EditorInfo.IME_ACTION_DONE ||
                        (event != null && event.keyCode == KeyEvent.KEYCODE_ENTER &&
                                event.action == KeyEvent.ACTION_DOWN)
                if (enter) {
                    nativeReturn()
                    true
                } else false
            }
        }
        addContentView(edit, ViewGroup.LayoutParams(1, 1))
        input = edit
    }

    // Called from native code (game thread) when a text field gains focus.
    @Suppress("unused")
    fun showTextInput(current: String) {
        runOnUiThread {
            val edit = input ?: return@runOnUiThread
            mirroring = true
            edit.setText(current)
            edit.setSelection(edit.text.length)
            mirroring = false
            edit.requestFocus()
            val imm = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
            imm.showSoftInput(edit, InputMethodManager.SHOW_IMPLICIT)
        }
    }

    // Called from native code when the game field changed on its own side.
    @Suppress("unused")
    fun syncTextInput(current: String) {
        runOnUiThread {
            val edit = input ?: return@runOnUiThread
            if (edit.text.toString() == current) return@runOnUiThread
            mirroring = true
            edit.setText(current)
            edit.setSelection(edit.text.length)
            mirroring = false
        }
    }

    // Called from native code when no text field is focused any more.
    @Suppress("unused")
    fun hideTextInput() {
        runOnUiThread {
            val edit = input ?: return@runOnUiThread
            val imm = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
            imm.hideSoftInputFromWindow(edit.windowToken, 0)
            edit.clearFocus()
        }
    }

    // maple-offline: the game is about to leave the screen; Android may kill
    // the whole process later, so write every logged-in character now.
    override fun onStop() {
        super.onStop()
        Thread {
            try {
                Server.getInstance().saveAllCharacters()
            } catch (e: Exception) {
                android.util.Log.w("maple-offline", "background save failed", e)
            }
        }.start()
    }
}
