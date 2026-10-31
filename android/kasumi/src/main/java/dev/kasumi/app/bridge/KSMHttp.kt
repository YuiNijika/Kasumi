package dev.kasumi.app.bridge

object KSMHttp {
    fun download(url: String, path: String, overwrite: Boolean = false): KSMHttpDownloadResult? {
        if (!KSM.isCommandEnabled("http.download")) {
            return null
        }
        return KSM.requireHostServices().download(url, path, overwrite)
    }
}
