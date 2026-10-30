package dev.kasumi.app.bridge

object KSMShell {
    fun openExternal(url: String): Boolean {
        val host = KSM.requireHostServices()
        return host.isExternalUrlAllowed(url) && host.openExternal(url)
    }
}
