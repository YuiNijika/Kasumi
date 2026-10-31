package dev.kasumi.app.bridge

import android.content.Context
import android.net.Uri
import android.webkit.MimeTypeMap
import android.webkit.WebResourceResponse
import androidx.webkit.WebViewAssetLoader

object KSMWebAssetLoader {
    const val APP_DOMAIN = "app.local"

    fun create(context: Context, assetRoot: String = ""): WebViewAssetLoader {
        val normalizedRoot = assetRoot.trim('/')
        require(
            !normalizedRoot.contains("..") &&
                !normalizedRoot.contains('\\') &&
                !normalizedRoot.contains('\u0000')
        ) {
            "assetRoot must be a safe relative asset directory"
        }

        return WebViewAssetLoader.Builder()
            .setDomain(APP_DOMAIN)
            .addPathHandler("/", object : WebViewAssetLoader.PathHandler {
                override fun handle(path: String): WebResourceResponse? {
                    val decoded = Uri.decode(path)
                    if (
                        decoded.contains("..") ||
                        decoded.contains('\\') ||
                        decoded.contains('\u0000')
                    ) {
                        return null
                    }

                    val normalized = decoded.trimStart('/')
                    val relative = if (normalized.isEmpty() || normalized.endsWith('/')) {
                        "${normalized}index.html"
                    } else {
                        normalized
                    }
                    val assetPath = if (normalizedRoot.isEmpty()) {
                        relative
                    } else {
                        "$normalizedRoot/$relative"
                    }
                    val mimeType = mimeTypeFor(relative)

                    return try {
                        WebResourceResponse(
                            mimeType,
                            if (isTextMimeType(mimeType)) "UTF-8" else null,
                            context.assets.open(assetPath)
                        )
                    } catch (_: Exception) {
                        null
                    }
                }
            })
            .build()
    }

    private fun mimeTypeFor(path: String): String {
        val extension = path.substringAfterLast('.', "").lowercase()
        return MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension)
            ?: when (extension) {
                "js", "mjs" -> "text/javascript"
                "json" -> "application/json"
                "svg" -> "image/svg+xml"
                else -> "application/octet-stream"
            }
    }

    private fun isTextMimeType(mimeType: String): Boolean {
        return mimeType.startsWith("text/") ||
            mimeType == "application/javascript" ||
            mimeType == "application/json" ||
            mimeType == "image/svg+xml"
    }
}
