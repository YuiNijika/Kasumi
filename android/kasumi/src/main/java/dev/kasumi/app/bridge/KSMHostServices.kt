package dev.kasumi.app.bridge

import dev.kasumi.app.internal.KSMVersionMetadata
import java.util.Locale

data class KSMOperatingSystemInfo(
    val platform: String,
    val architecture: String,
    val version: String,
    val locale: String,
    val timezone: String,
    val devMode: Boolean = false,
    val hostname: String? = null,
    val username: String? = null
)

data class KSMHttpHeader(val name: String, val value: String)

data class KSMHttpRequestSpec(
    val url: String,
    val method: String = "GET",
    val headers: List<KSMHttpHeader> = emptyList(),
    val body: String = "",
    val jarId: String? = null,
    val userAgent: String? = null,
    val referer: String? = null
)

data class KSMHttpResponse(
    val status: Int,
    val headers: List<KSMHttpHeader>,
    val body: String = "",
    val file: String? = null,
    val bytes: Long = 0
)

data class KSMHttpTransferSpec(
    val url: String,
    val method: String = "GET",
    val path: String,
    val headers: List<KSMHttpHeader> = emptyList(),
    val body: String = "",
    val jarId: String? = null,
    val userAgent: String? = null,
    val referer: String? = null,
    val overwrite: Boolean = false
)

data class KSMCookie(
    val name: String,
    val value: String,
    val domain: String = "",
    val path: String = "/",
    val secure: Boolean = true,
    val httpOnly: Boolean = false
)

data class KSMFileScope(
    val id: String,
    val path: String,
    val files: List<String>
)

data class KSMWalkEntry(
    val path: String,
    val name: String,
    val directory: Boolean,
    val size: Long,
    val modifiedAt: Long? = null
)

data class KSMMediaProbe(
    val title: String? = null,
    val artist: String? = null,
    val album: String? = null,
    val durationMs: Int? = null,
    val cover: String? = null,
    val lyrics: String? = null,
    val format: String? = null
)

data class KSMExternalTool(
    val path: String = "",
    val version: String = "",
    val valid: Boolean = false,
    val source: String? = null
)

data class KSMAudioDevice(
    val id: String,
    val name: String,
    val defaultDevice: Boolean = false
)

data class KSMAudioState(
    val playing: Boolean = false,
    val positionMs: Long = 0,
    val durationMs: Long? = null,
    val volume: Double = 1.0,
    val speed: Double = 1.0,
    val playback: String = "idle",
    val muted: Boolean = false,
    val outputMode: String = "shared",
    val deviceId: String? = null,
    val source: String? = null
)

data class KSMAudioProbe(
    val durationMs: Long? = null,
    val sampleRate: Int? = null,
    val channels: Int? = null,
    val codec: String? = null
)

data class KSMMediaSessionMetadata(
    val title: String = "",
    val artist: String = "",
    val album: String = "",
    val artwork: String? = null,
    val durationMs: Long? = null,
    val positionMs: Long? = null,
    val state: String = "stopped"
)

data class KSMWindowState(
    val visible: Boolean = true,
    val alwaysOnTop: Boolean = false,
    val transparent: Boolean = false,
    val x: Int = 0,
    val y: Int = 0,
    val width: Int = 0,
    val height: Int = 0
)

data class KSMTrayMenuItem(
    val id: String,
    val title: String = "",
    val enabled: Boolean = true
)

data class KSMTrayConfig(
    val icon: String? = null,
    val tooltip: String? = null,
    val items: List<KSMTrayMenuItem> = emptyList()
)

