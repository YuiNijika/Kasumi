package dev.kasumi.app.bridge

import dev.kasumi.app.internal.KSMVersionMetadata
import java.util.Locale

interface KSMHostServices {
    fun appVersion(): String = KSMVersionMetadata.VERSION
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
    fun isExternalUrlAllowed(url: String): Boolean = false
    fun openExternal(url: String): Boolean = false
    fun deviceInfo(): KSMDeviceInfo? = null
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
        params: List<Any?>
    ): KSMSqlQueryResult? = null
    fun sqlExecute(
        connectionId: String,
        statement: String,
        params: List<Any?>
    ): KSMSqlExecuteResult? = null
    fun sqlClose(connectionId: String): Boolean = false
    fun cancelRequest(requestId: String) {}
}

class KSMHostException(
    val code: String,
    override val message: String
) : Exception(message)
