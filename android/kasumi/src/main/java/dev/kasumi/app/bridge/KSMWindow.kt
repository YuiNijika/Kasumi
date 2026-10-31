package dev.kasumi.app.bridge

object KSMWindow {
    fun close(): Boolean {
        if (!KSM.isCommandEnabled("window.close")) {
            return false
        }
        return KSM.requireHostServices().closeWindow()
    }
}
