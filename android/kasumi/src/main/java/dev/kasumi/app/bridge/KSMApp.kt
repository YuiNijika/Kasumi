package dev.kasumi.app.bridge

object KSMApp {
    fun getInfo(): KSMAppInfo {
        val host = KSM.requireHostServices()
        return KSMAppInfo(
            name = "Kasumi",
            version = host.appVersion(),
            platform = KSMPlatform.ANDROID,
            language = host.language()
        )
    }

    fun exit(): Boolean {
        return KSM.requireHostServices().exitApp()
    }
}
