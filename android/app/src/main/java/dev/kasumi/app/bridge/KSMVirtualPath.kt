package dev.kasumi.app.bridge

import java.io.File

object KSMVirtualPath {
    private const val PICKED_PREFIX = "app://picked/"

    fun isSafe(path: String): Boolean {
        return !path.contains('\u0000') &&
            !path.contains("..") &&
            !path.contains('~') &&
            !path.startsWith("/") &&
            !path.startsWith("\\") &&
            !Regex("^[A-Za-z]:").containsMatchIn(path)
    }

    fun isPicked(path: String): Boolean {
        if (!isSafe(path) || !path.startsWith(PICKED_PREFIX)) {
            return false
        }
        val token = path.removePrefix(PICKED_PREFIX)
        return token.isNotEmpty() && !token.contains('/')
    }

    fun pickedPath(token: String): String = "$PICKED_PREFIX$token"

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
            if (
                relative.isEmpty() ||
                relative.startsWith("/") ||
                relative.startsWith("\\")
            ) {
                return@firstNotNullOfOrNull null
            }
            File(root, relative)
        }
    }
}
