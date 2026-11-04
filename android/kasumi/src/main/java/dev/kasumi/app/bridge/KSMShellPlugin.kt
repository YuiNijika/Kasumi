package dev.kasumi.app.bridge

import org.json.JSONObject

class KSMShellPlugin : KSMPlugin {
    override val id: String = "shell"
    override val commands: Set<String> = setOf(
        "shell.openExternal",
        "shell.openPath",
        "shell.showInFolder"
    )
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
        if (command != "shell.openExternal") {
            val path = args.opt("path")
            if (path !is String || !KSMVirtualPath.isSafe(path)) {
                throw KSMHostException(
                    "PERMISSION_DENIED",
                    "The path is outside the sandbox."
                )
            }
            val file = context.fileSystem?.resolveReadableFile(path)
                ?: throw KSMHostException("NOT_FOUND", "The path was not found.")
            val key = if (command == "shell.showInFolder") "shown" else "opened"
            val value = if (command == "shell.showInFolder") {
                host.showInFolder(file.absolutePath)
            } else {
                host.openPath(file.absolutePath)
            }
            return JSONObject().put(key, value)
        }
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
