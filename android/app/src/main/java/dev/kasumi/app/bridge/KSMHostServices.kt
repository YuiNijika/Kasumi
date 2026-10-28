package dev.kasumi.app.bridge

interface KSMHostServices {
    fun showMessage(title: String, message: String): Boolean
    fun confirm(title: String, message: String): Boolean
    fun openFile(filters: List<String>): String?
    fun saveFile(suggestedName: String): String?
    fun readPickedText(path: String): String?
    fun writePickedText(path: String, content: String): Boolean
    fun pickedExists(path: String): Boolean
    fun removePicked(path: String): Boolean
    fun readClipboardText(): String?
    fun writeClipboardText(text: String): Boolean
    fun showNotification(title: String, body: String): Boolean
    fun requestPermission(permission: String): Boolean
    fun closeWindow(): Boolean
    fun exitApp(): Boolean
}
