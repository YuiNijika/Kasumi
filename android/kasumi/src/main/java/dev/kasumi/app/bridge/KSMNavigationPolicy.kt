package dev.kasumi.app.bridge

object KSMNavigationPolicy {
    fun isAllowedOrigin(
        scheme: String?,
        host: String?,
        port: Int,
        allowDevelopmentOrigin: Boolean = false
    ): Boolean {
        val isAppOrigin = scheme.equals("https", ignoreCase = true) &&
            host.equals("app.local", ignoreCase = true) &&
            port == -1
        val isDevelopmentOrigin = scheme.equals("http", ignoreCase = true) &&
            host.equals("localhost", ignoreCase = true) &&
            port == 5173
        return isAppOrigin || (allowDevelopmentOrigin && isDevelopmentOrigin)
    }
}
