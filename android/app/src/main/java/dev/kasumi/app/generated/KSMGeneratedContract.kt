package dev.kasumi.app.generated

import org.json.JSONArray
import org.json.JSONObject

object KSMGeneratedContract {
    val commands: List<String> = listOf(
        "ping",
        "capabilities.query",
        "app.getInfo",
        "fs.readText",
        "fs.writeText",
        "fs.exists",
        "fs.remove",
        "fs.readDir",
        "fs.mkdir",
        "fs.stat",
        "dialog.message",
        "dialog.confirm",
        "dialog.openFile",
        "dialog.saveFile",
        "clipboard.readText",
        "clipboard.writeText",
        "notification.show",
        "permission.request",
        "event.subscribe",
        "event.unsubscribe",
        "window.setTitle",
        "window.minimize",
        "window.maximize",
        "window.close",
        "app.exit"
    )

    val windowsCommands: List<String> = listOf(
        "ping",
        "capabilities.query",
        "app.getInfo",
        "fs.readText",
        "fs.writeText",
        "fs.exists",
        "fs.remove",
        "fs.readDir",
        "fs.mkdir",
        "fs.stat",
        "dialog.message",
        "dialog.confirm",
        "dialog.openFile",
        "dialog.saveFile",
        "clipboard.readText",
        "clipboard.writeText",
        "notification.show",
        "permission.request",
        "event.subscribe",
        "event.unsubscribe",
        "window.setTitle",
        "window.minimize",
        "window.maximize",
        "window.close",
        "app.exit"
    )

    val androidCommands: List<String> = listOf(
        "ping",
        "capabilities.query",
        "app.getInfo",
        "fs.readText",
        "fs.writeText",
        "fs.exists",
        "fs.remove",
        "fs.readDir",
        "fs.mkdir",
        "fs.stat",
        "dialog.message",
        "dialog.confirm",
        "dialog.openFile",
        "dialog.saveFile",
        "clipboard.readText",
        "clipboard.writeText",
        "notification.show",
        "permission.request",
        "event.subscribe",
        "event.unsubscribe",
        "window.close",
        "app.exit"
    )

    val events: List<String> = listOf(
        "webview.reloaded",
        "window.resized"
    )

    val permissions: List<String> = listOf(
        "fs.read",
        "fs.write",
        "fs.remove",
        "dialog",
        "clipboard.read",
        "clipboard.write",
        "notification.show",
        "window.control"
    )

    private const val ARG_SCHEMAS_JSON = """{
  "ping": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "capabilities.query": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "app.getInfo": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "fs.readText": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.writeText": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      },
      "content": {
        "type": "string",
        "maxBytes": 16777216
      }
    },
    "required": [
      "path",
      "content"
    ],
    "additionalProperties": false
  },
  "fs.exists": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.remove": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.readDir": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.mkdir": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      },
      "recursive": {
        "type": "boolean"
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.stat": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "dialog.message": {
    "type": "object",
    "properties": {
      "title": {
        "type": "string"
      },
      "message": {
        "type": "string"
      }
    },
    "required": [
      "title",
      "message"
    ],
    "additionalProperties": false
  },
  "dialog.confirm": {
    "type": "object",
    "properties": {
      "title": {
        "type": "string"
      },
      "message": {
        "type": "string"
      }
    },
    "required": [
      "title",
      "message"
    ],
    "additionalProperties": false
  },
  "dialog.openFile": {
    "type": "object",
    "properties": {
      "filters": {
        "type": "array",
        "items": {
          "type": "string"
        }
      }
    },
    "required": [],
    "additionalProperties": false
  },
  "dialog.saveFile": {
    "type": "object",
    "properties": {
      "suggestedName": {
        "type": "string"
      }
    },
    "required": [],
    "additionalProperties": false
  },
  "clipboard.readText": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "clipboard.writeText": {
    "type": "object",
    "properties": {
      "text": {
        "type": "string"
      }
    },
    "required": [
      "text"
    ],
    "additionalProperties": false
  },
  "notification.show": {
    "type": "object",
    "properties": {
      "title": {
        "type": "string"
      },
      "body": {
        "type": "string"
      }
    },
    "required": [
      "title",
      "body"
    ],
    "additionalProperties": false
  },
  "permission.request": {
    "type": "object",
    "properties": {
      "permission": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "permission"
    ],
    "additionalProperties": false
  },
  "event.subscribe": {
    "type": "object",
    "properties": {
      "event": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "event"
    ],
    "additionalProperties": false
  },
  "event.unsubscribe": {
    "type": "object",
    "properties": {
      "event": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "event"
    ],
    "additionalProperties": false
  },
  "window.setTitle": {
    "type": "object",
    "properties": {
      "title": {
        "type": "string"
      }
    },
    "required": [
      "title"
    ],
    "additionalProperties": false
  },
  "window.minimize": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "window.maximize": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "window.close": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "app.exit": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  }
}"""
    private val argSchemas = JSONObject(ARG_SCHEMAS_JSON)

