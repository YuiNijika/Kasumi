package dev.kasumi.app.bridge

import java.net.Inet4Address
import java.net.Inet6Address
import java.net.InetAddress
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

    fun isAllowedDownloadUrl(value: String, allowedOrigins: Set<Origin>): Boolean {
        val url = parseHttpUrl(value, httpsOnly = true) ?: return false
        return url.protocol == "https" && originOf(url) in allowedOrigins
    }

    fun resolveHttpsRedirect(current: URL, location: String): URL? {
        val next = try {
            URL(current, location)
        } catch (_: Exception) {
            return null
        }
        return parseHttpUrl(next.toString(), httpsOnly = true)
    }

    fun isPublicHost(host: String): Boolean {
        val addresses = try {
            InetAddress.getAllByName(host)
        } catch (_: Exception) {
            return false
        }
        return addresses.isNotEmpty() && addresses.all(::isPublicAddress)
    }

    private fun isPublicAddress(address: InetAddress): Boolean {
        val bytes = address.address
        return when (address) {
            is Inet4Address -> isPublicIpv4(bytes)
            is Inet6Address -> isPublicIpv6(bytes)
            else -> false
        }
    }

    private fun isPublicIpv4(bytes: ByteArray): Boolean {
        val first = bytes[0].toInt() and 0xff
        val second = bytes[1].toInt() and 0xff
        return first != 0 &&
            first != 10 &&
            first != 127 &&
            !(first == 169 && second == 254) &&
            !(first == 172 && second in 16..31) &&
            !(first == 192 && second == 168) &&
            !(first == 100 && second in 64..127) &&
            first < 224
    }

    private fun isPublicIpv6(bytes: ByteArray): Boolean {
        val isIpv4Mapped = bytes.take(10).all { it == 0.toByte() } &&
            bytes[10] == 0xff.toByte() &&
            bytes[11] == 0xff.toByte()
        if (isIpv4Mapped) {
            return isPublicIpv4(bytes.copyOfRange(12, 16))
        }
        val isUnspecified = bytes.all { it == 0.toByte() }
        val isLoopback = bytes.take(15).all { it == 0.toByte() } &&
            bytes[15] == 1.toByte()
        val first = bytes[0].toInt() and 0xff
        val second = bytes[1].toInt() and 0xff
        return !isUnspecified &&
            !isLoopback &&
            (first and 0xfe) != 0xfc &&
            !(first == 0xfe && (second and 0xc0) == 0x80) &&
            (first and 0xf0) != 0xf0
    }
}
