package dev.kasumi.app.bridge

object KSMNotification {
    fun show(title: String, body: String): Boolean {
        val host = KSM.requireHostServices()
        if (!host.hasRuntimePermission("notification.show")) {
            return false
        }
        return host.showNotification(title, body)
    }
}
