package dev.kasumi.app.bridge

object KSMClipboard {
    fun readText(): String? {
        return KSM.requireHostServices().readClipboardText()
    }

    fun writeText(text: String): Boolean {
        return KSM.requireHostServices().writeClipboardText(text)
    }
}
