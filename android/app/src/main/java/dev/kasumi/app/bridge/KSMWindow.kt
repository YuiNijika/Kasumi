package dev.kasumi.app.bridge

object KSMWindow {
    fun close(): Boolean {
        return KSM.requireHostServices().closeWindow()
    }
}
