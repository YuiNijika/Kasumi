package dev.kasumi.app.bridge

object KSMHttp {
    fun download(url: String, path: String, overwrite: Boolean = false): KSMHttpDownloadResult? {
        return KSM.requireHostServices().download(url, path, overwrite)
    }
}