interface KSMHostServices {
    fun appVersion(): String = KSMVersionMetadata.VERSION
    fun appId(): String = ""
    fun language(): String = Locale.getDefault().toLanguageTag()
    fun showMessage(title: String, message: String): Boolean
    fun confirm(title: String, message: String): Boolean
    fun openFile(filters: List<String>): String?
    fun saveFile(suggestedName: String): String?
    fun readPickedText(path: String): String?
    fun writePickedText(path: String, content: String): Boolean
    fun pickedExists(path: String): Boolean
    fun pickedStat(path: String): KSMFileMetadata? = null
    fun removePicked(path: String): Boolean
    fun readBundledText(path: String): String?
    fun bundledExists(path: String): Boolean
    fun readBundledDirectory(path: String): List<KSMDirectoryEntry>? = null
    fun bundledStat(path: String): KSMFileMetadata? = null
    fun readClipboardText(): String?
    fun writeClipboardText(text: String): Boolean
    fun showNotification(title: String, body: String): Boolean
    fun requestPermission(permission: String): Boolean
    fun hasRuntimePermission(permission: String): Boolean = true
    fun closeWindow(): Boolean
    fun exitApp(): Boolean
    fun download(
        url: String,
        path: String,
        overwrite: Boolean,
        requestId: String? = null
    ): KSMHttpDownloadResult? = null
    fun isHttpRequestUrlAllowed(url: String): Boolean = false
    fun httpRequest(
        request: KSMHttpRequestSpec,
        requestId: String? = null
    ): KSMHttpResponse? = null
    fun startHttpTransfer(
        request: KSMHttpTransferSpec,
        requestId: String? = null
    ): String? = null
    fun cancelHttpTransfer(transferId: String): Boolean = false
    fun getCookies(jarId: String, url: String): List<KSMCookie>? = null
    fun setCookies(
        jarId: String,
        url: String,
        cookies: List<KSMCookie>
    ): Boolean = false
    fun clearCookies(jarId: String, url: String): Boolean = false
    fun openFolder(filters: List<String>): KSMFileScope? = null
    fun openFiles(filters: List<String>): KSMFileScope? = null
    fun walk(path: String, requestId: String? = null): List<KSMWalkEntry>? = null
    fun releaseScope(id: String): Boolean = false
    fun probeMedia(path: String): KSMMediaProbe? = null
    fun detectExternalTool(name: String): KSMExternalTool? = null
    fun validateExternalTool(path: String): KSMExternalTool? = null
    fun setExternalToolPath(name: String, path: String): Boolean = false
    fun pickExternalTool(name: String): String? = null
    fun isExternalUrlAllowed(url: String): Boolean = false
    fun openExternal(url: String): Boolean = false
    fun openPath(path: String): Boolean = false
    fun showInFolder(path: String): Boolean = false
    fun configureTray(config: KSMTrayConfig): Boolean = false
    fun installUpdate(
        packagePath: String,
        expectedHash: String,
        signature: String,
        requestId: String? = null
    ): Boolean = false
    fun takePendingDeepLink(): String? = null
    fun openDevtools(enabled: Boolean): Boolean = false
    fun pushDeepLink(url: String) {}
    fun setEventDispatcher(
        dispatcher: (event: String, payload: String) -> Unit
    ) {}
    fun audioDevices(): List<KSMAudioDevice>? = null
    fun setAudioDevice(id: String): Boolean = false
    fun setExclusiveAudioOutput(exclusive: Boolean): Boolean = false
    fun setAudioEqualizer(gains: List<Double>): Boolean = false
    fun setAudioSpeed(speed: Double): Boolean = false
    fun setAudioVolume(volume: Double): Boolean = false
    fun probeAudio(path: String): KSMAudioProbe? = null
    fun loadAudio(path: String): Boolean = false
    fun setAudioPlayback(playing: Boolean): Boolean = false
    fun seekAudio(positionMs: Long): Boolean = false
    fun audioState(): KSMAudioState? = null
    fun updateMediaSession(metadata: KSMMediaSessionMetadata): Boolean = false
    fun clearMediaSession(): Boolean = false
    fun createWindow(route: String, state: KSMWindowState): String? = null
    fun showWindowById(windowId: String, visible: Boolean): Boolean = false
    fun setWindowPosition(
        windowId: String,
        x: Int,
        y: Int
    ): Boolean = false
    fun setWindowAlwaysOnTop(
        windowId: String,
        enabled: Boolean
    ): Boolean = false
    fun setWindowTransparent(
        windowId: String,
        enabled: Boolean
    ): Boolean = false
    fun windowState(windowId: String): KSMWindowState? = null
    fun emitWindowEvent(
        windowId: String?,
        event: String,
        payload: String
    ): Boolean = false
    fun deviceInfo(): KSMDeviceInfo? = null
    fun operatingSystemInfo(): KSMOperatingSystemInfo? = null
    fun writeLog(level: String, target: String, message: String): Boolean = false
    fun registerSqlDriver(driver: KSMSqlDriver) {
        throw UnsupportedOperationException(
            "This host does not support SQL driver registration."
        )
    }
    fun sqlDrivers(): List<String> = emptyList()
    fun sqlOpen(driver: String, url: String): String? = null
    fun sqlQuery(
        connectionId: String,
        statement: String,
        params: List<Any?>,
        requestId: String? = null
    ): KSMSqlQueryResult? = null
    fun sqlExecute(
        connectionId: String,
        statement: String,
        params: List<Any?>,
        requestId: String? = null
    ): KSMSqlExecuteResult? = null
    fun sqlClose(connectionId: String): Boolean = false
    fun cancelRequest(requestId: String) {}
    fun isRequestCancelled(requestId: String): Boolean = false
    fun cancelRequestWithScope(requestId: String) {
        cancelRequest(requestId)
    }
}

class KSMHostException(
    val code: String,
    override val message: String
) : Exception(message)
