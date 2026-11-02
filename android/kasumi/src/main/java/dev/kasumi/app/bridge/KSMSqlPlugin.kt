package dev.kasumi.app.bridge

import org.json.JSONArray
import org.json.JSONObject

class KSMSqlPlugin : KSMPlugin {
    override val id: String = "sql"
    override val commands: Set<String> = setOf(
        "sql.drivers",
        "sql.open",
        "sql.query",
        "sql.execute",
        "sql.close"
    )
    override val permissions: Set<String> = setOf("sql")

    override fun handle(
        command: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any {
        if (!context.hasPermission("sql")) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The requested permission was denied."
            )
        }
        val host = context.hostServices ?: throw KSMHostException(
            "NOT_SUPPORTED",
            "The SQL service is not configured."
        )
        return when (command) {
            "sql.drivers" -> JSONObject()
                .put("drivers", JSONArray(host.sqlDrivers()))
            "sql.open" -> open(host, args)
            "sql.query" -> query(host, args)
            "sql.execute" -> execute(host, args)
            else -> close(host, args)
        }
    }

    private fun open(
        host: KSMHostServices,
        args: JSONObject
    ): JSONObject {
        val driver = requiredString(args, "driver")
        val url = requiredString(args, "url")
        val connectionId = host.sqlOpen(driver, url)
            ?: throw KSMHostException(
                "NOT_SUPPORTED",
                "The requested SQL driver is not available."
            )
        return JSONObject().put("connectionId", connectionId)
    }

    private fun query(
        host: KSMHostServices,
        args: JSONObject
    ): JSONObject {
        val connectionId = requiredString(args, "connectionId")
        val statement = requiredString(args, "statement")
        val result = host.sqlQuery(
            connectionId,
            statement,
            params(args)
        ) ?: throw KSMHostException(
            "IO_ERROR",
            "The SQL query could not be completed."
        )
        val rows = JSONArray()
        for (row in result.rows) {
            val value = JSONObject()
            for (index in result.columns.indices) {
                value.put(
                    result.columns[index],
                    row.getOrNull(index) ?: JSONObject.NULL
                )
            }
            rows.put(value)
        }
        return JSONObject()
            .put("columns", JSONArray(result.columns))
            .put("rows", rows)
    }

    private fun execute(
        host: KSMHostServices,
        args: JSONObject
    ): JSONObject {
        val connectionId = requiredString(args, "connectionId")
        val statement = requiredString(args, "statement")
        val result = host.sqlExecute(
            connectionId,
            statement,
            params(args)
        ) ?: throw KSMHostException(
            "IO_ERROR",
            "The SQL statement could not be executed."
        )
        return JSONObject()
            .put("rowsAffected", result.rowsAffected)
            .put("lastInsertId", result.lastInsertId ?: JSONObject.NULL)
    }

    private fun close(
        host: KSMHostServices,
        args: JSONObject
    ): JSONObject {
        val connectionId = requiredString(args, "connectionId")
        return JSONObject().put("closed", host.sqlClose(connectionId))
    }

    private fun params(args: JSONObject): List<Any?> {
        if (!args.has("params")) {
            return emptyList()
        }
        val raw = args.optJSONArray("params")
            ?: throw KSMHostException(
                "INVALID_ARGS",
                "params must be an array."
            )
        val values = mutableListOf<Any?>()
        for (index in 0 until raw.length()) {
            values.add(
                when (val value = raw.opt(index)) {
                    null,
                    JSONObject.NULL -> null
                    is String,
                    is Boolean,
                    is Number -> value
                    else -> throw KSMHostException(
                        "INVALID_ARGS",
                        "params must contain strings, numbers, booleans or null."
                    )
                }
            )
        }
        return values
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
