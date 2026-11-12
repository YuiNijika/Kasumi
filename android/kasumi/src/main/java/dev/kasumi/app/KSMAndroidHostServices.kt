package dev.kasumi.app

import android.Manifest
import android.app.Activity
import android.app.AlertDialog
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.ClipData
import android.content.ClipboardManager
import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.media.MediaMetadataRetriever
import android.graphics.Rect
import android.net.Uri
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.provider.DocumentsContract
import android.provider.OpenableColumns
import android.provider.Settings
import android.system.ErrnoException
import android.system.Os
import android.system.OsConstants
import android.util.Log
import androidx.core.content.ContextCompat
import androidx.documentfile.provider.DocumentFile
import androidx.media3.common.AudioAttributes
import androidx.media3.common.C
import androidx.media3.common.MediaItem
import androidx.media3.common.MediaMetadata
import androidx.media3.common.PlaybackException
import androidx.media3.common.Player
import androidx.media3.exoplayer.ExoPlayer
import androidx.media3.session.MediaSession
import dev.kasumi.app.bridge.KSMDirectoryEntry
import dev.kasumi.app.bridge.KSMCookie
import dev.kasumi.app.bridge.KSMDeviceInfo
import dev.kasumi.app.bridge.KSMAudioDevice
import dev.kasumi.app.bridge.KSMAudioProbe
import dev.kasumi.app.bridge.KSMAudioState
import dev.kasumi.app.bridge.KSMFileScope
import dev.kasumi.app.bridge.KSMHttpHeader
import dev.kasumi.app.bridge.KSMHttpRequestSpec
import dev.kasumi.app.bridge.KSMHttpResponse
import dev.kasumi.app.bridge.KSMHttpTransferSpec
import dev.kasumi.app.bridge.KSMMediaProbe
import dev.kasumi.app.bridge.KSMMediaSessionMetadata
import dev.kasumi.app.bridge.KSMTrayConfig
import dev.kasumi.app.bridge.KSMWindowState
import dev.kasumi.app.bridge.KSMWalkEntry
import dev.kasumi.app.bridge.KSMExternalTool
import dev.kasumi.app.bridge.KSMFileKind
import dev.kasumi.app.bridge.KSMFileMetadata
import dev.kasumi.app.bridge.KSMFileSystem
import dev.kasumi.app.bridge.KSMHostException
import dev.kasumi.app.bridge.KSMHostServices
import dev.kasumi.app.bridge.KSMHttpDownloadResult
import dev.kasumi.app.bridge.KSMHttpPolicy
import dev.kasumi.app.bridge.KSMOperatingSystemInfo
import dev.kasumi.app.bridge.KSMSqlDriver
import dev.kasumi.app.bridge.KSMSqlDriverRegistry
import dev.kasumi.app.bridge.KSMSqlExecuteResult
import dev.kasumi.app.bridge.KSMSqlQueryResult
import dev.kasumi.app.bridge.KSMSqliteDriver
import dev.kasumi.app.bridge.KSMVirtualPath
import dev.kasumi.app.bridge.KSMWebViewRuntime
import dev.kasumi.app.internal.KSMVersionMetadata
import dev.kasumi.app.media.KSMMediaSessionRegistry
import dev.kasumi.app.media.KSMMediaSessionService
import java.io.ByteArrayOutputStream
import java.io.BufferedOutputStream
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.net.SocketTimeoutException
import java.net.URL
import java.util.Locale
import java.util.TimeZone
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.CountDownLatch
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicReference
import javax.net.ssl.HttpsURLConnection

