package dev.kasumi.app.media

import androidx.media3.common.Player
import androidx.media3.session.MediaSession

internal object KSMMediaSessionRegistry {
    @Volatile
    var player: Player? = null

    @Volatile
    var session: MediaSession? = null
}
