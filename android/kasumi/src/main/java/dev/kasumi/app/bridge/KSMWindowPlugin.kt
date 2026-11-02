package dev.kasumi.app.bridge

import org.json.JSONObject

class KSMWindowPlugin : KSMPlugin {
    override val id: String = "window"
    override val commands: Set<String> = setOf(
        "window.setTitle",
        "window.minimize",
        "window.maximize",
        "window.setSize",
        "window.close"
    )
    override val permissions: Set<String> = setOf("window.control")

    override fun handle(
        command: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any {
        if (!context.hasPermission("window.control")) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The requested permission was denied."
            )
        }
        if (command != "window.close") {
            throw KSMHostException(
                "NOT_SUPPORTED",
                "The command is not supported on this platform."
            )
        }
        val host = context.hostServices ?: throw KSMHostException(
            "NOT_SUPPORTED",
            "The window service is not configured."
        )
        return JSONObject().put("closed", host.closeWindow())
    }
}
