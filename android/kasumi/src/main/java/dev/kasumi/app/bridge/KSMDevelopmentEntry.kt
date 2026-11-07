package dev.kasumi.app.bridge

import android.content.Intent

object KSMDevelopmentEntry {
    private const val EXTRA_DEV_URL = "ksmDevUrl"

    fun resolve(
        intent: Intent?,
        debug: Boolean,
        config: KSMWebViewConfig = KSMWebViewConfig.default()
    ): String {
        val requested = intent?.getStringExtra(EXTRA_DEV_URL)
            ?: return config.productionEntry
        if (!debug || !config.isAllowedDevelopmentEntry(requested)) {
            return config.productionEntry
        }
        return requested
    }
}
