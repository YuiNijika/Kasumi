package dev.kasumi.app.bridge

import org.json.JSONObject

class KSMDialogPlugin : KSMPlugin {
    override val id: String = "dialog"
    override val commands: Set<String> = setOf(
        "dialog.message",
        "dialog.confirm",
        "dialog.openFile",
        "dialog.saveFile"
    )
    override val permissions: Set<String> = setOf("dialog")

    override fun handle(
        command: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any {
        if (!context.hasPermission("dialog")) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The requested permission was denied."
            )
        }
        val host = context.hostServices ?: throw KSMHostException(
            "NOT_SUPPORTED",
            "The dialog service is not configured."
        )
        return when (command) {
            "dialog.message" -> {
                val title = requiredString(args, "title")
                val message = requiredString(args, "message")
                JSONObject().put("confirmed", host.showMessage(title, message))
            }
            "dialog.confirm" -> {
                val title = requiredString(args, "title")
                val message = requiredString(args, "message")
                JSONObject().put("confirmed", host.confirm(title, message))
            }
            "dialog.openFile" -> openFile(host, args)
            else -> saveFile(host, args)
        }
    }

    private fun openFile(
        host: KSMHostServices,
        args: JSONObject
    ): JSONObject {
        val filters = mutableListOf<String>()
        if (args.has("filters")) {
            val raw = args.optJSONArray("filters")
                ?: throw KSMHostException(
                    "INVALID_ARGS",
                    "filters must be an array."
                )
            for (index in 0 until raw.length()) {
                val filter = raw.opt(index)
                if (filter !is String) {
                    throw KSMHostException(
                        "INVALID_ARGS",
                        "filters must contain strings."
                    )
                }
                filters.add(filter)
            }
        }
        return JSONObject().put(
            "file",
            host.openFile(filters) ?: JSONObject.NULL
        )
    }

    private fun saveFile(
        host: KSMHostServices,
        args: JSONObject
    ): JSONObject {
        val suggested = if (args.has("suggestedName") && !args.isNull("suggestedName")) {
            args.opt("suggestedName")
        } else {
            null
        }
        if (suggested != null && suggested !is String) {
            throw KSMHostException(
                "INVALID_ARGS",
                "suggestedName must be a string."
            )
        }
        return JSONObject().put(
            "file",
            host.saveFile(suggested ?: "") ?: JSONObject.NULL
        )
    }

    private fun requiredString(args: JSONObject, field: String): String {
        val value = args.opt(field)
        if (value !is String) {
            throw KSMHostException(
                "INVALID_ARGS",
                "$field is required."
            )
        }
        return value
    }
}
