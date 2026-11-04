package dev.kasumi.app.bridge

import org.json.JSONObject

class KSMOperatingSystemPlugin : KSMPlugin {
    override val id: String = "os"
    override val commands: Set<String> = setOf("os.getInfo")
    override val permissions: Set<String> = setOf("os.info")

    override fun handle(
        command: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any {
        if (!context.hasPermission("os.info")) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The requested permission was denied."
            )
        }
        val host = context.hostServices
            ?: throw KSMHostException(
                "NOT_SUPPORTED",
                "The operating system service is not configured."
            )
        val info = host.operatingSystemInfo()
            ?: throw KSMHostException(
                "NOT_SUPPORTED",
                "Operating system information is unavailable."
            )
        return JSONObject()
            .put("platform", info.platform)
            .put("architecture", info.architecture)
            .put("version", info.version)
            .put("locale", info.locale)
            .put("timezone", info.timezone)
            .put("appVersion", host.appVersion())
            .put("devMode", info.devMode)
            .apply {
                info.hostname?.let { put("hostname", it) }
                info.username?.let { put("username", it) }
            }
    }
}
