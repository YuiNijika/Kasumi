package dev.kasumi.app.bridge

object KSMApp {
    fun getInfo(): KSMAppInfo {
        check(KSM.isCommandEnabled("app.getInfo")) {
            "The app.getInfo command is disabled by application configuration."
        }
        val host = KSM.requireHostServices()
        return KSMAppInfo(
            name = "Kasumi",
            version = host.appVersion(),
            platform = KSMPlatform.ANDROID,
            language = host.language()
        )
    }

    fun exit(): Boolean {
        if (!KSM.isCommandEnabled("app.exit")) {
            return false
        }
        return KSM.requireHostServices().exitApp()
    }
}
