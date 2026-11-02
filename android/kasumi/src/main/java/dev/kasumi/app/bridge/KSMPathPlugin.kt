package dev.kasumi.app.bridge

import org.json.JSONArray
import org.json.JSONObject

class KSMPathPlugin : KSMPlugin {
    override val id: String = "path"
    override val commands: Set<String> = setOf(
        "path.dirs",
        "path.join",
        "path.normalize",
        "path.dirname",
        "path.basename",
        "path.extname",
        "path.isAbsolute"
    )

    companion object {
        private const val APP_SCHEME_LENGTH = 6
        private val roots = listOf(
            "app://data",
            "app://cache",
            "app://tmp",
            "app://picked",
            "app://res",
            "app://assets"
        )

        private fun isSafeSegment(segment: String): Boolean {
            return segment.isNotEmpty() &&
                segment != "." &&
                segment != ".." &&
                !segment.contains('\u0000') &&
                !segment.contains('\\') &&
                !segment.contains(':') &&
                !segment.contains('~')
        }

        private fun normalize(raw: String): String? {
            if (raw.isEmpty() || raw.contains('\u0000')) {
                return null
            }
            val root = roots.firstOrNull {
                raw == it || raw.startsWith("$it/")
            } ?: return null
            if (raw == root || raw == "$root/") {
                return root
            }
            val relative = raw.removePrefix("$root/").trimEnd('/')
            if (relative.isEmpty() || relative.contains("..")) {
                return null
            }
            val segments = relative.split('/')
            if (segments.any { !isSafeSegment(it) }) {
                return null
            }
            if (root == "app://picked" && segments.size != 1) {
                return null
            }
            return "$root/${segments.joinToString("/")}"
        }

        private fun normalizeArg(args: JSONObject): String {
            val value = args.opt("path")
            if (value !is String || value.isEmpty()) {
                throw KSMHostException("INVALID_ARGS", "path is required.")
            }
            return normalize(value)
                ?: throw KSMHostException(
                    "INVALID_ARGS",
                    "path must be a Kasumi virtual path without traversal."
                )
        }

        private fun join(args: JSONObject): JSONObject {
            val base = args.opt("base")
            if (base !is String || base.isEmpty()) {
                throw KSMHostException("INVALID_ARGS", "base is required.")
            }
            val normalized = normalize(base)
                ?: throw KSMHostException(
                    "INVALID_ARGS",
                    "base must be a Kasumi virtual path without traversal."
                )
            val values = args.opt("segments")
            if (values !is JSONArray) {
                throw KSMHostException("INVALID_ARGS", "segments must be an array.")
            }
            val parts = mutableListOf<String>()
            for (index in 0 until values.length()) {
                val fragment = values.opt(index)
                if (
                    fragment !is String ||
                    fragment.isEmpty() ||
                    fragment.startsWith('/') ||
                    fragment.contains('\\') ||
                    fragment.contains(':') ||
                    fragment.contains('\u0000') ||
                    fragment.contains('~')
                ) {
                    throw KSMHostException(
                        "INVALID_ARGS",
                        "segments must be relative virtual path fragments."
                    )
                }
                val segments = fragment.split('/')
                if (segments.any { !isSafeSegment(it) }) {
                    throw KSMHostException(
                        "INVALID_ARGS",
                        "segments contain an invalid path component."
                    )
                }
                parts.addAll(segments)
            }
            val result = if (parts.isEmpty()) {
                normalized
            } else {
                "$normalized/${parts.joinToString("/")}"
            }
            return JSONObject().put(
                "path",
                normalize(result)
                    ?: throw KSMHostException(
                        "INVALID_ARGS",
                        "The joined path is invalid."
                    )
            )
        }

        private fun dirname(args: JSONObject): JSONObject {
            val path = normalizeArg(args)
            val separator = path.lastIndexOf('/')
            val parent = if (
                separator <= APP_SCHEME_LENGTH + 1
            ) {
                roots.first { path == it || path.startsWith("$it/") }
            } else {
                path.substring(0, separator)
            }
            return JSONObject().put("path", parent)
        }

        private fun basename(args: JSONObject): JSONObject {
            val path = normalizeArg(args)
            val separator = path.lastIndexOf('/')
            var name = if (
                separator <= APP_SCHEME_LENGTH + 1
            ) {
                ""
            } else {
                path.substring(separator + 1)
            }
            val suffix = args.opt("suffix")
            if (suffix != null && suffix !is String) {
                throw KSMHostException("INVALID_ARGS", "suffix must be a string.")
            }
            if (
                suffix is String &&
                suffix.isNotEmpty() &&
                name.length > suffix.length &&
                name.endsWith(suffix)
            ) {
                name = name.dropLast(suffix.length)
            }
            return JSONObject().put("name", name)
        }

        private fun extname(args: JSONObject): JSONObject {
            val path = normalizeArg(args)
            val separator = path.lastIndexOf('/')
            val dot = path.lastIndexOf('.')
            val nameStart = if (separator == -1) {
                APP_SCHEME_LENGTH
            } else {
                separator + 1
            }
            val extension = if (
                dot <= nameStart ||
                (separator != -1 && dot < separator)
            ) {
                ""
            } else {
                path.substring(dot)
            }
            return JSONObject().put("extension", extension)
        }
    }

    override fun handle(
        command: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any {
        return when (command) {
            "path.dirs" -> JSONObject()
                .put("data", "app://data")
                .put("cache", "app://cache")
                .put("temp", "app://tmp")
                .put("resource", "app://res")
                .put("asset", "app://assets")
            "path.join" -> join(args)
            "path.normalize" -> JSONObject().put("path", normalizeArg(args))
            "path.dirname" -> dirname(args)
            "path.basename" -> basename(args)
            "path.extname" -> extname(args)
            else -> {
                val path = args.opt("path")
                if (path !is String || path.isEmpty()) {
                    throw KSMHostException("INVALID_ARGS", "path is required.")
                }
                JSONObject().put("absolute", normalize(path) != null)
            }
        }
    }
}
