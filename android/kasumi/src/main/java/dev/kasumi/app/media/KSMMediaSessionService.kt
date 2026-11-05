package dev.kasumi.app.media

import androidx.media3.common.Player
import androidx.media3.exoplayer.ExoPlayer
import androidx.media3.session.MediaSession
import androidx.media3.session.MediaSessionService

class KSMMediaSessionService : MediaSessionService() {
    private var player: Player? = null
    private var mediaSession: MediaSession? = null
    private var ownsPlayer = false
    private var ownsSession = false

    override fun onCreate() {
        super.onCreate()
        val sharedPlayer = KSMMediaSessionRegistry.player
        val sharedSession = KSMMediaSessionRegistry.session
        if (sharedPlayer != null && sharedSession != null) {
            player = sharedPlayer
            mediaSession = sharedSession
        } else {
            val createdPlayer = ExoPlayer.Builder(this).build()
            player = createdPlayer
            mediaSession = MediaSession.Builder(this, createdPlayer).build()
            ownsPlayer = true
            ownsSession = true
        }
    }

    override fun onGetSession(
        controllerInfo: MediaSession.ControllerInfo
    ): MediaSession? {
        return mediaSession
    }

    override fun onDestroy() {
        if (ownsSession) {
            mediaSession?.release()
        }
        mediaSession = null
        if (ownsPlayer) {
            (player as? ExoPlayer)?.release()
        }
        player = null
        super.onDestroy()
    }
}
