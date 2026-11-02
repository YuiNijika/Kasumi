package dev.kasumi.app.bridge

import org.json.JSONObject

object KSMLog {
    fun write(
        level: String,
        message: String,
        target: String? = null
    ): Boolean {
        if (!KSM.isCommandEnabled("log.write")) {
            return false
        }
        val args = JSONObject()
            .put("level", level)
            .put("message", message)
        if (target != null) {
            args.put("target", target)
        }
        return KSM.invoke("log.write", args).optBoolean("ok")
    }
}
