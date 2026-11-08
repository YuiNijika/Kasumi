package dev.kasumi.app.bridge

import android.net.Uri
import android.webkit.MimeTypeMap
import android.webkit.WebResourceResponse
import java.io.ByteArrayInputStream

object KSMLocalResourceHandler {
    private const val APP_DOMAIN = "app.local"
    private const val FILE_PATH = "/__ksm_file"

    fun responseFor(
        url: Uri,
        fileSystem: KSMFileSystem
    ): WebResourceResponse? {
        if (
            url.scheme != "https" ||
            url.host != APP_DOMAIN ||
            url.path != FILE_PATH
        ) {
            return null
        }
        val path = url.getQueryParameter("path")
            ?: return error(400, "The file path is missing.")
        if (
            !path.startsWith("app://data/") &&
            !path.startsWith("app://cache/") &&
            !path.startsWith("app://tmp/")
        ) {
            return error(403, "The path is outside the readable sandbox.")
        }
        if (!KSMVirtualPath.isSafe(path)) {
            return error(403, "The path is outside the readable sandbox.")
        }
        val file = fileSystem.resolveReadableFile(path)
            ?: return error(404, "The file was not found.")
        return try {
            WebResourceResponse(
                mimeTypeFor(path),
                null,
                file.inputStream()
            )
        } catch (_: Exception) {
            error(404, "The file could not be opened.")
        }
    }

    private fun error(status: Int, message: String): WebResourceResponse {
        return WebResourceResponse(
            "text/plain",
            "UTF-8",
            status,
            message,
            mapOf("Cache-Control" to "no-store"),
            ByteArrayInputStream(message.toByteArray(Charsets.UTF_8))
        )
    }

    private fun mimeTypeFor(path: String): String {
        val extension = path.substringAfterLast('.', "").lowercase()
        return MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension)
            ?: when (extension) {
                "avif" -> "image/avif"
                "flac" -> "audio/flac"
                "m4a" -> "audio/mp4"
                "ogg", "opus" -> "audio/ogg"
                "wav" -> "audio/wav"
                "webp" -> "image/webp"
                else -> "application/octet-stream"
            }
    }
}
