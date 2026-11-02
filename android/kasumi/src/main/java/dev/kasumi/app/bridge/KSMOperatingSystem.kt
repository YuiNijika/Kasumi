package dev.kasumi.app.bridge

import org.json.JSONObject

object KSMOperatingSystem {
    fun getInfo(): KSMOperatingSystemInfo? {
        if (!KSM.isCommandEnabled("os.getInfo")) {
            return null
        }
        val response = KSM.invoke("os.getInfo", JSONObject())
        val data = response.optJSONObject("data") ?: return null
        return KSMOperatingSystemInfo(
            platform = data.getString("platform"),
            architecture = data.getString("architecture"),
            version = data.getString("version"),
            locale = data.getString("locale"),
            timezone = data.getString("timezone")
        )
    }
}
