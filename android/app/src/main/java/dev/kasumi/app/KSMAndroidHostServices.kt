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
import android.graphics.Rect
import android.net.Uri
import android.os.Build
import android.provider.DocumentsContract
import android.provider.OpenableColumns
import android.provider.Settings
import android.system.ErrnoException
import android.system.Os
import android.system.OsConstants
import androidx.core.content.ContextCompat
import dev.kasumi.app.bridge.KSMDirectoryEntry
import dev.kasumi.app.bridge.KSMDeviceInfo
import dev.kasumi.app.bridge.KSMFileKind
import dev.kasumi.app.bridge.KSMFileMetadata
import dev.kasumi.app.bridge.KSMFileSystem
import dev.kasumi.app.bridge.KSMHostServices
import dev.kasumi.app.bridge.KSMHttpDownloadResult
import dev.kasumi.app.bridge.KSMHttpPolicy
import dev.kasumi.app.bridge.KSMVirtualPath
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.net.SocketTimeoutException
import java.util.Locale
import java.util.TimeZone
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.CountDownLatch
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
    private val fileSystem: KSMFileSystem? = null
) : KSMHostServices {
    companion object {
        const val REQUEST_OPEN_FILE = 1001
        const val REQUEST_SAVE_FILE = 1002
        const val REQUEST_NOTIFICATION_PERMISSION = 1003

        private const val CHANNEL_ID = "kasumi"
        private const val DIALOG_TIMEOUT_SECONDS = 60L
        private const val URI_TIMEOUT_SECONDS = 60L
        private const val DOWNLOAD_IO_TIMEOUT_MS = 15000
        private const val DOWNLOAD_TOTAL_TIMEOUT_MS = 120000L
        private const val MAX_DOWNLOAD_BYTES = 100L * 1024L * 1024L
        private const val MAX_REDIRECTS = 5
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

    private data class PendingPermission(
        val latch: CountDownLatch,
        val result: AtomicBoolean = AtomicBoolean(false)
    )

    private var pendingOpenFile: PendingUri? = null
    private var pendingSaveFile: PendingUri? = null
    private var pendingPermission: PendingPermission? = null
    private val pickedFiles = mutableMapOf<String, Uri>()
    private val allowedOrigins = KSMHttpPolicy.normalizeOrigins(allowedExternalOrigins)
    private val activeDownloads = ConcurrentHashMap<String, DownloadSession>()
    private val downloadWatchdog = Executors.newSingleThreadScheduledExecutor { task ->
        Thread(task, "kasumi-download-watchdog").apply {
            isDaemon = true
        }
    }

    override fun appVersion(): String {
        return BuildConfig.VERSION_NAME
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
                return null
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
                return null
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
        val part = File(parent, ".${target.name}.${System.nanoTime()}.part")
        var current = KSMHttpPolicy.parseHttpUrl(url, httpsOnly = true)
            ?: throw KSMHostException("INVALID_ARGS", "url must use HTTPS.")
        var redirects = 0
        val session = DownloadSession()
        val downloadKey = requestId ?: "anonymous-${System.nanoTime()}"
        val deadlineNanos =
            System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(DOWNLOAD_TOTAL_TIMEOUT_MS)
        activeDownloads[downloadKey] = session
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
        } catch (_: InterruptedException) {
            Thread.currentThread().interrupt()
            throw downloadCancellation(deadlineNanos)
        } catch (error: SocketTimeoutException) {
            if (session.cancelled.get()) {
                throw downloadCancellation(deadlineNanos)
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
    }

    fun cancelAllDownloads() {
        activeDownloads.values.forEach(::cancelDownload)
    }

    fun shutdown() {
        cancelAllDownloads()
        downloadWatchdog.shutdownNow()
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
        } ?: return false

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
