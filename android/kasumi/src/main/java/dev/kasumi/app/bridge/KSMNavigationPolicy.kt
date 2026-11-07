package dev.kasumi.app.bridge

object KSMNavigationPolicy {
    fun isAllowedDevelopmentUrl(
        url: String,
        config: KSMWebViewConfig = KSMWebViewConfig.default()
    ): Boolean {
        return config.isAllowedDevelopmentEntry(url)
    }

    fun isAllowedOrigin(
        scheme: String?,
        host: String?,
        port: Int,
        allowDevelopmentOrigin: Boolean = false,
        config: KSMWebViewConfig = KSMWebViewConfig.default()
    ): Boolean {
        val origin = KSMWebViewConfig.originFor(scheme, host, port)
        return origin == config.productionOrigin ||
            (allowDevelopmentOrigin && config.developmentOrigins.contains(origin))
    }
}
