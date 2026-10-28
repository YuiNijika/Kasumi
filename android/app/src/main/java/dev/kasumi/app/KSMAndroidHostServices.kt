package dev.kasumi.app

import android.Manifest
import android.app.Activity
import android.app.AlertDialog
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.provider.DocumentsContract
import androidx.core.content.ContextCompat
import dev.kasumi.app.bridge.KSMFileSystem
import dev.kasumi.app.bridge.KSMHostServices
import dev.kasumi.app.bridge.KSMVirtualPath
import java.io.ByteArrayOutputStream
import java.io.InputStream
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicReference

class KSMAndroidHostServices(
    private val activity: Activity
) : KSMHostServices {
    companion object {
        const val REQUEST_OPEN_FILE = 1001
        const val REQUEST_SAVE_FILE = 1002
        const val REQUEST_NOTIFICATION_PERMISSION = 1003

        private const val CHANNEL_ID = "kasumi"
        private const val DIALOG_TIMEOUT_SECONDS = 60L
        private const val URI_TIMEOUT_SECONDS = 60L
        private val nextNotificationId = AtomicInteger(1)
        private val nextPickedId = AtomicInteger(1)
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

    override fun removePicked(path: String): Boolean {
        val uri = synchronized(this) { pickedFiles.remove(path) } ?: return false
        return try {
            DocumentsContract.deleteDocument(activity.contentResolver, uri) ||
                activity.contentResolver.delete(uri, null, null) > 0
        } catch (_: RuntimeException) {
            false
        }
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

    override fun closeWindow(): Boolean {
        activity.runOnUiThread { activity.finish() }
        return true
    }

    override fun exitApp(): Boolean {
        activity.runOnUiThread { activity.finishAffinity() }
        return true
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

    private fun hasNotificationPermission(): Boolean {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU ||
            ContextCompat.checkSelfPermission(
                activity,
                Manifest.permission.POST_NOTIFICATIONS
            ) == PackageManager.PERMISSION_GRANTED
    }
}
