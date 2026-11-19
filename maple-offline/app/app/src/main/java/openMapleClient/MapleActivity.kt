package openMapleClient

import android.app.NativeActivity
import android.os.Bundle
import net.server.Server

class MapleActivity : NativeActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
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
