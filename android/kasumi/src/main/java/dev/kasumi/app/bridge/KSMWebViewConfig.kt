package dev.kasumi.app.bridge

import java.net.URI

class KSMWebViewConfig(
    productionEntry: String,
    developmentEntries: Collection<String>,
    supportedHtmlFiles: Collection<String>
) {
    private val htmlFilePattern = Regex(
        "^(?:[A-Za-z0-9._-]+/)*[A-Za-z0-9._-]+\\.html$"
    )
    val productionEntry: String = parseProductionEntry(productionEntry)
    val developmentEntries: Set<String>
    val supportedHtmlFiles: Set<String>
    val productionOrigin: String = originOf(this.productionEntry)
    val developmentOrigins: Set<String>
    val productionHtmlFile: String

    init {
        require(supportedHtmlFiles.isNotEmpty()) {
            "supportedHtmlFiles must not be empty"
        }
        this.supportedHtmlFiles = supportedHtmlFiles.mapTo(linkedSetOf()) {
            require(htmlFilePattern.matches(it)) {
                "Unsupported HTML file: $it"
            }
            it
        }
        productionHtmlFile = htmlFileOf(this.productionEntry)
        require(this.productionEntry.endsWith("/$productionHtmlFile")) {
            "productionEntry must use a safe HTML file name"
        }
        require(this.supportedHtmlFiles.contains(productionHtmlFile)) {
            "productionEntry must point to a supported HTML file"
        }
        require(developmentEntries.isNotEmpty()) {
            "developmentEntries must not be empty"
        }
        this.developmentEntries = developmentEntries.mapTo(linkedSetOf()) {
            val parsed = parseEntryUrl(it, "http", "localhost", 5173)
            val htmlFile = parsed.rawPath.orEmpty().removePrefix("/")
            require(
                htmlFile.isEmpty() ||
                    this.supportedHtmlFiles.contains(htmlFile)
            ) {
                "Unsupported development entry: $it"
            }
            it
        }
        require(
            this.developmentEntries.any {
                it.endsWith("/$productionHtmlFile")
            }
        ) {
            "developmentEntries must include the production HTML file"
        }
        developmentOrigins = this.developmentEntries
            .mapTo(linkedSetOf()) { originOf(it) }
    }

    fun resolveHtmlFile(path: String): String? {
        val normalized = path.removePrefix("/")
        return when {
            normalized.isEmpty() ||
                normalized.endsWith('/') -> productionHtmlFile
            supportedHtmlFiles.contains(normalized) -> normalized
            else -> null
        }
    }

    fun isAllowedNavigation(url: String): Boolean {
        val parsed = parseUrl(url) ?: return false
        val origin = originOf(parsed)
        val allowedOrigins = if (origin == productionOrigin) {
            setOf(productionOrigin)
        } else {
            developmentOrigins
        }
        if (origin !in allowedOrigins) {
            return false
        }
        return resolveHtmlFile(parsed.rawPath.orEmpty()) != null
    }

    fun isAllowedDevelopmentEntry(url: String): Boolean {
        val parsed = parseUrl(url) ?: return false
        if (originOf(parsed) !in developmentOrigins) {
            return false
        }
        return developmentEntries.contains(url) &&
            resolveHtmlFile(parsed.rawPath.orEmpty()) != null
    }

    private fun parseProductionEntry(value: String): String {
        val parsed = parseEntryUrl(value, "https", "app.local", -1)
        require(htmlFileOf(value).endsWith(".html")) {
            "productionEntry must point to an HTML file"
        }
        return value
    }

    private fun parseEntryUrl(
        value: String,
        scheme: String,
        host: String,
        port: Int
    ): URI {
        val parsed = parseUrl(value)
            ?: throw IllegalArgumentException("Invalid WebView entry: $value")
        require(
            parsed.scheme.equals(scheme, ignoreCase = true) &&
                parsed.host.equals(host, ignoreCase = true) &&
                parsed.port == port &&
                parsed.userInfo == null &&
                isSafePath(parsed.rawPath.orEmpty())
        ) {
            "Invalid WebView entry: $value"
        }
        return parsed
    }

    private fun parseUrl(value: String): URI? {
        val parsed = try {
            URI(value)
        } catch (_: IllegalArgumentException) {
            return null
        }
        if (
            parsed.toString() != value ||
            value.contains('?') ||
            value.contains('#')
        ) {
            return null
        }
        return parsed.takeIf {
            it.isAbsolute &&
                it.scheme != null &&
                it.host != null &&
                it.rawQuery == null &&
                it.rawFragment == null &&
                !it.rawPath.orEmpty().contains('%') &&
                !it.rawPath.orEmpty().contains('\\') &&
                !it.rawPath.orEmpty().contains('\u0000')
        }
    }

    private fun isSafePath(path: String): Boolean {
        return path.isEmpty() ||
            Regex("^/(?:[A-Za-z0-9._-]+/)*[A-Za-z0-9._-]*$").matches(path)
    }

    private fun originOf(url: String): String {
        return originOf(
            parseUrl(url) ?: throw IllegalArgumentException("Invalid WebView origin: $url")
        )
    }

    private fun originOf(parsed: URI): String {
        return originFor(parsed.scheme, parsed.host, parsed.port)
    }

    private fun htmlFileOf(url: String): String {
        val parsed = parseUrl(url)
            ?: throw IllegalArgumentException("Invalid WebView entry: $url")
        return parsed.rawPath.orEmpty().removePrefix("/")
    }

    companion object {
        fun originFor(scheme: String?, host: String?, port: Int): String {
            val normalizedScheme = scheme?.lowercase() ?: return ""
            val normalizedHost = host?.lowercase() ?: return ""
            val defaultPort = if (normalizedScheme == "https") 443 else 80
            val effectivePort = port.takeIf { it != -1 && it != defaultPort }
            val portText = effectivePort?.let { ":$it" } ?: ""
            return "$normalizedScheme://$normalizedHost$portText"
        }

        fun default(): KSMWebViewConfig {
            return KSMWebViewConfig(
                productionEntry = "https://app.local/index.html",
                developmentEntries = listOf(
                    "http://localhost:5173/index.html",
                    "http://localhost:5173/",
                    "http://localhost:5173"
                ),
                supportedHtmlFiles = listOf("index.html")
            )
        }
    }
}
