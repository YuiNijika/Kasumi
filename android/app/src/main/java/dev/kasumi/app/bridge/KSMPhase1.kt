package dev.kasumi.app.bridge

enum class KSMPlatform {
    ANDROID
}

data class KSMCapabilities(
    val platform: KSMPlatform,
    val commands: List<String>,
    val permissions: List<String>
)

data class KSMAppInfo(
    val name: String,
    val version: String,
    val platform: KSMPlatform,
    val language: String
)

data class KSMPingResult(
    val pong: Boolean
)

data class KSMHttpDownloadResult(
    val path: String,
    val bytes: Long
)

data class KSMDeviceInfo(
    val platform: String,
    val osVersion: String,
    val model: String,
    val manufacturer: String,
    val appVersion: String,
    val language: String,
    val screenWidth: Int,
    val screenHeight: Int,
    val density: Float,
    val deviceName: String,
    val timezone: String,
    val architecture: String
)
