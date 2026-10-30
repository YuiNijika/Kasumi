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
