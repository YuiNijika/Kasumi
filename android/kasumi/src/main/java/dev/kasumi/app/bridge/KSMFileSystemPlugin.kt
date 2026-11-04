package dev.kasumi.app.bridge

import org.json.JSONArray
import org.json.JSONObject

class KSMFileSystemPlugin : KSMPlugin {
    override val id: String = "fs"
    override val commands: Set<String> = setOf(
        "fs.readText",
        "fs.writeText",
        "fs.appendText",
        "fs.copy",
        "fs.move",
        "fs.readBytes",
        "fs.writeBytes",
        "fs.exists",
        "fs.remove",
        "fs.readDir",
        "fs.mkdir",
        "fs.stat"
    )
    override val permissions: Set<String> = setOf(
        "fs.read",
        "fs.write",
        "fs.remove"
    )

    override fun handle(
        command: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any {
        val path = args.opt("path")
        if (path !is String || path.isEmpty()) {
            throw KSMHostException("INVALID_ARGS", "path is required.")
        }

        return when (command) {
            "fs.mkdir" -> makeDirectory(path, args, context)
            "fs.readDir" -> readDirectory(path, context)
            "fs.stat" -> stat(path, context)
            "fs.readText" -> readText(path, context)
            "fs.writeText" -> writeText(path, args, context)
            "fs.appendText" -> appendText(path, args, context)
            "fs.copy" -> copy(args, context)
            "fs.move" -> move(args, context)
            "fs.readBytes" -> readBytes(path, context)
            "fs.writeBytes" -> writeBytes(path, args, context)
            "fs.exists" -> exists(path, context)
            else -> remove(path, context)
        }
    }

    private fun readText(path: String, context: KSMPluginContext): Any {
        requirePermission(context, "fs.read")
        requireSafe(path)
        val isReadOnly = KSMVirtualPath.isReadOnly(path)
        val isPicked = KSMVirtualPath.isPicked(path)
        requireServices(isReadOnly || isPicked, context)
        val fs = context.fileSystem
        if (!isReadOnly && !isPicked && fs == null) {
            throw KSMHostException(
                "NOT_SUPPORTED",
                "The filesystem is not configured."
            )
        }
        val value = when {
            isReadOnly -> context.hostServices?.readBundledText(path)
            isPicked -> context.hostServices?.readPickedText(path)
            else -> fs?.readText(path)
        }
        if (value == null) {
            val exists = when {
                isReadOnly -> context.hostServices?.bundledExists(path) == true
                isPicked -> context.hostServices?.pickedExists(path) == true
                else -> fs?.exists(path) == true
            }
            throw KSMHostException(
                if (exists) "IO_ERROR" else "NOT_FOUND",
                if (exists) "The file could not be read." else "The file was not found."
            )
        }
        return textResponse(value, path, fs)
    }

    private fun writeText(
        path: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any {
        requirePermission(context, "fs.write")
        requireSafe(path)
        if (KSMVirtualPath.isReadOnly(path)) {
            throw KSMHostException("PERMISSION_DENIED", "The path is read-only.")
        }
        val content = args.opt("content")
        if (content !is String) {
            throw KSMHostException("INVALID_ARGS", "content is required.")
        }
        val isPicked = KSMVirtualPath.isPicked(path)
        requireServices(isPicked, context)
        val written = if (isPicked) {
            context.hostServices?.writePickedText(path, content) == true
        } else {
            context.fileSystem?.writeText(path, content) == true
        }
        if (!written) {
            throw KSMHostException("IO_ERROR", "The file could not be written.")
        }
        return JSONObject().put("written", true)
    }

    private fun exists(path: String, context: KSMPluginContext): Any {
        requirePermission(context, "fs.read")
        requireSafe(path)
        val isReadOnly = KSMVirtualPath.isReadOnly(path)
        val isPicked = KSMVirtualPath.isPicked(path)
        requireServices(isReadOnly || isPicked, context)
        val exists = when {
            isReadOnly -> context.hostServices?.bundledExists(path) == true
            isPicked -> context.hostServices?.pickedExists(path) == true
            else -> context.fileSystem?.exists(path) == true
        }
        return JSONObject().put("exists", exists)
    }

    private fun remove(path: String, context: KSMPluginContext): Any {
        requirePermission(context, "fs.remove")
        requireSafe(path)
        if (KSMVirtualPath.isReadOnly(path)) {
            throw KSMHostException("PERMISSION_DENIED", "The path is read-only.")
        }
        val isPicked = KSMVirtualPath.isPicked(path)
        requireServices(isPicked, context)
        val removed = if (isPicked) {
            context.hostServices?.removePicked(path) == true
        } else {
            context.fileSystem?.remove(path) == true
        }
        return JSONObject().put("removed", removed)
    }

    private fun appendText(
        path: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any {
        requirePermission(context, "fs.write")
        requireSafe(path)
        if (
            KSMVirtualPath.isReadOnly(path) ||
            KSMVirtualPath.isPicked(path)
        ) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The path is outside the writable sandbox."
            )
        }
        val content = args.opt("content")
        if (content !is String) {
            throw KSMHostException("INVALID_ARGS", "content is required.")
        }
        if (context.fileSystem?.appendText(path, content) != true) {
            throw KSMHostException("IO_ERROR", "The file could not be appended.")
        }
        return JSONObject().put("written", true)
    }

    private fun copy(args: JSONObject, context: KSMPluginContext): Any {
        requirePermission(context, "fs.read")
        requirePermission(context, "fs.write")
        val from = args.opt("from")
        val to = args.opt("to")
        if (from !is String || to !is String) {
            throw KSMHostException("INVALID_ARGS", "from and to are required.")
        }
        requireSafe(from)
        requireSafe(to)
        if (KSMVirtualPath.isReadOnly(to) || KSMVirtualPath.isPicked(to)) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The target path is outside the writable sandbox."
            )
        }
        if (context.fileSystem?.copy(from, to) != true) {
            throw KSMHostException("IO_ERROR", "The file could not be copied.")
        }
        return JSONObject().put("copied", true)
    }

    private fun move(args: JSONObject, context: KSMPluginContext): Any {
        requirePermission(context, "fs.write")
        val from = args.opt("from")
        val to = args.opt("to")
        if (from !is String || to !is String) {
            throw KSMHostException("INVALID_ARGS", "from and to are required.")
        }
        requireSafe(from)
        requireSafe(to)
        if (
            KSMVirtualPath.isReadOnly(from) ||
            KSMVirtualPath.isReadOnly(to) ||
            KSMVirtualPath.isPicked(from) ||
            KSMVirtualPath.isPicked(to)
        ) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The path is outside the writable sandbox."
            )
        }
        if (context.fileSystem?.move(from, to) != true) {
            throw KSMHostException("IO_ERROR", "The file could not be moved.")
        }
        return JSONObject().put("moved", true)
    }

    private fun readBytes(path: String, context: KSMPluginContext): Any {
        requirePermission(context, "fs.read")
        requireSafe(path)
        if (KSMVirtualPath.isReadOnly(path) || KSMVirtualPath.isPicked(path)) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The path is outside the sandbox."
            )
        }
        val bytes = context.fileSystem?.readBytes(path)
            ?: throw KSMHostException("NOT_FOUND", "The file was not found.")
        if (bytes.size > MAX_BINARY_BYTES) {
            throw KSMHostException(
                "RESOURCE_LIMIT",
                "The binary result exceeds the configured limit."
            )
        }
        return JSONObject().put(
            "base64",
            java.util.Base64.getEncoder().encodeToString(bytes)
        )
    }

    private fun writeBytes(
        path: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any {
        requirePermission(context, "fs.write")
        requireSafe(path)
        if (
            KSMVirtualPath.isReadOnly(path) ||
            KSMVirtualPath.isPicked(path)
        ) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The path is outside the writable sandbox."
            )
        }
        val base64 = args.opt("base64")
        if (base64 !is String) {
            throw KSMHostException("INVALID_ARGS", "base64 is required.")
        }
        val bytes = try {
            java.util.Base64.getDecoder().decode(base64)
        } catch (_: IllegalArgumentException) {
            throw KSMHostException("INVALID_ARGS", "base64 is not valid.")
        }
        if (bytes.size > MAX_BINARY_BYTES) {
            throw KSMHostException(
                "RESOURCE_LIMIT",
                "The binary payload exceeds the configured limit."
            )
        }
        if (context.fileSystem?.writeBytes(path, bytes) != true) {
            throw KSMHostException("IO_ERROR", "The file could not be written.")
        }
        return JSONObject().put("written", true)
    }

    private fun readDirectory(
        path: String,
        context: KSMPluginContext
    ): Any {
        requirePermission(context, "fs.read")
        if (!KSMVirtualPath.isSafeDirectory(path) || KSMVirtualPath.isPicked(path)) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The path is not a readable directory."
            )
        }
        val isReadOnly = KSMVirtualPath.isReadOnly(path)
        requireServices(isReadOnly, context)
        val entries = if (isReadOnly) {
            context.hostServices?.readBundledDirectory(path)
        } else {
            context.fileSystem?.readDir(path)
        }
        if (entries == null) {
            val metadata = if (isReadOnly) {
                context.hostServices?.bundledStat(path)
            } else {
                context.fileSystem?.stat(path)
            }
            if (metadata != null && metadata.kind != KSMFileKind.DIRECTORY) {
                throw KSMHostException(
                    "INVALID_ARGS",
                    "The path is not a directory."
                )
            }
            throw KSMHostException(
                if (metadata != null) "IO_ERROR" else "NOT_FOUND",
                if (metadata != null) {
                    "The directory could not be read."
                } else {
                    "The directory was not found."
                }
            )
        }
        val values = JSONArray()
        for (entry in entries) {
            val value = metadataJson(
                KSMFileMetadata(
                    entry.kind,
                    entry.size,
                    entry.modifiedAt
                )
            )
                .put("name", entry.name)
                .put("path", entry.path)
            values.put(value)
        }
        return JSONObject().put("entries", values)
    }

    private fun makeDirectory(
        path: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any {
        requirePermission(context, "fs.write")
        if (
            !KSMVirtualPath.isSafeDirectory(path) ||
            KSMVirtualPath.isPicked(path) ||
            KSMVirtualPath.isReadOnly(path)
        ) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The path is outside the writable sandbox."
            )
        }
        val recursive = args.opt("recursive")
        if (recursive != null && recursive !is Boolean) {
            throw KSMHostException(
                "INVALID_ARGS",
                "recursive must be a boolean."
            )
        }
        val created = context.fileSystem?.makeDirectory(
            path,
            recursive == true
        ) ?: throw KSMHostException(
            "NOT_SUPPORTED",
            "The filesystem is not configured."
        )
        return JSONObject().put("created", created)
    }

    private fun stat(path: String, context: KSMPluginContext): Any {
        requirePermission(context, "fs.read")
        if (!KSMVirtualPath.isSafeStat(path) || path == "app://picked") {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The path is outside the sandbox."
            )
        }
        val isReadOnly = KSMVirtualPath.isReadOnly(path)
        val isPicked = KSMVirtualPath.isPicked(path)
        requireServices(isReadOnly || isPicked, context)
        val metadata = when {
            isPicked -> context.hostServices?.pickedStat(path)
            isReadOnly -> context.hostServices?.bundledStat(path)
            else -> context.fileSystem?.stat(path)
        } ?: throw KSMHostException("NOT_FOUND", "The path was not found.")
        return metadataJson(metadata)
    }

    private fun textResponse(
        value: String,
        path: String,
        fileSystem: KSMFileSystem?
    ): Any {
        if (value.toByteArray(Charsets.UTF_8).size <= MAX_INLINE_TEXT_BYTES) {
            return value
        }
        if (path.startsWith("app://tmp/read-")) {
            return JSONObject().put("file", path)
        }
        val file = fileSystem?.createTemporaryText(value)
            ?: throw KSMHostException(
                "IO_ERROR",
                "The large text result could not be staged."
            )
        return JSONObject().put("file", file)
    }

    private fun metadataJson(metadata: KSMFileMetadata): JSONObject {
        return JSONObject()
            .put(
                "kind",
                if (metadata.kind == KSMFileKind.DIRECTORY) {
                    "directory"
                } else {
                    "file"
                }
            )
            .put("size", metadata.size)
            .put("modifiedAt", metadata.modifiedAt ?: JSONObject.NULL)
    }

    private fun requireSafe(path: String) {
        if (!KSMVirtualPath.isSafe(path)) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The path is outside the sandbox."
            )
        }
    }

    private fun requireServices(
        needsHost: Boolean,
        context: KSMPluginContext
    ) {
        if (needsHost && context.hostServices == null) {
            throw KSMHostException(
                "NOT_SUPPORTED",
                "The requested file service is not configured."
            )
        }
    }

    private fun requirePermission(
        context: KSMPluginContext,
        permission: String
    ) {
        if (!context.hasPermission(permission)) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The requested permission was denied."
            )
        }
    }

    private companion object {
        const val MAX_INLINE_TEXT_BYTES = 64 * 1024
        const val MAX_BINARY_BYTES = 32 * 1024 * 1024
    }
}
