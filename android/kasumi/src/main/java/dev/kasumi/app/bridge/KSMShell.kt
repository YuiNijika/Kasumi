package dev.kasumi.app.bridge

object KSMShell {
    fun openExternal(url: String): Boolean {
        if (!KSM.isCommandEnabled("shell.openExternal")) {
            return false
        }
        val host = KSM.requireHostServices()
        return host.isExternalUrlAllowed(url) && host.openExternal(url)
    }
}
