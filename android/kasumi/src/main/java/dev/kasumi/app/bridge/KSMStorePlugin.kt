package dev.kasumi.app.bridge

import org.json.JSONArray
import org.json.JSONObject

class KSMStorePlugin : KSMPlugin {
    override val id: String = "store"
    override val commands: Set<String> = setOf(
        "store.get",
        "store.set",
        "store.delete",
        "store.clear",
        "store.keys"
    )
    override val permissions: Set<String> = setOf("store")

    companion object {
        private const val STORE_PATH = "app://data/.kasumi/store.json"
        private const val MAX_KEY_BYTES = 256
        private const val MAX_VALUE_BYTES = 1024 * 1024
        private const val MAX_STORE_BYTES = 16 * 1024 * 1024
        private val mutex = Any()
    }

    override fun handle(
        command: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any {
        requirePermission(context)
        synchronized(mutex) {
            return when (command) {
                "store.get" -> get(context, requiredKey(args))
                "store.set" -> set(context, requiredKey(args), args.opt("value"))
                "store.delete" -> delete(context, requiredKey(args))
                "store.clear" -> clear(context)
                else -> JSONObject().put("keys", JSONArray(load(context).keys().asSequence().sorted().toList()))
            }
        }
    }

    private fun get(
        context: KSMPluginContext,
        key: String
    ): JSONObject {
        val root = load(context)
        return if (root.has(key)) {
            JSONObject().put("found", true).put("value", root.opt(key))
        } else {
            JSONObject().put("found", false).put("value", JSONObject.NULL)
        }
    }

    private fun set(
        context: KSMPluginContext,
        key: String,
        value: Any?
    ): JSONObject {
        if (value == null) {
            throw KSMHostException("INVALID_ARGS", "value is required.")
        }
        assertJsonValue(value, 0)
        if (value.toString().toByteArray(Charsets.UTF_8).size > MAX_VALUE_BYTES) {
            throw KSMHostException("INVALID_ARGS", "value must not exceed 1 MiB.")
        }
        val root = load(context)
        root.put(key, value)
        save(context, root)
        return JSONObject().put("updated", true)
    }

    private fun delete(
        context: KSMPluginContext,
        key: String
    ): JSONObject {
        val root = load(context)
        if (!root.has(key)) {
            return JSONObject().put("deleted", false)
        }
        root.remove(key)
        save(context, root)
        return JSONObject().put("deleted", true)
    }

    private fun clear(context: KSMPluginContext): JSONObject {
        val fileSystem = requireFileSystem(context)
        if (fileSystem.exists(STORE_PATH) && !fileSystem.remove(STORE_PATH)) {
            throw KSMHostException("IO_ERROR", "The store could not be cleared.")
        }
        return JSONObject().put("cleared", true)
    }

    private fun load(context: KSMPluginContext): JSONObject {
        val fileSystem = requireFileSystem(context)
        if (!fileSystem.exists(STORE_PATH)) {
            return JSONObject()
        }
        val text = fileSystem.readText(STORE_PATH)
            ?: throw KSMHostException("IO_ERROR", "The store could not be read.")
        return try {
            JSONObject(text)
        } catch (_: Exception) {
            throw KSMHostException("IO_ERROR", "The store contains invalid JSON.")
        }
    }

    private fun save(
        context: KSMPluginContext,
        root: JSONObject
    ) {
        val text = root.toString()
        if (text.toByteArray(Charsets.UTF_8).size > MAX_STORE_BYTES) {
            throw KSMHostException("IO_ERROR", "The store exceeds 16 MiB.")
        }
        if (!requireFileSystem(context).writeText(STORE_PATH, text)) {
            throw KSMHostException("IO_ERROR", "The store could not be written.")
        }
    }

    private fun requiredKey(args: JSONObject): String {
        val key = args.opt("key")
        if (
            key !is String ||
            key.isEmpty() ||
            key.toByteArray(Charsets.UTF_8).size > MAX_KEY_BYTES
        ) {
            throw KSMHostException(
                "INVALID_ARGS",
                "key must contain between 1 and 256 bytes."
            )
        }
        return key
    }

    private fun assertJsonValue(value: Any?, depth: Int) {
        if (depth > 32) {
            throw KSMHostException("INVALID_ARGS", "value exceeds the maximum JSON depth.")
        }
        when (value) {
            null,
            JSONObject.NULL,
            is String,
            is Boolean -> return
            is Number -> {
                if (
                    value is Double && !value.isFinite() ||
                    value is Float && !value.isFinite()
                ) {
                    throw KSMHostException(
                        "INVALID_ARGS",
                        "value must contain only finite numbers."
                    )
                }
                return
            }
            is JSONObject -> {
                val keys = value.keys()
                while (keys.hasNext()) {
                    assertJsonValue(value.opt(keys.next()), depth + 1)
                }
                return
            }
            is JSONArray -> {
                for (index in 0 until value.length()) {
                    assertJsonValue(value.opt(index), depth + 1)
                }
                return
            }
            else -> throw KSMHostException(
                "INVALID_ARGS",
                "value must be a JSON value."
            )
        }
    }

    private fun requirePermission(context: KSMPluginContext) {
        if (!context.hasPermission("store")) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The requested permission was denied."
            )
        }
    }

    private fun requireFileSystem(context: KSMPluginContext): KSMFileSystem {
        return context.fileSystem
            ?: throw KSMHostException(
                "NOT_SUPPORTED",
                "The filesystem is not configured."
            )
    }
}
