package dev.kasumi.app.bridge

import java.net.URL

object KSMHttpPolicy {
    const val MAX_URL_LENGTH = 8192

    data class Origin(
        val scheme: String,
        val host: String,
        val port: Int
    )

    fun parseHttpUrl(value: String, httpsOnly: Boolean = false): URL? {
        if (value.isEmpty() || value.length > MAX_URL_LENGTH) {
            return null
        }
        val url = try {
            URL(value)
        } catch (_: Exception) {
            return null
        }
        val scheme = url.protocol.lowercase()
        if (scheme != "https" && (httpsOnly || scheme != "http")) {
            return null
        }
        if (
            url.host.isNullOrBlank() ||
            url.userInfo != null ||
            url.port == 0 ||
            url.port > 65535
        ) {
            return null
        }
        return url
    }

    fun normalizeOrigins(values: Set<String>): Set<Origin> {
        return values.mapNotNull { value ->
            val url = parseHttpUrl(value) ?: return@mapNotNull null
            if (url.path.isNotEmpty() && url.path != "/") {
                return@mapNotNull null
            }
            if (url.query != null || url.ref != null) {
                return@mapNotNull null
            }
            originOf(url)
        }.toSet()
    }

    fun originOf(url: URL): Origin {
        val scheme = url.protocol.lowercase()
        val port = when {
            url.port >= 0 -> url.port
            scheme == "https" -> 443
            else -> 80
        }
        return Origin(scheme, url.host.lowercase(), port)
    }

    fun isAllowedExternalUrl(value: String, allowedOrigins: Set<Origin>): Boolean {
        val url = parseHttpUrl(value) ?: return false
        return originOf(url) in allowedOrigins
    }

    fun resolveHttpsRedirect(current: URL, location: String): URL? {
        val next = try {
            URL(current, location)
        } catch (_: Exception) {
            return null
        }
        return parseHttpUrl(next.toString(), httpsOnly = true)
    }
}
