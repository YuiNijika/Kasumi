package dev.kasumi.app.bridge

import org.json.JSONObject

class KSMHttpPlugin : KSMPlugin {
    override val id: String = "http"
    override val commands: Set<String> = setOf("http.download")
    override val permissions: Set<String> = setOf("http.download")

    override fun handle(
        command: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any {
        if (!context.hasPermission("http.download")) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The requested permission was denied."
            )
        }
        val host = context.hostServices ?: throw KSMHostException(
            "NOT_SUPPORTED",
            "The download service is not configured."
        )
        val url = args.opt("url")
        val path = args.opt("path")
        if (url !is String || path !is String) {
            throw KSMHostException(
                "INVALID_ARGS",
                "url and path are required."
            )
        }
        if (KSMHttpPolicy.parseHttpUrl(url, httpsOnly = true) == null) {
            throw KSMHostException("INVALID_ARGS", "url must use HTTPS.")
        }
        if (
            !KSMVirtualPath.isSafe(path) ||
            KSMVirtualPath.isReadOnly(path) ||
            KSMVirtualPath.isPicked(path)
        ) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "path is outside the writable sandbox."
            )
        }
        val overwrite = args.opt("overwrite")
        if (overwrite != null && overwrite !is Boolean) {
            throw KSMHostException(
                "INVALID_ARGS",
                "overwrite must be a boolean."
            )
        }
        val result = host.download(
            url,
            path,
            overwrite == true,
            context.requestId
        ) ?: throw KSMHostException(
            "IO_ERROR",
            "The download could not be completed."
        )
        return JSONObject()
            .put("path", result.path)
            .put("bytes", result.bytes)
    }
}
