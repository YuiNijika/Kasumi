package dev.kasumi.app.bridge

import org.json.JSONObject

class KSMLogPlugin : KSMPlugin {
    override val id: String = "log"
    override val commands: Set<String> = setOf("log.write")
    override val permissions: Set<String> = setOf("log.write")

    override fun handle(
        command: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any {
        if (!context.hasPermission("log.write")) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The requested permission was denied."
            )
        }
        val host = context.hostServices
            ?: throw KSMHostException(
                "NOT_SUPPORTED",
                "The logger service is not configured."
            )
        val level = requiredString(args, "level")
        val message = requiredString(args, "message")
        val target = if (args.has("target")) {
            requiredString(args, "target")
        } else {
            ""
        }
        if (!host.writeLog(level, target, message)) {
            throw KSMHostException("IO_ERROR", "The message could not be logged.")
        }
        return JSONObject().put("logged", true)
    }

    private fun requiredString(args: JSONObject, field: String): String {
        val value = args.opt(field)
        if (value !is String || value.isEmpty()) {
            throw KSMHostException(
                "INVALID_ARGS",
                "$field is required."
            )
        }
        return value
    }
}
