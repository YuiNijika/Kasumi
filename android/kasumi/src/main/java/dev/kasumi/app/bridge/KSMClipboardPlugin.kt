package dev.kasumi.app.bridge

import org.json.JSONObject

class KSMClipboardPlugin : KSMPlugin {
    override val id: String = "clipboard"
    override val commands: Set<String> = setOf(
        "clipboard.readText",
        "clipboard.writeText"
    )
    override val permissions: Set<String> = setOf(
        "clipboard.read",
        "clipboard.write"
    )

    override fun handle(
        command: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any {
        val host = context.hostServices ?: throw KSMHostException(
            "NOT_SUPPORTED",
            "The clipboard service is not configured."
        )
        return if (command == "clipboard.readText") {
            requirePermission(context, "clipboard.read")
            JSONObject().put("text", host.readClipboardText() ?: "")
        } else {
            requirePermission(context, "clipboard.write")
            val text = args.opt("text")
            if (text !is String) {
                throw KSMHostException("INVALID_ARGS", "text is required.")
            }
            JSONObject().put("written", host.writeClipboardText(text))
        }
    }

    private fun requirePermission(
        context: KSMPluginContext,
        permission: String
    ) {
        if (!context.hasPermission(permission)) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The requested permission was denied."
            )
        }
    }
}
