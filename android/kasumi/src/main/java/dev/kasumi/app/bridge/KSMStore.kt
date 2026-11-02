package dev.kasumi.app.bridge

import org.json.JSONObject

data class KSMStoreGetResult(
    val found: Boolean,
    val value: Any?
)

object KSMStore {
    fun get(key: String): KSMStoreGetResult? {
        val data = invokeData("store.get", JSONObject().put("key", key))
            ?: return null
        val value = data.opt("value")
        return KSMStoreGetResult(
            found = data.getBoolean("found"),
            value = if (value === JSONObject.NULL) null else value
        )
    }

    fun set(key: String, value: Any?): Boolean {
        val data = invokeData(
            "store.set",
            JSONObject()
                .put("key", key)
                .put("value", value ?: JSONObject.NULL)
        ) ?: return false
        return data.optBoolean("updated")
    }

    fun delete(key: String): Boolean? {
        return invokeData("store.delete", JSONObject().put("key", key))
            ?.optBoolean("deleted")
    }

    fun clear(): Boolean {
        return invokeData("store.clear", JSONObject())?.optBoolean("cleared") == true
    }

    fun keys(): List<String>? {
        val data = invokeData("store.keys", JSONObject()) ?: return null
        val values = data.optJSONArray("keys") ?: return emptyList()
        return List(values.length()) { index -> values.getString(index) }
    }

    private fun invokeData(command: String, args: JSONObject): JSONObject? {
        if (!KSM.isCommandEnabled(command)) {
            return null
        }
        val response = KSM.invoke(command, args)
        return if (response.optBoolean("ok")) {
            response.optJSONObject("data")
        } else {
            null
        }
    }
}
