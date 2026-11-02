package dev.kasumi.app.bridge

import org.json.JSONArray
import org.json.JSONObject

data class KSMPathDirectories(
    val data: String,
    val cache: String,
    val temp: String,
    val resource: String,
    val asset: String
)

object KSMPath {
    fun dirs(): KSMPathDirectories? {
        val data = invokeData("path.dirs", JSONObject()) ?: return null
        return KSMPathDirectories(
            data = data.getString("data"),
            cache = data.getString("cache"),
            temp = data.getString("temp"),
            resource = data.getString("resource"),
            asset = data.getString("asset")
        )
    }

    fun join(base: String, segments: List<String>): String? {
        val args = JSONObject()
            .put("base", base)
            .put("segments", JSONArray(segments))
        return invokeData("path.join", args)?.getString("path")
    }

    fun normalize(path: String): String? {
        return invokeData("path.normalize", JSONObject().put("path", path))
            ?.getString("path")
    }

    fun dirname(path: String): String? {
        return invokeData("path.dirname", JSONObject().put("path", path))
            ?.getString("path")
    }

    fun basename(path: String, suffix: String? = null): String? {
        val args = JSONObject().put("path", path)
        if (suffix != null) {
            args.put("suffix", suffix)
        }
        return invokeData("path.basename", args)?.getString("name")
    }

    fun extname(path: String): String? {
        return invokeData("path.extname", JSONObject().put("path", path))
            ?.getString("extension")
    }

    fun isAbsolute(path: String): Boolean? {
        return invokeData("path.isAbsolute", JSONObject().put("path", path))
            ?.getBoolean("absolute")
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
