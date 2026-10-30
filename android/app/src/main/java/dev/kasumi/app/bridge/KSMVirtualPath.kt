package dev.kasumi.app.bridge

import java.io.File

object KSMVirtualPath {
    private const val PICKED_PREFIX = "app://picked/"
    private const val RESOURCE_PREFIX = "app://res/"
    private const val ASSET_PREFIX = "app://assets/"
    private val roots = setOf(
        "app://data",
        "app://cache",
        "app://tmp",
        "app://picked",
        "app://res",
        "app://assets"
    )
    private val prefixes = listOf(
        "app://data/",
        "app://cache/",
        "app://tmp/",
        PICKED_PREFIX,
        RESOURCE_PREFIX,
        ASSET_PREFIX
    )

    fun isSafe(path: String): Boolean {
        if (
            path.contains('\u0000') ||
            path.contains("..") ||
            path.contains('~') ||
            path.startsWith("/") ||
            path.startsWith("\\") ||
            Regex("^[A-Za-z]:").containsMatchIn(path)
        ) {
            return false
        }
        if (
            path == "app://data" ||
            path == "app://cache" ||
            path == "app://tmp" ||
            path == "app://picked" ||
            path == "app://res" ||
            path == "app://assets"
        ) {
            return false
        }
        val prefix = prefixes.firstOrNull { path.startsWith(it) } ?: return false
        val relative = path.removePrefix(prefix)
        if (path.startsWith(PICKED_PREFIX)) {
            return isSafeRelative(relative) && !relative.contains('/')
        }
        return isSafeRelative(relative)
    }

    fun isSafeDirectory(path: String): Boolean {
        if (
            path.contains('\u0000') ||
            path.contains("..") ||
            path.contains('~') ||
            path.startsWith("/") ||
            path.startsWith("\\") ||
            Regex("^[A-Za-z]:").containsMatchIn(path) ||
            path.endsWith("//") ||
            path == "app://picked" ||
            path.startsWith(PICKED_PREFIX)
        ) {
            return false
        }
        val normalized = normalizeDirectory(path)
        return normalized in roots || isSafe(normalized)
    }

    fun isSafeStat(path: String): Boolean {
        if (
            path.contains('\u0000') ||
            path.contains("..") ||
            path.contains('~') ||
            path.startsWith("/") ||
            path.startsWith("\\") ||
            Regex("^[A-Za-z]:").containsMatchIn(path) ||
            path.endsWith("//")
        ) {
            return false
        }
        val normalized = normalizeDirectory(path)
        return normalized != "app://picked" &&
            (normalized in roots || isSafe(normalized))
    }

    fun normalizeDirectory(path: String): String {
        return path.trimEnd('/')
    }

    private fun isSafeRelative(relative: String): Boolean {
        return relative.isNotEmpty() &&
            !relative.contains('\u0000') &&
            !relative.contains("..") &&
            !relative.contains('~') &&
            !relative.contains(':') &&
            !relative.contains('\\') &&
            !relative.startsWith("/") &&
            relative.split('/').none { it.isEmpty() || it == "." }
    }

    fun isPicked(path: String): Boolean {
        if (!isSafe(path) || !path.startsWith(PICKED_PREFIX)) {
            return false
        }
        val token = path.removePrefix(PICKED_PREFIX)
        return token.isNotEmpty() && !token.contains('/')
    }

    fun pickedPath(token: String): String = "$PICKED_PREFIX$token"

    fun isReadOnly(path: String): Boolean {
        return path.startsWith(RESOURCE_PREFIX) ||
            path.startsWith(ASSET_PREFIX)
    }

    fun bundledAssetPath(path: String): String? {
        if (!isSafe(path)) {
            return null
        }
        if (path.startsWith(RESOURCE_PREFIX)) {
            return "res/${path.removePrefix(RESOURCE_PREFIX)}"
        }
        if (path.startsWith(ASSET_PREFIX)) {
            return "web/${path.removePrefix(ASSET_PREFIX)}"
        }
        return null
    }

    fun bundledAssetDirectoryPath(path: String): String? {
        if (!isSafeDirectory(path)) {
            return null
        }
        val normalized = normalizeDirectory(path)
        return when {
            normalized == "app://res" -> "res"
            normalized == "app://assets" -> "web"
            normalized.startsWith(RESOURCE_PREFIX) ->
                "res/${normalized.removePrefix(RESOURCE_PREFIX)}"
            normalized.startsWith(ASSET_PREFIX) ->
                "web/${normalized.removePrefix(ASSET_PREFIX)}"
            else -> null
        }
    }

    fun resolveWritable(
        path: String,
        dataRoot: File,
        cacheRoot: File,
        tempRoot: File
    ): File? {
        if (!isSafe(path)) {
            return null
        }
        val roots = listOf(
            "app://data/" to dataRoot,
            "app://cache/" to cacheRoot,
            "app://tmp/" to tempRoot
        )
        return roots.firstNotNullOfOrNull { (prefix, root) ->
            if (!path.startsWith(prefix)) {
                return@firstNotNullOfOrNull null
            }
            val relative = path.removePrefix(prefix)
            if (!isSafeRelative(relative)) {
                return@firstNotNullOfOrNull null
            }
            File(root, relative)
        }
    }

    fun resolveWritableDirectory(
        path: String,
        dataRoot: File,
        cacheRoot: File,
        tempRoot: File
    ): File? {
        if (!isSafeDirectory(path)) {
            return null
        }
        val normalized = normalizeDirectory(path)
        val roots = listOf(
            "app://data" to dataRoot,
            "app://cache" to cacheRoot,
            "app://tmp" to tempRoot
        )
        return roots.firstNotNullOfOrNull { (rootPath, root) ->
            when {
                normalized == rootPath -> root
                normalized.startsWith("$rootPath/") -> {
                    val relative = normalized.removePrefix("$rootPath/")
                    if (isSafeRelative(relative)) File(root, relative) else null
                }
                else -> null
            }
        }
    }
}
