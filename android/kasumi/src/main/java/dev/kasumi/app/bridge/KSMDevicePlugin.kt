package dev.kasumi.app.bridge

import org.json.JSONObject

class KSMDevicePlugin : KSMPlugin {
    override val id: String = "device"
    override val commands: Set<String> = setOf("device.getInfo")
    override val permissions: Set<String> = setOf("device.info")

    override fun handle(
        command: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any {
        if (!context.hasPermission("device.info")) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The requested permission was denied."
            )
        }
        val host = context.hostServices ?: throw KSMHostException(
            "NOT_SUPPORTED",
            "The device service is not configured."
        )
        val info = host.deviceInfo() ?: throw KSMHostException(
            "NOT_SUPPORTED",
            "The device service is not configured."
        )
        return JSONObject()
            .put("platform", info.platform)
            .put("osVersion", info.osVersion)
            .put("model", info.model)
            .put("manufacturer", info.manufacturer)
            .put("appVersion", info.appVersion)
            .put("language", info.language)
            .put("screenWidth", info.screenWidth)
            .put("screenHeight", info.screenHeight)
            .put("density", info.density.toDouble())
            .put("deviceName", info.deviceName)
            .put("timezone", info.timezone)
            .put("architecture", info.architecture)
    }
}