    fun isKnownCommand(command: String): Boolean = argSchemas.has(command)

    fun validateArgs(command: String, args: JSONObject): Boolean {
        val schema = argSchemas.optJSONObject(command) ?: return false
        return validateValue(args, schema)
    }

    private fun validateValue(value: Any?, schema: JSONObject): Boolean {
        val rawType = schema.opt("type")
        val types: List<String> = when (rawType) {
            is String -> listOf(rawType)
            is JSONArray -> (0 until rawType.length()).mapNotNull { rawType.optString(it, "") }
            else -> emptyList()
        }
        if (types.isEmpty() || types.none { matchesType(value, it) }) {
            return false
        }

        if (schema.has("enum")) {
            val candidates = schema.optJSONArray("enum") ?: return false
            var found = false
            for (index in 0 until candidates.length()) {
                if (sameValue(candidates.opt(index), value)) {
                    found = true
                    break
                }
            }
            if (!found) return false
        }
        if (schema.has("const") && !sameValue(schema.opt("const"), value)) {
            return false
        }

        if (value is JSONObject) {
            val properties = schema.optJSONObject("properties") ?: JSONObject()
            val required = schema.optJSONArray("required")
            if (required != null) {
                for (index in 0 until required.length()) {
                    if (!value.has(required.optString(index))) return false
                }
            }
            if (schema.optBoolean("additionalProperties", true).not()) {
                val keys = value.keys()
                while (keys.hasNext()) {
                    if (!properties.has(keys.next())) return false
                }
            }
            val propertyKeys = properties.keys()
            while (propertyKeys.hasNext()) {
                val field = propertyKeys.next()
                val propertySchema = properties.optJSONObject(field) ?: continue
                if (value.has(field) && !validateValue(value.opt(field), propertySchema)) {
                    return false
                }
            }
        }

        if (value is JSONArray && schema.has("items")) {
            val itemSchema = schema.optJSONObject("items") ?: return false
            for (index in 0 until value.length()) {
                if (!validateValue(value.opt(index), itemSchema)) return false
            }
        }

        if (value is String) {
            if (schema.has("minLength") && value.length < schema.optInt("minLength")) {
                return false
            }
            if (
                schema.has("maxBytes") &&
                value.toByteArray(Charsets.UTF_8).size > schema.optInt("maxBytes")
            ) {
                return false
            }
        }
        return true
    }

    private fun matchesType(value: Any?, type: String): Boolean {
        return when (type) {
            "object" -> value is JSONObject
            "array" -> value is JSONArray
            "string" -> value is String
            "number" -> value is Number
            "integer" -> value is Number && value.toDouble() % 1.0 == 0.0
            "boolean" -> value is Boolean
            "null" -> value == null || value === JSONObject.NULL
            else -> false
        }
    }

    private fun sameValue(left: Any?, right: Any?): Boolean {
        return left == right || left?.toString() == right?.toString()
    }
}