class KSMAndroidHostServices(
    private val activity: Activity,
    allowedExternalOrigins: Set<String> = emptySet(),
    private val fileSystem: KSMFileSystem? = null,
    private val appVersion: String = KSMVersionMetadata.VERSION,
    allowedHttpOrigins: Set<String> = emptySet()
) : KSMHostServices {
    companion object {
        const val REQUEST_OPEN_FILE = 1001
        const val REQUEST_SAVE_FILE = 1002
        const val REQUEST_NOTIFICATION_PERMISSION = 1003
        const val REQUEST_OPEN_FOLDER = 1004
        const val REQUEST_OPEN_FILES = 1005

        private const val CHANNEL_ID = "kasumi"
        private const val DIALOG_TIMEOUT_SECONDS = 60L
        private const val URI_TIMEOUT_SECONDS = 60L
        private const val DOWNLOAD_IO_TIMEOUT_MS = 15000
        private const val DOWNLOAD_TOTAL_TIMEOUT_MS = 120000L
        private const val MAX_DOWNLOAD_BYTES = 100L * 1024L * 1024L
        private const val MAX_REDIRECTS = 5
        private const val MAX_INLINE_RESPONSE_BYTES = 60 * 1024
        private const val MAX_HTTP_RESPONSE_BYTES = 100L * 1024L * 1024L
        private val nextNotificationId = AtomicInteger(1)
        private val nextPickedId = AtomicInteger(1)
    }

    private data class DownloadSession(
        val cancelled: AtomicBoolean = AtomicBoolean(false),
        val connection: AtomicReference<HttpsURLConnection?> = AtomicReference()
    ) {
        @Volatile
        var deadlineTask: ScheduledFuture<*>? = null
    }

    private data class PendingUri(
        val latch: CountDownLatch,
        val result: AtomicReference<String?> = AtomicReference(null)
    )

    private data class PendingUris(
        val latch: CountDownLatch,
        val result: AtomicReference<List<String>> = AtomicReference(emptyList())
    )

    private data class FileScopeState(
        val root: Uri,
        val files: List<Uri>
    )

    private data class TransferState(
        val cancelled: AtomicBoolean = AtomicBoolean(false),
        val connection: AtomicReference<HttpsURLConnection?> = AtomicReference()
    )

    private data class PendingPermission(
        val latch: CountDownLatch,
        val result: AtomicBoolean = AtomicBoolean(false)
    )

    private var pendingOpenFile: PendingUri? = null
    private var pendingSaveFile: PendingUri? = null
    private var pendingOpenFolder: PendingUri? = null
    private var pendingOpenFiles: PendingUris? = null
    private var pendingPermission: PendingPermission? = null
    private val pickedFiles = mutableMapOf<String, Uri>()
    private val allowedOrigins = KSMHttpPolicy.normalizeOrigins(allowedExternalOrigins)
    private val allowedHttpOrigins =
        KSMHttpPolicy.normalizeOrigins(allowedHttpOrigins)
    private val activeDownloads = ConcurrentHashMap<String, DownloadSession>()
    private val cookieJars = ConcurrentHashMap<String, MutableList<KSMCookie>>()
    private val fileScopes = ConcurrentHashMap<String, FileScopeState>()
    private val activeTransfers = ConcurrentHashMap<String, TransferState>()
    private val externalTools = ConcurrentHashMap<String, String>()
    private val pendingDeepLinks = ArrayDeque<String>()
    private val cancelledRequests = ConcurrentHashMap.newKeySet<String>()
    private val capabilityExecutor: ExecutorService =
        Executors.newCachedThreadPool { task ->
            Thread(task, "kasumi-capability").apply { isDaemon = true }
        }
    private var eventDispatcher: (String, String) -> Unit = { _, _ -> }
    private var nextCapabilityId = AtomicInteger(1)
    private val sqlDriverRegistry = KSMSqlDriverRegistry().apply {
        register(KSMSqliteDriver(fileSystem))
    }
    private val downloadWatchdog = Executors.newSingleThreadScheduledExecutor { task ->
        Thread(task, "kasumi-download-watchdog").apply {
            isDaemon = true
        }
    }
    private val audioPlayer: ExoPlayer = ExoPlayer.Builder(activity)
        .setAudioAttributes(
            AudioAttributes.Builder()
                .setUsage(C.USAGE_MEDIA)
                .setContentType(C.AUDIO_CONTENT_TYPE_MUSIC)
                .build(),
            true
        )
        .build()
    private val audioSession = MediaSession.Builder(activity, audioPlayer).build()
    private val audioHandler = Handler(Looper.getMainLooper())
    private var audioSource: String? = null
    private var audioVolume = 1.0
    private var audioSpeed = 1.0
    private var audioDeviceId: String? = null
    private var audioTickTask: ScheduledFuture<*>? = null

    init {
        KSMMediaSessionRegistry.player = audioPlayer
        KSMMediaSessionRegistry.session = audioSession
        audioPlayer.addListener(object : Player.Listener {
            override fun onIsPlayingChanged(isPlaying: Boolean) {
                dispatchAudioState()
                if (isPlaying) {
                    startAudioTicks()
                } else {
                    stopAudioTicks()
                }
            }

            override fun onPlaybackStateChanged(playbackState: Int) {
                if (playbackState == Player.STATE_ENDED) {
                    eventDispatcher(
                        "audio.ended",
                        org.json.JSONObject()
                            .put("source", audioSource ?: org.json.JSONObject.NULL)
                            .toString()
                    )
                    dispatchAudioState()
                    stopAudioTicks()
                }
            }

            override fun onPlayerError(error: PlaybackException) {
                eventDispatcher(
                    "audio.error",
                    org.json.JSONObject()
                        .put("code", "MEDIA_UNSUPPORTED")
                        .put("message", error.message ?: "Audio playback failed.")
                        .put("source", audioSource ?: org.json.JSONObject.NULL)
                        .toString()
                )
            }
        })
    }

    override fun appVersion(): String {
        if (appVersion.isNotBlank()) {
            return appVersion
        }
        return try {
            val versionName = if (
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
            ) {
                activity.packageManager.getPackageInfo(
                    activity.packageName,
                    PackageManager.PackageInfoFlags.of(0)
                ).versionName
            } else {
                @Suppress("DEPRECATION")
                activity.packageManager.getPackageInfo(
                    activity.packageName,
                    0
                ).versionName
            }
            versionName?.takeIf { it.isNotBlank() } ?: KSMVersionMetadata.VERSION
        } catch (_: Exception) {
            KSMVersionMetadata.VERSION
        }
    }

    override fun appId(): String {
        return try {
            activity.packageName
        } catch (_: Exception) {
            ""
        }
    }

    override fun language(): String {
        return Locale.getDefault().toLanguageTag()
    }

    override fun showMessage(title: String, message: String): Boolean {
        return showDialog(title, message, hasNegativeButton = false)
    }

    override fun confirm(title: String, message: String): Boolean {
        return showDialog(title, message, hasNegativeButton = true)
    }

    override fun openFile(filters: List<String>): String? {
        val pending = PendingUri(CountDownLatch(1))
        synchronized(this) {
            if (pendingOpenFile != null) {
                throw KSMHostException(
                    "BUSY",
                    "A file picker is already active."
                )
            }
            pendingOpenFile = pending
        }
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            if (filters.isNotEmpty()) {
                putExtra(Intent.EXTRA_MIME_TYPES, filters.toTypedArray())
            }
        }
        activity.runOnUiThread {
            try {
                activity.startActivityForResult(intent, REQUEST_OPEN_FILE)
            } catch (_: RuntimeException) {
                pending.latch.countDown()
            }
        }
        return awaitUri(pending, isOpen = true)?.let { registerPicked(Uri.parse(it)) }
    }

    override fun saveFile(suggestedName: String): String? {
        val pending = PendingUri(CountDownLatch(1))
        synchronized(this) {
            if (pendingSaveFile != null) {
                throw KSMHostException(
                    "BUSY",
                    "A file picker is already active."
                )
            }
            pendingSaveFile = pending
        }
        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            if (suggestedName.isNotEmpty()) {
                putExtra(Intent.EXTRA_TITLE, suggestedName)
            }
        }
        activity.runOnUiThread {
            try {
                activity.startActivityForResult(intent, REQUEST_SAVE_FILE)
            } catch (_: RuntimeException) {
                pending.latch.countDown()
            }
        }
        return awaitUri(pending, isOpen = false)?.let { registerPicked(Uri.parse(it)) }
    }

    override fun readPickedText(path: String): String? {
        val uri = resolvePicked(path) ?: return null
        return try {
            activity.contentResolver.openInputStream(uri)?.use { input ->
                readLimited(input)
            }
        } catch (_: RuntimeException) {
            null
        }
    }

    override fun writePickedText(path: String, content: String): Boolean {
        if (content.toByteArray(Charsets.UTF_8).size > KSMFileSystem.MAX_TEXT_BYTES) {
            return false
        }
        val uri = resolvePicked(path) ?: return false
        return try {
            activity.contentResolver.openOutputStream(uri, "wt")?.use { output ->
                output.write(content.toByteArray(Charsets.UTF_8))
                output.flush()
                true
            } ?: false
        } catch (_: RuntimeException) {
            false
        }
    }

    override fun pickedExists(path: String): Boolean {
        val uri = resolvePicked(path) ?: return false
        return try {
            activity.contentResolver.openFileDescriptor(uri, "r")?.use { true } ?: false
        } catch (_: RuntimeException) {
            false
        }
    }

    override fun pickedStat(path: String): KSMFileMetadata? {
        val uri = resolvePicked(path) ?: return null
        return try {
            var size = 0L
            activity.contentResolver.query(
                uri,
                arrayOf(OpenableColumns.SIZE),
                null,
                null,
                null
            )?.use { cursor ->
                if (cursor.moveToFirst()) {
                    val index = cursor.getColumnIndex(OpenableColumns.SIZE)
                    if (index >= 0 && !cursor.isNull(index)) {
                        size = cursor.getLong(index)
                    }
                }
            }
            if (size <= 0L) {
                size = activity.contentResolver
                    .openFileDescriptor(uri, "r")
                    ?.use { descriptor -> descriptor.statSize }
                    ?.takeIf { it >= 0L }
                    ?: 0L
            }
            KSMFileMetadata(KSMFileKind.FILE, size, null)
        } catch (_: RuntimeException) {
            null
        }
    }

    override fun removePicked(path: String): Boolean {
        val uri = resolvePicked(path) ?: return false
        val removed = try {
            DocumentsContract.deleteDocument(activity.contentResolver, uri) ||
                activity.contentResolver.delete(uri, null, null) > 0
        } catch (_: RuntimeException) {
            false
        }
        if (removed) {
            synchronized(this) {
                pickedFiles.remove(path, uri)
            }
        }
        return removed
    }

    override fun readBundledText(path: String): String? {
        val assetPath = KSMVirtualPath.bundledAssetPath(path) ?: return null
        return try {
            activity.assets.open(assetPath).use { input -> readLimited(input) }
        } catch (_: Exception) {
            null
        }
    }

    override fun bundledExists(path: String): Boolean {
        val assetPath = KSMVirtualPath.bundledAssetPath(path) ?: return false
        return try {
            activity.assets.open(assetPath).use { true }
        } catch (_: Exception) {
            false
        }
    }

    override fun readBundledDirectory(
        path: String
    ): List<KSMDirectoryEntry>? {
        val directoryPath = KSMVirtualPath.bundledAssetDirectoryPath(path)
            ?: return null
        val metadata = bundledStat(path) ?: return null
        if (metadata.kind != KSMFileKind.DIRECTORY) {
            return null
        }
        val names = try {
            activity.assets.list(directoryPath)?.toList()
        } catch (_: Exception) {
            null
        } ?: return null
        val parent = KSMVirtualPath.normalizeDirectory(path)
        return names.mapNotNull { name ->
            val childPath = "$parent/$name"
            val child = bundledStat(childPath) ?: return@mapNotNull null
            KSMDirectoryEntry(
                name,
                childPath,
                child.kind,
                child.size,
                child.modifiedAt
            )
        }.sortedBy { it.name }
    }

    override fun bundledStat(path: String): KSMFileMetadata? {
        val normalized = KSMVirtualPath.normalizeDirectory(path)
        val filePath = KSMVirtualPath.bundledAssetPath(normalized)
        if (filePath != null) {
            val size = bundledFileSize(filePath)
            if (size != null) {
                return KSMFileMetadata(KSMFileKind.FILE, size, null)
            }
        }
        val directoryPath = KSMVirtualPath.bundledAssetDirectoryPath(normalized)
            ?: return null
        if (!bundledDirectoryExists(directoryPath)) {
            return null
        }
        return KSMFileMetadata(KSMFileKind.DIRECTORY, 0, null)
    }

    override fun readClipboardText(): String? {
        val clipboard = activity.getSystemService(Context.CLIPBOARD_SERVICE)
            as? ClipboardManager
            ?: return null
        return clipboard.primaryClip
            ?.takeIf { it.itemCount > 0 }
            ?.getItemAt(0)
            ?.coerceToText(activity)
            ?.toString()
    }

    override fun writeClipboardText(text: String): Boolean {
        val clipboard = activity.getSystemService(Context.CLIPBOARD_SERVICE)
            as? ClipboardManager
            ?: return false
        return try {
            clipboard.setPrimaryClip(ClipData.newPlainText("Kasumi", text))
            true
        } catch (_: RuntimeException) {
            false
        }
    }

    override fun showNotification(title: String, body: String): Boolean {
        if (!hasNotificationPermission()) {
            return false
        }
        val manager = activity.getSystemService(Context.NOTIFICATION_SERVICE)
            as? NotificationManager
            ?: return false
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            manager.createNotificationChannel(
                NotificationChannel(
                    CHANNEL_ID,
                    "Kasumi",
                    NotificationManager.IMPORTANCE_DEFAULT
                )
            )
        }
        val builder = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Notification.Builder(activity, CHANNEL_ID)
        } else {
            Notification.Builder(activity)
        }
        builder
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setContentTitle(title)
            .setContentText(body)
            .setAutoCancel(true)
        return try {
            manager.notify(nextNotificationId.getAndIncrement(), builder.build())
            true
        } catch (_: RuntimeException) {
            false
        }
    }

    override fun requestPermission(permission: String): Boolean {
        if (permission != "notification.show") {
            return true
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            return true
        }
        if (hasNotificationPermission()) {
            return true
        }
        val pending = PendingPermission(CountDownLatch(1))
        synchronized(this) {
            if (pendingPermission != null) {
                return false
            }
            pendingPermission = pending
        }
        activity.runOnUiThread {
            try {
                activity.requestPermissions(
                    arrayOf(Manifest.permission.POST_NOTIFICATIONS),
                    REQUEST_NOTIFICATION_PERMISSION
                )
            } catch (_: RuntimeException) {
                pending.latch.countDown()
            }
        }
        val completed = try {
            pending.latch.await(URI_TIMEOUT_SECONDS, TimeUnit.SECONDS)
        } catch (_: InterruptedException) {
            Thread.currentThread().interrupt()
            false
        }
        synchronized(this) {
            pendingPermission = null
        }
        return completed && pending.result.get()
    }

    override fun hasRuntimePermission(permission: String): Boolean {
        return permission != "notification.show" || hasNotificationPermission()
    }

    override fun closeWindow(): Boolean {
        activity.runOnUiThread { activity.finish() }
        return true
    }

    override fun exitApp(): Boolean {
        activity.runOnUiThread { activity.finishAffinity() }
        return true
    }

    override fun download(
        url: String,
        path: String,
        overwrite: Boolean,
        requestId: String?
    ): KSMHttpDownloadResult? = downloadInternal(
        url,
        path,
        overwrite,
        requestId,
        emptyList(),
        emptyList(),
        false
    )

    override fun downloadTrusted(
        url: String,
        path: String,
        overwrite: Boolean,
        requestId: String?,
        headers: List<KSMHttpHeader>,
        allowedHosts: List<String>
    ): KSMHttpDownloadResult? = downloadInternal(
        url,
        path,
        overwrite,
        requestId,
        headers,
        allowedHosts,
        true
    )

    private fun downloadInternal(
        url: String,
        path: String,
        overwrite: Boolean,
        requestId: String?,
        headers: List<KSMHttpHeader>,
        allowedHosts: List<String>,
        trusted: Boolean
    ): KSMHttpDownloadResult? {
        val target = fileSystem?.resolveWritableFile(path)
            ?: throw KSMHostException(
                "PERMISSION_DENIED",
                "path is outside the writable sandbox."
            )
        if (target.isDirectory) {
            throw KSMHostException("INVALID_ARGS", "path must name a file.")
        }
        if (target.exists() && !overwrite) {
            throw KSMHostException("IO_ERROR", "The target already exists.")
        }
        val parent = target.parentFile
            ?: throw KSMHostException("IO_ERROR", "The target directory is invalid.")
        if (!parent.exists() && !parent.mkdirs()) {
            throw KSMHostException("IO_ERROR", "The target directory could not be created.")
        }
        var current = KSMHttpPolicy.parseHttpUrl(url, httpsOnly = true)
            ?: throw KSMHostException("INVALID_ARGS", "url must use HTTPS.")
        fun hostAllowed(candidate: URL): Boolean {
            if (!trusted) {
                return KSMHttpPolicy.isAllowedDownloadUrl(
                    candidate.toString(),
                    allowedHttpOrigins
                )
            }
            val host = candidate.host.lowercase(Locale.ROOT)
            return allowedHosts.any { entry ->
                val suffix = entry.lowercase(Locale.ROOT)
                host == suffix || host.endsWith(".$suffix")
            }
        }
        headers.forEach { header ->
            if (
                header.name.isEmpty() ||
                header.name.contains('\r') ||
                header.name.contains('\n') ||
                header.value.contains('\r') ||
                header.value.contains('\n')
            ) {
                throw KSMHostException(
                    "INVALID_ARGS",
                    "The request headers are invalid."
                )
            }
        }
        if (!hostAllowed(current)) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The URL origin is not allowlisted for downloads."
            )
        }
        if (!KSMHttpPolicy.isPublicHost(current.host)) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The download host is not a public network address."
            )
        }
        var redirects = 0
        val session = DownloadSession()
        val downloadKey = requestId ?: "anonymous-${System.nanoTime()}"
        if (activeDownloads.putIfAbsent(downloadKey, session) != null) {
            throw KSMHostException(
                "BUSY",
                "A download with this request ID is already running."
            )
        }
        val part = File(parent, ".${target.name}.${System.nanoTime()}.part")
        val deadlineNanos =
            System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(DOWNLOAD_TOTAL_TIMEOUT_MS)
        session.deadlineTask = downloadWatchdog.schedule(
            {
                session.cancelled.set(true)
                session.connection.getAndSet(null)?.disconnect()
            },
            DOWNLOAD_TOTAL_TIMEOUT_MS,
            TimeUnit.MILLISECONDS
        )
        return try {
            while (true) {
                if (session.cancelled.get() || Thread.currentThread().isInterrupted) {
                    throw downloadCancellation(deadlineNanos)
                }
                val remainingMs = TimeUnit.NANOSECONDS.toMillis(
                    deadlineNanos - System.nanoTime()
                )
                if (remainingMs <= 0) {
                    throw KSMHostException("TIMEOUT", "The download timed out.")
                }
                val opened = current.openConnection() as? HttpsURLConnection
                    ?: throw KSMHostException("IO_ERROR", "The HTTPS connection failed.")
                session.connection.set(opened)
                if (session.cancelled.get()) {
                    throw downloadCancellation(deadlineNanos)
                }
                opened.instanceFollowRedirects = false
                opened.useCaches = false
                opened.connectTimeout = minOf(
                    DOWNLOAD_IO_TIMEOUT_MS.toLong(),
                    remainingMs
                ).toInt()
                opened.readTimeout = minOf(
                    DOWNLOAD_IO_TIMEOUT_MS.toLong(),
                    remainingMs
                ).toInt()
                opened.requestMethod = "GET"
                opened.setRequestProperty("Accept", "*/*")
                headers.forEach { header ->
                    opened.setRequestProperty(header.name, header.value)
                }
                try {
                    val status = opened.responseCode
                    if (session.cancelled.get()) {
                        throw downloadCancellation(deadlineNanos)
                    }
                    if (status in 300..399) {
                        if (redirects >= MAX_REDIRECTS) {
                            throw KSMHostException(
                                "IO_ERROR",
                                "The download exceeded the redirect limit."
                            )
                        }
                        val location = opened.getHeaderField("Location")
                            ?: throw KSMHostException(
                                "IO_ERROR",
                                "The redirect did not include a location."
                            )
                        current = KSMHttpPolicy.resolveHttpsRedirect(current, location)
                            ?: throw KSMHostException(
                                "IO_ERROR",
                                "The redirect did not use HTTPS."
                            )
                        if (
                            !hostAllowed(current) ||
                            !KSMHttpPolicy.isPublicHost(current.host)
                        ) {
                            throw KSMHostException(
                                "PERMISSION_DENIED",
                                "The redirect target is not allowlisted for downloads."
                            )
                        }
                        redirects += 1
                        continue
                    }
                    if (status !in 200..299) {
                        throw KSMHostException(
                            "IO_ERROR",
                            "The server returned HTTP $status."
                        )
                    }
                    val declaredLength = opened.contentLengthLong
                    if (declaredLength > MAX_DOWNLOAD_BYTES) {
                        throw KSMHostException(
                            "IO_ERROR",
                            "The download exceeds the maximum size."
                        )
                    }
                    val bytes = opened.inputStream.use { input ->
                        copyLimited(
                            input,
                            part,
                            MAX_DOWNLOAD_BYTES,
                            session,
                            deadlineNanos
                        )
                    }
                    if (target.exists() && !overwrite) {
                        throw KSMHostException("IO_ERROR", "The target already exists.")
                    }
                    if (!commitDownload(part, target, overwrite)) {
                        throw KSMHostException(
                            "IO_ERROR",
                            "The downloaded file could not be committed."
                        )
                    }
                    return KSMHttpDownloadResult(path, bytes)
                } finally {
                    opened.disconnect()
                    session.connection.compareAndSet(opened, null)
                }
            }
                throw KSMHostException("IO_ERROR", "The download ended unexpectedly.")
        } catch (_: InterruptedException) {
            Thread.currentThread().interrupt()
            throw downloadCancellation(deadlineNanos)
        } catch (error: SocketTimeoutException) {
            if (session.cancelled.get()) {
                throw downloadCancellation(deadlineNanos)
            }
            if (System.nanoTime() >= deadlineNanos) {
                throw KSMHostException("TIMEOUT", "The download timed out.")
            }
            throw KSMHostException(
                "IO_ERROR",
                error.message ?: "The connection timed out."
            )
        } catch (error: IOException) {
            if (session.cancelled.get()) {
                throw downloadCancellation(deadlineNanos)
            }
            throw KSMHostException("IO_ERROR", error.message ?: "The download failed.")
        } finally {
            session.connection.set(null)
            session.deadlineTask?.cancel(false)
            activeDownloads.remove(downloadKey, session)
            part.delete()
        }
    }

    override fun cancelRequest(requestId: String) {
        activeDownloads[requestId]?.let(::cancelDownload)
        activeTransfers.entries.filter {
            it.key == requestId || it.key.endsWith(":$requestId")
        }.forEach { (_, state) -> cancelTransfer(state) }
        sqlDriverRegistry.cancel(requestId)
    }

    override fun cancelRequestWithScope(requestId: String) {
        cancelledRequests.add(requestId)
        cancelRequest(requestId)
    }

    override fun isRequestCancelled(requestId: String): Boolean {
        return cancelledRequests.contains(requestId)
    }

    fun cancelAllDownloads() {
        activeDownloads.values.forEach(::cancelDownload)
    }

    fun cancelDownloads(windowId: String) {
        val prefix = "$windowId:"
        activeDownloads.entries
            .filter { it.key.startsWith(prefix) }
            .forEach { (_, session) -> cancelDownload(session) }
    }

    fun shutdown() {
        cancelAllDownloads()
        activeTransfers.values.forEach(::cancelTransfer)
        activeDownloads.clear()
        activeTransfers.clear()
        cancelledRequests.clear()
        stopAudioTicks()
        stopMediaSessionService()
        runOnAudioThread {
            if (KSMMediaSessionRegistry.session === audioSession) {
                KSMMediaSessionRegistry.session = null
            }
            if (KSMMediaSessionRegistry.player === audioPlayer) {
                KSMMediaSessionRegistry.player = null
            }
            audioSession.release()
            audioPlayer.release()
        }
        capabilityExecutor.shutdownNow()
        downloadWatchdog.shutdownNow()
        sqlDriverRegistry.closeAll()
        synchronized(this) {
            pendingOpenFile?.latch?.countDown()
            pendingSaveFile?.latch?.countDown()
            pendingPermission?.latch?.countDown()
            pendingOpenFile = null
            pendingSaveFile = null
            pendingPermission = null
            pickedFiles.clear()
            pendingDeepLinks.clear()
        }
    }

    override fun setEventDispatcher(
        dispatcher: (event: String, payload: String) -> Unit
    ) {
        eventDispatcher = dispatcher
    }

    override fun pushDeepLink(url: String) {
        synchronized(this) {
            pendingDeepLinks.addLast(url)
        }
        eventDispatcher(
            "app.deepLink.received",
            org.json.JSONObject().put("url", url).toString()
        )
    }

    override fun isHttpRequestUrlAllowed(url: String): Boolean {
        return KSMHttpPolicy.isAllowedDownloadUrl(url, allowedHttpOrigins)
    }

    override fun httpRequest(
        request: KSMHttpRequestSpec,
        requestId: String?
    ): KSMHttpResponse? {
        val initial = KSMHttpPolicy.parseHttpUrl(request.url, httpsOnly = true)
            ?: throw KSMHostException("INVALID_ARGS", "url must use HTTPS.")
        if (
            request.method != "GET" && request.method != "POST" ||
            request.body.toByteArray(Charsets.UTF_8).size > MAX_INLINE_RESPONSE_BYTES
        ) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The request is not allowed."
            )
        }
        val jarId = request.jarId
        var current = initial
        var redirects = 0
        val deadline = System.nanoTime() +
            TimeUnit.MILLISECONDS.toNanos(DOWNLOAD_TOTAL_TIMEOUT_MS)
        while (true) {
            if (System.nanoTime() >= deadline) {
                throw KSMHostException("TIMEOUT", "The request timed out.")
            }
            if (requestId?.let(::isRequestCancelled) == true) {
                throw KSMHostException("CANCELLED", "The request was cancelled.")
            }
            if (
                !KSMHttpPolicy.isAllowedDownloadUrl(
                    current.toString(),
                    allowedHttpOrigins
                ) ||
                !KSMHttpPolicy.isPublicHost(current.host)
            ) {
                throw KSMHostException(
                    "PERMISSION_DENIED",
                    "The request origin is not allowlisted."
                )
            }
            val connection = current.openConnection() as? HttpsURLConnection
                ?: throw KSMHostException(
                    "IO_ERROR",
                    "The HTTPS connection failed."
                )
            connection.instanceFollowRedirects = false
            connection.connectTimeout = DOWNLOAD_IO_TIMEOUT_MS
            connection.readTimeout = DOWNLOAD_IO_TIMEOUT_MS
            connection.requestMethod = request.method
            connection.doInput = true
            if (request.method == "POST") {
                connection.doOutput = true
            }
            fun validateHeader(value: String) {
                if (value.contains('\r') || value.contains('\n')) {
                    throw KSMHostException(
                        "INVALID_ARGS",
                        "The request headers are invalid."
                    )
                }
            }
            request.userAgent?.let {
                validateHeader(it)
                connection.setRequestProperty("User-Agent", it)
            }
            request.referer?.let {
                validateHeader(it)
                connection.setRequestProperty("Referer", it)
            }
            val blockedHeaders = setOf(
                "cookie",
                "host",
                "proxy-authorization",
                "set-cookie",
                "user-agent",
                "referer"
            )
            request.headers.forEach {
                validateHeader(it.value)
                if (it.name.lowercase(Locale.ROOT) in blockedHeaders) {
                    throw KSMHostException(
                        "PERMISSION_DENIED",
                        "The header is controlled by the host."
                    )
                }
                connection.setRequestProperty(it.name, it.value)
            }
            val jar = jarId?.let {
                cookieJars.getOrPut(it) { mutableListOf() }
            }
            val jarCookie = jar?.let {
                synchronized(it) {
                    it.joinToString("; ") { cookie ->
                        "${cookie.name}=${cookie.value}"
                    }
                }
            } ?: ""
            val explicitCookie = request.cookieHeader.orEmpty()
            validateHeader(explicitCookie)
            val cookieHeader = listOf(jarCookie, explicitCookie)
                .filter { it.isNotEmpty() }
                .joinToString("; ")
            if (cookieHeader.isNotEmpty()) {
                connection.setRequestProperty("Cookie", cookieHeader)
            }
            try {
                if (request.method == "POST") {
                    BufferedOutputStream(connection.outputStream).use { output ->
                        output.write(request.body.toByteArray(Charsets.UTF_8))
                    }
                }
                val status = connection.responseCode
                if (status in 300..399) {
                    if (redirects >= MAX_REDIRECTS) {
                        throw KSMHostException(
                            "IO_ERROR",
                            "The request exceeded the redirect limit."
                        )
                    }
                    val location = connection.getHeaderField("Location")
                        ?: throw KSMHostException(
                            "IO_ERROR",
                            "The redirect did not include a location."
                        )
                    current = KSMHttpPolicy.resolveHttpsRedirect(current, location)
                        ?: throw KSMHostException(
                            "IO_ERROR",
                            "The redirect did not use HTTPS."
                        )
                    redirects += 1
                    continue
                }
                if (jar != null) {
                    connection.headerFields.forEach { (name, values) ->
                        if (name?.lowercase(Locale.ROOT) == "set-cookie") {
                            values.forEach { value ->
                                val pair = value.substringBefore(';')
                                val separator = pair.indexOf('=')
                                if (separator > 0) {
                                    val cookie = KSMCookie(
                                        pair.substring(0, separator),
                                        pair.substring(separator + 1),
                                        current.host,
                                        current.path.ifEmpty { "/" },
                                        value.contains(
                                            "secure",
                                            ignoreCase = true
                                        ),
                                        value.contains(
                                            "httponly",
                                            ignoreCase = true
                                        )
                                    )
                                    synchronized(jar) {
                                        jar.removeAll {
                                            it.name == cookie.name &&
                                                it.domain == cookie.domain
                                        }
                                        jar.add(cookie)
                                    }
                                }
                            }
                        }
                    }
                }
                val contentType = connection.contentType.orEmpty()
                val headers = listOf(KSMHttpHeader("content-type", contentType))
                val stagedPath = "app://tmp/http-${System.nanoTime()}.bin"
                val target = fileSystem?.resolveWritableFile(stagedPath)
                val body = ByteArrayOutputStream()
                var total = 0L
                var stagedOutput: OutputStream? = null
                val buffer = ByteArray(8192)
                val input = if (status in 200..299) {
                    connection.inputStream
                } else {
                    connection.errorStream
                }
                try {
                    input?.use { source ->
                        while (true) {
                            if (
                                requestId?.let(::isRequestCancelled) == true ||
                                System.nanoTime() >= deadline
                            ) {
                                throw KSMHostException(
                                    "CANCELLED",
                                    "The request was cancelled."
                                )
                            }
                            val read = source.read(buffer)
                            if (read < 0) {
                                break
                            }
                            total += read
                            if (total > MAX_HTTP_RESPONSE_BYTES) {
                                throw KSMHostException(
                                    "RESOURCE_LIMIT",
                                    "The response exceeds the maximum size."
                                )
                            }
                            if (
                                body.size() + read <=
                                MAX_INLINE_RESPONSE_BYTES
                            ) {
                                body.write(buffer, 0, read)
                            } else {
                                if (stagedOutput == null) {
                                    val file = target ?: throw KSMHostException(
                                        "NOT_SUPPORTED",
                                        "Large HTTP responses require a writable sandbox."
                                    )
                                    val parent = file.parentFile
                                        ?: throw KSMHostException(
                                            "IO_ERROR",
                                            "The response directory is invalid."
                                        )
                                    if (!parent.exists() && !parent.mkdirs()) {
                                        throw KSMHostException(
                                            "IO_ERROR",
                                            "The response directory could not be created."
                                        )
                                    }
                                    stagedOutput = file.outputStream()
                                    stagedOutput?.write(body.toByteArray())
                                    body.reset()
                                }
                                stagedOutput?.write(buffer, 0, read)
                            }
                        }
                    }
                } finally {
                    stagedOutput?.close()
                }
                val staged = if (stagedOutput != null) {
                    stagedPath
                } else {
                    null
                }
                return KSMHttpResponse(
                    status,
                    headers,
                    if (total <= MAX_INLINE_RESPONSE_BYTES) {
                        body.toString(Charsets.UTF_8.name())
                    } else {
                        ""
                    },
                    staged,
                    total
                )
            } catch (error: IOException) {
                if (requestId?.let(::isRequestCancelled) == true) {
                    throw KSMHostException(
                        "CANCELLED",
                        "The request was cancelled."
                    )
                }
                throw KSMHostException(
                    "IO_ERROR",
                    error.message ?: "The request failed."
                )
            } finally {
                connection.disconnect()
            }
        }
    }

    override fun startHttpTransfer(
        request: KSMHttpTransferSpec,
        requestId: String?
    ): String? {
        val transferId = "transfer-${nextCapabilityId.getAndIncrement()}"
        val state = TransferState()
        activeTransfers[transferId] = state
        capabilityExecutor.execute {
            try {
                val response = httpRequest(
                    KSMHttpRequestSpec(
                        request.url,
                        request.method,
                        request.headers,
                        request.body,
                        request.jarId,
                        request.userAgent,
                        request.referer,
                        cookieHeader = request.cookieHeader
                    ),
                    requestId
                ) ?: throw KSMHostException(
                    "NOT_SUPPORTED",
                    "The transfer is not implemented on this host."
                )
                if (state.cancelled.get()) {
                    throw KSMHostException(
                        "CANCELLED",
                        "The transfer was cancelled."
                    )
                }
                eventDispatcher(
                    "http.transfer.completed",
                    org.json.JSONObject()
                        .put("transferId", transferId)
                        .put("status", response.status)
                        .put("bytes", response.bytes)
                        .put("file", response.file.orEmpty())
                        .toString()
                )
            } catch (error: Exception) {
                eventDispatcher(
                    "http.transfer.failed",
                    org.json.JSONObject()
                        .put("transferId", transferId)
                        .put("message", error.message.orEmpty())
                        .toString()
                )
            } finally {
                activeTransfers.remove(transferId, state)
            }
        }
        return transferId
    }

    override fun cancelHttpTransfer(transferId: String): Boolean {
        val state = activeTransfers[transferId] ?: return false
        cancelTransfer(state)
        return true
    }

    override fun getCookies(
        jarId: String,
        url: String
    ): List<KSMCookie>? {
        if (!isHttpRequestUrlAllowed(url) || jarId.isEmpty()) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The URL origin is not allowlisted."
            )
        }
        val jar = cookieJars[jarId] ?: return emptyList()
        return synchronized(jar) { jar.toList() }
    }

    override fun setCookies(
        jarId: String,
        url: String,
        cookies: List<KSMCookie>
    ): Boolean {
        if (!isHttpRequestUrlAllowed(url) || jarId.isEmpty()) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The URL origin is not allowlisted."
            )
        }
        val jar = cookieJars.getOrPut(jarId) { mutableListOf() }
        synchronized(jar) {
            jar.clear()
            jar.addAll(cookies)
        }
        return true
    }

    override fun clearCookies(jarId: String, url: String): Boolean {
        if (!isHttpRequestUrlAllowed(url) || jarId.isEmpty()) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The URL origin is not allowlisted."
            )
        }
        cookieJars.remove(jarId)
        return true
    }

    override fun openFolder(filters: List<String>): KSMFileScope? {
        val pending = PendingUri(CountDownLatch(1))
        synchronized(this) {
            if (pendingOpenFolder != null) {
                throw KSMHostException("BUSY", "A folder picker is already active.")
            }
            pendingOpenFolder = pending
        }
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).apply {
            if (filters.isNotEmpty()) {
                putExtra(Intent.EXTRA_MIME_TYPES, filters.toTypedArray())
            }
        }
        activity.runOnUiThread {
            try {
                activity.startActivityForResult(intent, REQUEST_OPEN_FOLDER)
            } catch (_: RuntimeException) {
                pending.latch.countDown()
            }
        }
        val uri = awaitUris(listOf(pending))?.firstOrNull()
            ?.let(Uri::parse) ?: return null
        takePersistablePermission(
            uri,
            Intent.FLAG_GRANT_READ_URI_PERMISSION or
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION
        )
        val id = "scope-${nextCapabilityId.getAndIncrement()}"
        fileScopes[id] = FileScopeState(uri, emptyList())
        return KSMFileScope(id, "app://scope/$id", emptyList())
    }

    override fun openFiles(filters: List<String>): KSMFileScope? {
        val pending = PendingUris(CountDownLatch(1))
        synchronized(this) {
            if (pendingOpenFiles != null) {
                throw KSMHostException("BUSY", "A file picker is already active.")
            }
            pendingOpenFiles = pending
        }
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
            if (filters.isNotEmpty()) {
                putExtra(Intent.EXTRA_MIME_TYPES, filters.toTypedArray())
            }
        }
        activity.runOnUiThread {
            try {
                activity.startActivityForResult(intent, REQUEST_OPEN_FILES)
            } catch (_: RuntimeException) {
                pending.latch.countDown()
            }
        }
        val completed = try {
            pending.latch.await(URI_TIMEOUT_SECONDS, TimeUnit.SECONDS)
        } catch (_: InterruptedException) {
            Thread.currentThread().interrupt()
            false
        }
        synchronized(this) { pendingOpenFiles = null }
        val uris = if (completed) {
            pending.result.get().map(Uri::parse)
        } else {
            emptyList()
        }
        if (uris.isEmpty()) {
            return null
        }
        uris.forEach {
            takePersistablePermission(it, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        val id = "scope-${nextCapabilityId.getAndIncrement()}"
        fileScopes[id] = FileScopeState(uris.first(), uris)
        val root = "app://scope/$id"
        return KSMFileScope(
            id,
            root,
            uris.indices.map { "$root/file-$it" }
        )
    }

    override fun walk(
        path: String,
        requestId: String?
    ): List<KSMWalkEntry>? {
        if (
            !path.startsWith("app://scope/") ||
            requestId?.let(::isRequestCancelled) == true
        ) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The path is outside an active file scope."
            )
        }
        val id = path.removePrefix("app://scope/").substringBefore('/')
        val scope = fileScopes[id] ?: return null
        if (scope.files.isNotEmpty()) {
            return scope.files.mapIndexed { index, uri ->
                KSMWalkEntry(
                    "app://scope/$id/file-$index",
                    documentName(uri),
                    false,
                    documentSize(uri),
                    null
                )
            }
        }
        val root = DocumentFile.fromTreeUri(activity, scope.root) ?: return null
        val entries = mutableListOf<KSMWalkEntry>()
        walkDocument(root, "app://scope/$id", entries, requestId)
        return entries
    }

    override fun releaseScope(id: String): Boolean {
        return fileScopes.remove(id) != null
    }

    override fun probeMedia(path: String): KSMMediaProbe? {
        val uri = resolveAudioUri(path) ?: return null
        val retriever = MediaMetadataRetriever()
        return try {
            if (uri.scheme == "content") {
                retriever.setDataSource(activity, uri)
            } else {
                retriever.setDataSource(uri.toString(), emptyMap())
            }
            KSMMediaProbe(
                title = retriever.extractMetadata(
                    MediaMetadataRetriever.METADATA_KEY_TITLE
                ),
                artist = retriever.extractMetadata(
                    MediaMetadataRetriever.METADATA_KEY_ARTIST
                ),
                album = retriever.extractMetadata(
                    MediaMetadataRetriever.METADATA_KEY_ALBUM
                ),
                durationMs = retriever.extractMetadata(
                    MediaMetadataRetriever.METADATA_KEY_DURATION
                )?.toIntOrNull(),
                format = uri.lastPathSegment
                    ?.substringAfterLast('.', "")
                    ?.takeIf { it.isNotEmpty() }
            )
        } catch (_: RuntimeException) {
            null
        } finally {
            retriever.release()
        }
    }

    override fun detectExternalTool(name: String): KSMExternalTool? {
        return if (name != "ffmpeg") {
            KSMExternalTool("", "", false)
        } else {
            null
        }
    }

    override fun validateExternalTool(path: String): KSMExternalTool? = null

    override fun setExternalToolPath(name: String, path: String): Boolean {
        if (name.isEmpty() || path.isEmpty()) {
            return false
        }
        externalTools[name] = path
        return true
    }

    override fun pickExternalTool(name: String): String? = null

    override fun configureTray(config: KSMTrayConfig): Boolean = false

    override fun installUpdate(
        packagePath: String,
        expectedHash: String,
        signature: String,
        requestId: String?
    ): Boolean = false

    override fun takePendingDeepLink(): String? {
        return synchronized(this) {
            pendingDeepLinks.removeFirstOrNull()
        }
    }

    override fun openDevtools(enabled: Boolean): Boolean {
        activity.runOnUiThread {
            KSMWebViewRuntime.setDebuggingEnabled(enabled)
        }
        return true
    }

    override fun audioDevices(): List<KSMAudioDevice>? {
        val manager = activity.getSystemService(AudioManager::class.java)
            ?: return null
        val devices = manager.getDevices(AudioManager.GET_DEVICES_OUTPUTS)
        return devices.map { device ->
            KSMAudioDevice(
                id = device.id.toString(),
                name = device.productName?.toString()
                    ?.takeIf { it.isNotBlank() }
                    ?: device.type.toString(),
                defaultDevice = device.type == AudioDeviceInfo.TYPE_BUILTIN_SPEAKER
            )
        }
    }

    override fun setAudioDevice(id: String): Boolean {
        if (id.isEmpty()) {
            return false
        }
        val manager = activity.getSystemService(AudioManager::class.java)
            ?: return false
        val device = if (id == "default") {
            null
        } else {
            manager.getDevices(AudioManager.GET_DEVICES_OUTPUTS)
                .firstOrNull { it.id.toString() == id }
                ?: return false
        }
        audioDeviceId = id.takeUnless { it == "default" }
        runOnAudioThread {
            audioPlayer.setPreferredAudioDevice(device)
        }
        dispatchAudioState()
        return true
    }
    override fun setExclusiveAudioOutput(exclusive: Boolean): Boolean = false
    override fun setAudioEqualizer(gains: List<Double>): Boolean = false
    override fun setAudioSpeed(speed: Double): Boolean {
        if (!speed.isFinite() || speed < 0.25 || speed > 4.0) {
            throw KSMHostException(
                "INVALID_ARGS",
                "speed must be between 0.25 and 4."
            )
        }
        audioSpeed = speed
        runOnAudioThread {
            audioPlayer.setPlaybackSpeed(speed.toFloat())
        }
        dispatchAudioState()
        return true
    }

    override fun setAudioVolume(volume: Double): Boolean {
        if (!volume.isFinite() || volume < 0.0 || volume > 1.0) {
            throw KSMHostException(
                "INVALID_ARGS",
                "volume must be between 0 and 1."
            )
        }
        audioVolume = volume
        runOnAudioThread {
            audioPlayer.volume = volume.toFloat()
        }
        dispatchAudioState()
        return true
    }

    override fun probeAudio(path: String): KSMAudioProbe? {
        val probe = probeMedia(path) ?: return null
        return KSMAudioProbe(
            durationMs = probe.durationMs?.toLong(),
            sampleRate = null,
            channels = null,
            codec = probe.format
        )
    }

    override fun loadAudio(path: String): Boolean {
        val uri = resolveAudioUri(path)
            ?: throw KSMHostException(
                "PERMISSION_DENIED",
                "The audio source is outside the allowed scope."
            )
        runOnAudioThread {
            audioSource = path
            audioPlayer.setMediaItem(MediaItem.fromUri(uri))
            audioPlayer.setPlaybackSpeed(audioSpeed.toFloat())
            audioPlayer.volume = audioVolume.toFloat()
            audioDeviceId
                ?.toIntOrNull()
                ?.let { id ->
                    activity.getSystemService(AudioManager::class.java)
                        ?.getDevices(AudioManager.GET_DEVICES_OUTPUTS)
                        ?.firstOrNull { it.id == id }
                }
                ?.let(audioPlayer::setPreferredAudioDevice)
            audioPlayer.prepare()
        }
        dispatchAudioState()
        return true
    }

    override fun setAudioPlayback(playing: Boolean): Boolean {
        val hasSource = runOnAudioThread {
            audioPlayer.currentMediaItem != null
        }
        if (!hasSource) {
            return false
        }
        runOnAudioThread {
            audioPlayer.playWhenReady = playing
            if (playing) {
                audioPlayer.play()
            } else {
                audioPlayer.pause()
            }
        }
        if (playing) {
            startMediaSessionService()
        } else {
            stopMediaSessionService()
        }
        dispatchAudioState()
        return true
    }

    override fun seekAudio(positionMs: Long): Boolean {
        if (positionMs < 0L) {
            throw KSMHostException("INVALID_ARGS", "positionMs must be non-negative.")
        }
        val hasSource = runOnAudioThread {
            audioPlayer.currentMediaItem != null
        }
        if (!hasSource) {
            return false
        }
        runOnAudioThread { audioPlayer.seekTo(positionMs) }
        dispatchAudioState()
        return true
    }

    override fun audioState(): KSMAudioState {
        return runOnAudioThread {
            val playback = when {
                audioPlayer.currentMediaItem == null -> "idle"
                audioPlayer.playbackState == Player.STATE_BUFFERING -> "loading"
                audioPlayer.playbackState == Player.STATE_ENDED -> "stopped"
                audioPlayer.isPlaying -> "playing"
                else -> "paused"
            }
            KSMAudioState(
                playing = audioPlayer.isPlaying,
                positionMs = audioPlayer.currentPosition.coerceAtLeast(0L),
                durationMs = audioPlayer.duration.takeIf { it >= 0L },
                volume = audioVolume,
                speed = audioSpeed,
                playback = playback,
                deviceId = audioDeviceId,
                source = audioSource
            )
        }
    }
    override fun updateMediaSession(
        metadata: KSMMediaSessionMetadata
    ): Boolean {
        val current = runOnAudioThread { audioPlayer.currentMediaItem }
        if (current == null) {
            return false
        }
        val mediaMetadata = MediaMetadata.Builder()
            .setTitle(metadata.title)
            .setArtist(metadata.artist)
            .setAlbumTitle(metadata.album)
            .apply {
                val artwork = metadata.artwork
                    ?.let(::resolveAudioUri)
                if (artwork != null) {
                    setArtworkUri(artwork)
                }
            }
            .build()
        runOnAudioThread {
            audioPlayer.replaceMediaItem(
                audioPlayer.currentMediaItemIndex,
                current.buildUpon().setMediaMetadata(mediaMetadata).build()
            )
            metadata.positionMs?.let { audioPlayer.seekTo(it.coerceAtLeast(0L)) }
            when (metadata.state) {
                "playing" -> audioPlayer.play()
                "paused" -> audioPlayer.pause()
                "stopped" -> {
                    audioPlayer.pause()
                    audioPlayer.seekTo(0L)
                }
            }
        }
        return true
    }

    override fun clearMediaSession(): Boolean {
        val current = runOnAudioThread { audioPlayer.currentMediaItem }
        if (current == null) {
            stopMediaSessionService()
            return true
        }
        runOnAudioThread {
            audioPlayer.replaceMediaItem(
                audioPlayer.currentMediaItemIndex,
                current.buildUpon()
                    .setMediaMetadata(MediaMetadata.Builder().build())
                    .build()
            )
        }
        stopMediaSessionService()
        return true
    }

    private fun startMediaSessionService() {
        try {
            ContextCompat.startForegroundService(
                activity,
                Intent(activity, KSMMediaSessionService::class.java)
            )
        } catch (_: SecurityException) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "Media playback notification permission is required."
            )
        } catch (_: RuntimeException) {
            throw KSMHostException(
                "IO_ERROR",
                "The media session service could not be started."
            )
        }
    }

    private fun stopMediaSessionService() {
        activity.stopService(
            Intent(activity, KSMMediaSessionService::class.java)
        )
    }
    override fun createWindow(
        route: String,
        state: KSMWindowState
    ): String? = null
    override fun showWindowById(windowId: String, visible: Boolean): Boolean = false
    override fun setWindowPosition(
        windowId: String,
        x: Int,
        y: Int
    ): Boolean = false
    override fun setWindowAlwaysOnTop(
        windowId: String,
        enabled: Boolean
    ): Boolean = false
    override fun setWindowTransparent(
        windowId: String,
        enabled: Boolean
    ): Boolean = false
    override fun windowState(windowId: String): KSMWindowState? = null
    override fun emitWindowEvent(
        windowId: String?,
        event: String,
        payload: String
    ): Boolean {
        eventDispatcher(event, payload)
        return false
    }

    override fun isExternalUrlAllowed(url: String): Boolean {
        return KSMHttpPolicy.isAllowedExternalUrl(url, allowedOrigins)
    }

    override fun openExternal(url: String): Boolean {
        if (!isExternalUrlAllowed(url)) {
            return false
        }
        val intent = Intent(Intent.ACTION_VIEW, Uri.parse(url))
        return try {
            activity.startActivity(intent)
            true
        } catch (_: ActivityNotFoundException) {
            false
        } catch (_: SecurityException) {
            false
        }
    }

    override fun openPath(path: String): Boolean {
        val file = File(path)
        val intent = if (file.isDirectory) {
            Intent(Intent.ACTION_VIEW, Uri.fromFile(file))
        } else {
            Intent(Intent.ACTION_VIEW).apply {
                setDataAndType(
                    Uri.fromFile(file),
                    "application/octet-stream"
                )
            }
        }
        return try {
            activity.startActivity(intent)
            true
        } catch (_: ActivityNotFoundException) {
            false
        } catch (_: SecurityException) {
            false
        }
    }

    override fun showInFolder(path: String): Boolean {
        val file = File(path)
        val intent = Intent(Intent.ACTION_VIEW, Uri.fromFile(file))
        return try {
            activity.startActivity(intent)
            true
        } catch (_: ActivityNotFoundException) {
            false
        } catch (_: SecurityException) {
            false
        }
    }

    override fun deviceInfo(): KSMDeviceInfo {
        val metrics = activity.resources.displayMetrics
        val bounds = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            activity.windowManager.currentWindowMetrics.bounds
        } else {
            val legacy = android.util.DisplayMetrics()
            @Suppress("DEPRECATION")
            activity.windowManager.defaultDisplay.getRealMetrics(legacy)
            Rect(0, 0, legacy.widthPixels, legacy.heightPixels)
        }
        val deviceName = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N_MR1) {
            Settings.Global.getString(
                activity.contentResolver,
                Settings.Global.DEVICE_NAME
            )
        } else {
            null
        }?.takeIf { it.isNotBlank() } ?: Build.MODEL
        return KSMDeviceInfo(
            platform = "android",
            osVersion = Build.VERSION.RELEASE ?: "unknown",
            model = Build.MODEL,
            manufacturer = Build.MANUFACTURER,
            appVersion = appVersion(),
            language = language(),
            screenWidth = bounds.width(),
            screenHeight = bounds.height(),
            density = metrics.density,
            deviceName = deviceName,
            timezone = TimeZone.getDefault().id,
            architecture = Build.SUPPORTED_ABIS.firstOrNull() ?: Build.CPU_ABI
        )
    }

    override fun operatingSystemInfo(): KSMOperatingSystemInfo {
        return KSMOperatingSystemInfo(
            platform = "android",
            architecture = Build.SUPPORTED_ABIS.firstOrNull() ?: "unknown",
            version = Build.VERSION.RELEASE ?: "unknown",
            locale = language(),
            timezone = TimeZone.getDefault().id,
            devMode = true
        )
    }

    override fun writeLog(
        level: String,
        target: String,
        message: String
    ): Boolean {
        val priority = when (level) {
            "trace" -> Log.VERBOSE
            "debug" -> Log.DEBUG
            "info" -> Log.INFO
            "warn" -> Log.WARN
            else -> Log.ERROR
        }
        val tag = target.ifBlank { "Kasumi" }.take(23)
        return try {
            Log.println(priority, tag, message)
            true
        } catch (_: RuntimeException) {
            false
        }
    }

    override fun registerSqlDriver(driver: KSMSqlDriver) {
        sqlDriverRegistry.register(driver)
    }

    override fun sqlDrivers(): List<String> {
        return sqlDriverRegistry.drivers()
    }

    override fun sqlOpen(driver: String, url: String): String? {
        return sqlDriverRegistry.open(driver, url)
    }

    override fun sqlQuery(
        connectionId: String,
        statement: String,
        params: List<Any?>,
        requestId: String?
    ): KSMSqlQueryResult {
        return sqlDriverRegistry.query(
            connectionId,
            statement,
            params,
            requestId
        )
    }

    override fun sqlExecute(
        connectionId: String,
        statement: String,
        params: List<Any?>,
        requestId: String?
    ): KSMSqlExecuteResult {
        return sqlDriverRegistry.execute(
            connectionId,
            statement,
            params,
            requestId
        )
    }

    override fun sqlClose(connectionId: String): Boolean {
        return sqlDriverRegistry.close(connectionId)
    }

    fun handleActivityResult(
        requestCode: Int,
        resultCode: Int,
        data: Intent?
    ): Boolean {
        val pending = synchronized(this) {
            when (requestCode) {
                REQUEST_OPEN_FILE -> pendingOpenFile
                REQUEST_SAVE_FILE -> pendingSaveFile
                else -> null
            }
        }
        if (requestCode == REQUEST_OPEN_FOLDER) {
            val folder = synchronized(this) { pendingOpenFolder }
                ?: return false
            if (resultCode == Activity.RESULT_OK) {
                data?.data?.let { uri ->
                    takePersistablePermission(
                        uri,
                        Intent.FLAG_GRANT_READ_URI_PERMISSION or
                            Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                    )
                    folder.result.set(uri.toString())
                }
            }
            folder.latch.countDown()
            synchronized(this) { pendingOpenFolder = null }
            return true
        }
        if (pending == null) {
            return false
        }

        if (resultCode == Activity.RESULT_OK) {
            data?.data?.let { uri ->
                val flags = if (requestCode == REQUEST_OPEN_FILE) {
                    Intent.FLAG_GRANT_READ_URI_PERMISSION
                } else {
                    Intent.FLAG_GRANT_READ_URI_PERMISSION or
                        Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                }
                try {
                    activity.contentResolver.takePersistableUriPermission(uri, flags)
                } catch (_: SecurityException) {
                    // Some document providers do not offer persistable access.
                }
                pending.result.set(uri.toString())
            }
        }
        pending.latch.countDown()
        return true
    }

    fun handleMultiSelectActivityResult(
        requestCode: Int,
        resultCode: Int,
        data: Intent?
    ): Boolean {
        if (requestCode != REQUEST_OPEN_FILES) {
            return false
        }
        val pending = synchronized(this) { pendingOpenFiles } ?: return false
        if (resultCode == Activity.RESULT_OK && data != null) {
            val uris = mutableListOf<Uri>()
            data.data?.let(uris::add)
            val clip = data.clipData
            if (clip != null) {
                for (index in 0 until clip.itemCount) {
                    clip.getItemAt(index).uri?.let(uris::add)
                }
            }
            uris.distinct().forEach {
                takePersistablePermission(
                    it,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION
                )
            }
            pending.result.set(uris.distinct().map(Uri::toString))
        }
        pending.latch.countDown()
        return true
    }

    fun handleRequestPermissionsResult(
        requestCode: Int,
        grantResults: IntArray
    ): Boolean {
        if (requestCode != REQUEST_NOTIFICATION_PERMISSION) {
            return false
        }
        val pending = synchronized(this) { pendingPermission } ?: return false
        pending.result.set(
            grantResults.isNotEmpty() &&
                grantResults[0] == PackageManager.PERMISSION_GRANTED
        )
        pending.latch.countDown()
        return true
    }

    private fun showDialog(
        title: String,
        message: String,
        hasNegativeButton: Boolean
    ): Boolean {
        val result = AtomicBoolean(false)
        val latch = CountDownLatch(1)
        val dialogRef = AtomicReference<AlertDialog?>(null)
        activity.runOnUiThread {
            try {
                val builder = AlertDialog.Builder(activity)
                    .setTitle(title)
                    .setMessage(message)
                    .setPositiveButton("OK") { _, _ ->
                        result.set(true)
                        latch.countDown()
                    }
                    .setOnCancelListener {
                        result.set(false)
                        latch.countDown()
                    }
                if (hasNegativeButton) {
                    builder.setNegativeButton("Cancel") { _, _ ->
                        result.set(false)
                        latch.countDown()
                    }
                }
                val dialog = builder.create()
                dialogRef.set(dialog)
                dialog.show()
            } catch (_: RuntimeException) {
                latch.countDown()
            }
        }
        val completed = try {
            latch.await(DIALOG_TIMEOUT_SECONDS, TimeUnit.SECONDS)
        } catch (_: InterruptedException) {
            Thread.currentThread().interrupt()
            false
        }
        if (!completed) {
            activity.runOnUiThread { dialogRef.get()?.dismiss() }
        }
        return completed && result.get()
    }

    private fun awaitUri(pending: PendingUri, isOpen: Boolean): String? {
        val completed = try {
            pending.latch.await(URI_TIMEOUT_SECONDS, TimeUnit.SECONDS)
        } catch (_: InterruptedException) {
            Thread.currentThread().interrupt()
            false
        }
        synchronized(this) {
            if (isOpen) {
                pendingOpenFile = null
            } else {
                pendingSaveFile = null
            }
        }
        return if (completed) pending.result.get() else null
    }

    private fun awaitUris(pendings: List<PendingUri>): List<String>? {
        val completed = try {
            pendings.first().latch.await(URI_TIMEOUT_SECONDS, TimeUnit.SECONDS)
        } catch (_: InterruptedException) {
            Thread.currentThread().interrupt()
            false
        }
        synchronized(this) { pendingOpenFolder = null }
        val value = pendings.first().result.get()
        return if (completed && value != null) listOf(value) else null
    }

    private fun takePersistablePermission(uri: Uri, flags: Int) {
        try {
            activity.contentResolver.takePersistableUriPermission(uri, flags)
        } catch (_: SecurityException) {
        }
    }

    private fun documentName(uri: Uri): String {
        activity.contentResolver.query(
            uri,
            arrayOf(OpenableColumns.DISPLAY_NAME),
            null,
            null,
            null
        )?.use { cursor ->
            if (cursor.moveToFirst()) {
                val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                if (index >= 0) {
                    return cursor.getString(index) ?: ""
                }
            }
        }
        return ""
    }

    private fun documentSize(uri: Uri): Long {
        activity.contentResolver.query(
            uri,
            arrayOf(OpenableColumns.SIZE),
            null,
            null,
            null
        )?.use { cursor ->
            if (cursor.moveToFirst()) {
                val index = cursor.getColumnIndex(OpenableColumns.SIZE)
                if (index >= 0 && !cursor.isNull(index)) {
                    return cursor.getLong(index)
                }
            }
        }
        return 0L
    }

    private fun walkDocument(
        document: DocumentFile,
        path: String,
        entries: MutableList<KSMWalkEntry>,
        requestId: String?
    ) {
        if (requestId?.let(::isRequestCancelled) == true) {
            throw KSMHostException("CANCELLED", "The command was cancelled.")
        }
        document.listFiles().forEach { child ->
            val childPath = "$path/${child.name.orEmpty()}"
            entries.add(
                KSMWalkEntry(
                    childPath,
                    child.name.orEmpty(),
                    child.isDirectory,
                    if (child.isDirectory) 0L else child.length(),
                    child.lastModified()
                )
            )
            if (child.isDirectory) {
                walkDocument(child, childPath, entries, requestId)
            }
        }
    }

    private fun resolveAudioUri(path: String): Uri? {
        if (path.startsWith("https://")) {
            return if (isHttpRequestUrlAllowed(path)) Uri.parse(path) else null
        }
        if (path.startsWith("app://scope/")) {
            return resolveScopeUri(path)
        }
        return fileSystem?.resolveReadableFile(path)?.let(Uri::fromFile)
    }

    private fun resolveScopeUri(path: String): Uri? {
        val remainder = path.removePrefix("app://scope/")
        val id = remainder.substringBefore('/')
        val scope = fileScopes[id] ?: return null
        val relative = remainder.substringAfter('/', "")
        if (relative.isEmpty()) {
            return scope.root
        }
        if (scope.files.isNotEmpty() && relative.startsWith("file-")) {
            val index = relative.removePrefix("file-").toIntOrNull()
                ?: return null
            return scope.files.getOrNull(index)
        }
        val root = DocumentFile.fromTreeUri(activity, scope.root)
            ?: return null
        var current: DocumentFile = root
        for (segment in relative.split('/')) {
            if (segment.isEmpty() || segment == "." || segment == "..") {
                return null
            }
            current = current.findFile(segment) ?: return null
        }
        return current.uri
    }

    private fun <T> runOnAudioThread(block: () -> T): T {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            return block()
        }
        val result = AtomicReference<T?>()
        val failure = AtomicReference<Throwable?>()
        val latch = CountDownLatch(1)
        audioHandler.post {
            try {
                result.set(block())
            } catch (error: Throwable) {
                failure.set(error)
            } finally {
                latch.countDown()
            }
        }
        if (!latch.await(URI_TIMEOUT_SECONDS, TimeUnit.SECONDS)) {
            throw KSMHostException("TIMEOUT", "The audio operation timed out.")
        }
        failure.get()?.let { throw it }
        return result.get() as T
    }

    private fun dispatchAudioState() {
        val state = audioState()
        eventDispatcher(
            "audio.stateChanged",
            org.json.JSONObject()
                .put(
                    "state",
                    org.json.JSONObject()
                        .put(
                            "playback",
                            state.playback
                        )
                        .put("positionMs", state.positionMs)
                        .put("durationMs", state.durationMs ?: org.json.JSONObject.NULL)
                        .put("volume", state.volume)
                        .put("muted", state.muted)
                        .put("speed", state.speed)
                        .put("outputMode", state.outputMode)
                        .put(
                            "deviceId",
                            state.deviceId ?: org.json.JSONObject.NULL
                        )
                        .put(
                            "source",
                            state.source ?: org.json.JSONObject.NULL
                        )
                )
                .put(
                    "reason",
                    if (state.playing) "play" else "pause"
                )
                .toString()
        )
    }

    private fun startAudioTicks() {
        if (audioTickTask != null) {
            return
        }
        audioTickTask = downloadWatchdog.scheduleAtFixedRate(
            {
                val state = audioState()
                eventDispatcher(
                    "audio.tick",
                    org.json.JSONObject()
                        .put("positionMs", state.positionMs)
                        .put(
                            "durationMs",
                            state.durationMs ?: org.json.JSONObject.NULL
                        )
                        .toString()
                )
            },
            250L,
            250L,
            TimeUnit.MILLISECONDS
        )
    }

    private fun stopAudioTicks() {
        audioTickTask?.cancel(false)
        audioTickTask = null
    }

    private fun cancelTransfer(state: TransferState) {
        state.cancelled.set(true)
        state.connection.getAndSet(null)?.disconnect()
    }

    private fun registerPicked(uri: Uri): String {
        val path = KSMVirtualPath.pickedPath(nextPickedId.getAndIncrement().toString())
        synchronized(this) {
            pickedFiles[path] = uri
        }
        return path
    }

    private fun resolvePicked(path: String): Uri? {
        return synchronized(this) { pickedFiles[path] }
    }

    private fun bundledFileSize(path: String): Long? {
        val descriptorSize = try {
            activity.assets.openFd(path).use { descriptor ->
                descriptor.length.takeIf { it >= 0L }
            }
        } catch (_: Exception) {
            null
        }
        if (descriptorSize != null) {
            return descriptorSize
        }
        return try {
            activity.assets.open(path).use { input ->
                input.available().toLong()
            }
        } catch (_: Exception) {
            null
        }
    }

    private fun bundledDirectoryExists(path: String): Boolean {
        if (path == "res" || path == "web") {
            return true
        }
        val parent = path.substringBeforeLast('/', "")
        val name = path.substringAfterLast('/')
        if (parent.isEmpty() || name.isEmpty()) {
            return false
        }
        return try {
            activity.assets.list(parent)?.contains(name) == true
        } catch (_: Exception) {
            false
        }
    }

    private fun readLimited(input: InputStream): String? {
        val output = ByteArrayOutputStream()
        val buffer = ByteArray(8192)
        var total = 0
        while (true) {
            val read = input.read(buffer)
            if (read < 0) {
                break
            }
            total += read
            if (total > KSMFileSystem.MAX_TEXT_BYTES) {
                return null
            }
            output.write(buffer, 0, read)
        }
        return output.toString(Charsets.UTF_8.name())
    }

    private fun copyLimited(
        input: InputStream,
        outputFile: File,
        limit: Long,
        session: DownloadSession,
        deadlineNanos: Long
    ): Long {
        var total = 0L
        val buffer = ByteArray(8192)
        outputFile.outputStream().use { output ->
            while (true) {
                if (
                    session.cancelled.get() ||
                    Thread.currentThread().isInterrupted ||
                    System.nanoTime() >= deadlineNanos
                ) {
                    throw downloadCancellation(deadlineNanos)
                }
                val read = input.read(buffer)
                if (read < 0) {
                    break
                }
                total += read
                if (total > limit) {
                    throw IOException("Download exceeds the maximum size.")
                }
                output.write(buffer, 0, read)
            }
            output.flush()
        }
        return total
    }

    private fun cancelDownload(session: DownloadSession) {
        session.cancelled.set(true)
        session.connection.getAndSet(null)?.disconnect()
        session.deadlineTask?.cancel(false)
    }

    private fun downloadCancellation(deadlineNanos: Long): KSMHostException {
        return if (System.nanoTime() >= deadlineNanos) {
            KSMHostException("TIMEOUT", "The download timed out.")
        } else {
            KSMHostException("CANCELLED", "The download was cancelled.")
        }
    }

    private fun commitDownload(
        part: File,
        target: File,
        overwrite: Boolean
    ): Boolean {
        return try {
            if (overwrite) {
                Os.rename(part.path, target.path)
            } else {
                Os.link(part.path, target.path)
                part.delete()
            }
            true
        } catch (error: ErrnoException) {
            if (error.errno == OsConstants.EEXIST && !overwrite) {
                throw KSMHostException("IO_ERROR", "The target already exists.")
            }
            false
        } catch (_: RuntimeException) {
            false
        }
    }

    private fun hasNotificationPermission(): Boolean {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU ||
            ContextCompat.checkSelfPermission(
                activity,
                Manifest.permission.POST_NOTIFICATIONS
            ) == PackageManager.PERMISSION_GRANTED
    }
}
