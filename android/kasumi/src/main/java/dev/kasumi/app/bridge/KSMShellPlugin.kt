package dev.kasumi.app.bridge

import org.json.JSONObject

class KSMShellPlugin : KSMPlugin {
    override val id: String = "shell"
    override val commands: Set<String> = setOf("shell.openExternal")
    override val permissions: Set<String> = setOf("shell.open")

    override fun handle(
        command: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any {
        if (!context.hasPermission("shell.open")) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The requested permission was denied."
            )
        }
        val host = context.hostServices ?: throw KSMHostException(
            "NOT_SUPPORTED",
            "The shell service is not configured."
        )
        val url = args.opt("url")
        if (url !is String || KSMHttpPolicy.parseHttpUrl(url) == null) {
            throw KSMHostException(
                "INVALID_ARGS",
                "url must be an absolute HTTP or HTTPS URL."
            )
        }
        if (!host.isExternalUrlAllowed(url)) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The URL origin is not allowlisted."
            )
        }
        return JSONObject().put("opened", host.openExternal(url))
    }
}
