package dev.kasumi.app.bridge

import org.json.JSONObject

class KSMNotificationPlugin : KSMPlugin {
    override val id: String = "notification"
    override val commands: Set<String> = setOf("notification.show")
    override val permissions: Set<String> = setOf("notification.show")

    override fun handle(
        command: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any {
        if (!context.hasPermission("notification.show")) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The requested permission was denied."
            )
        }
        val host = context.hostServices ?: throw KSMHostException(
            "NOT_SUPPORTED",
            "The notification service is not configured."
        )
        if (!host.hasRuntimePermission("notification.show")) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The requested permission was denied."
            )
        }
        val title = args.opt("title")
        val body = args.opt("body")
        if (title !is String || body !is String) {
            throw KSMHostException(
                "INVALID_ARGS",
                "title and body are required."
            )
        }
        return JSONObject().put("shown", host.showNotification(title, body))
    }
}
