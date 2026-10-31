package dev.kasumi.app.bridge

object KSMClipboard {
    fun readText(): String? {
        if (!KSM.isCommandEnabled("clipboard.readText")) {
            return null
        }
        return KSM.requireHostServices().readClipboardText()
    }

    fun writeText(text: String): Boolean {
        if (!KSM.isCommandEnabled("clipboard.writeText")) {
            return false
        }
        return KSM.requireHostServices().writeClipboardText(text)
    }
}
