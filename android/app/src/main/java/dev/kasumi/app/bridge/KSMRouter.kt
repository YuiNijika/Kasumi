package dev.kasumi.app.bridge

import dev.kasumi.app.generated.KSMGeneratedContract
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.util.Locale
import java.util.concurrent.ConcurrentHashMap

class KSMRouter(
    private val fileSystem: KSMFileSystem? = null,
    private val hostServices: KSMHostServices? = null,
    private val eventSender: (String) -> Unit = {}
) {
    companion object {
        private const val MAX_INLINE_TEXT_BYTES = 64 * 1024
    }

    private val subscribedEvents = ConcurrentHashMap.newKeySet<String>()
    private val grantedPermissions = ConcurrentHashMap.newKeySet<String>().apply {
        addAll(KSMCommandRegistry.permissions)
    }

    fun emitEvent(event: String, payload: JSONObject) {
        if (event !in subscribedEvents) {
            return
        }
        eventSender(
            JSONObject()
                .put("type", "event")
                .put("event", event)
                .put("payload", payload)
                .toString()
        )
    }

    fun emitSystemEvent(event: String, payload: JSONObject) {
        eventSender(
            JSONObject()
                .put("type", "event")
                .put("event", event)
                .put("payload", payload)
                .toString()
        )
    }

    fun handleMessage(message: String): String {
        if (message.toByteArray(Charsets.UTF_8).size > 64 * 1024) {
            return errorResponse(
                JSONObject.NULL,
                "INVALID_ARGS",
                "Request exceeds the 64KB message limit."
            )
        }
        return try {
            val request = JSONObject(message)
            if (request.optString("type") == "cancel") {
                return ""
            }
            val id = request.opt("id")
            if (
                request.optString("type") != "request" ||
                !request.has("id") ||
                request.opt("id") !is String ||
                !request.has("cmd") ||
                request.opt("cmd") !is String ||
                !request.opt("args").isJsonObject()
            ) {
                errorResponse(id, "INVALID_ARGS", "Request fields are invalid.")
            } else {
                val args = request.getJSONObject("args")
                val command = request.getString("cmd")
                if (!KSMGeneratedContract.isKnownCommand(command)) {
                    return errorResponse(
                        id,
                        "UNKNOWN_COMMAND",
                        "The command is not registered."
                    )
                }
                if (!KSMGeneratedContract.validateArgs(command, args)) {
                    return errorResponse(
                        id,
                        "INVALID_ARGS",
                        "Arguments do not match the command schema."
                    )
                }
                when (command) {
                    "ping" -> successResponse(id, JSONObject().put("pong", true))
                    "capabilities.query" -> successResponse(
                        id,
                        JSONObject()
                            .put("platform", "android")
                            .put("commands", KSMCommandRegistry.supportedCommands)
                            .put("permissions", JSONArray(grantedPermissions.toList()))
                    )
                    "app.getInfo" -> successResponse(
                        id,
                        JSONObject()
                            .put("name", "Kasumi")
                            .put("version", hostServices?.appVersion() ?: "0.1.0")
                            .put("platform", "android")
                            .put(
                                "language",
                                hostServices?.language()
                                    ?: Locale.getDefault().toLanguageTag()
                            )
                    )
                    "permission.request" -> {
                        if (
                            args.opt("permission") !is String ||
                            args.getString("permission").isEmpty()
                        ) {
                            errorResponse(id, "INVALID_ARGS", "permission is required.")
                        } else {
                            val permission = args.getString("permission")
                            val known = permission in KSMGeneratedContract.permissions
                            if (!known) {
                                return errorResponse(
                                    id,
                                    "INVALID_ARGS",
                                    "permission is not registered."
                                )
                            }
                            val granted = hostServices?.requestPermission(permission) ?: true
                            if (granted) {
                                grantedPermissions.add(permission)
                            } else {
                                grantedPermissions.remove(permission)
                            }
                            successResponse(id, JSONObject().put("granted", granted))
                        }
                    }
                    "event.subscribe" -> {
                        val event = args.opt("event")
                        if (event !is String || event.isEmpty()) {
                            errorResponse(id, "INVALID_ARGS", "event is required.")
                        } else if (event !in KSMGeneratedContract.events) {
                            errorResponse(id, "INVALID_ARGS", "event is not registered.")
                        } else {
                            subscribedEvents.add(event)
                            successResponse(id, JSONObject().put("subscribed", true))
                        }
                    }
                    "event.unsubscribe" -> {
                        val event = args.opt("event")
                        if (event !is String || event.isEmpty()) {
                            errorResponse(id, "INVALID_ARGS", "event is required.")
                        } else if (event !in KSMGeneratedContract.events) {
                            errorResponse(id, "INVALID_ARGS", "event is not registered.")
                        } else {
                            subscribedEvents.remove(event)
                            successResponse(id, JSONObject().put("unsubscribed", true))
                        }
                    }
                    "fs.readText" -> readText(id, args)
                    "fs.writeText" -> writeText(id, args)
                    "fs.exists" -> exists(id, args)
                    "fs.remove" -> remove(id, args)
                    "fs.readDir" -> readDir(id, args)
                    "fs.mkdir" -> makeDirectory(id, args)
                    "fs.stat" -> stat(id, args)
                    "dialog.message", "dialog.confirm" -> showDialog(
                        id,
                        command,
                        args
                    )
                    "dialog.openFile" -> openFile(id, args)
                    "dialog.saveFile" -> saveFile(id, args)
                    "clipboard.readText" -> readClipboard(id)
                    "clipboard.writeText" -> writeClipboard(id, args)
                    "notification.show" -> showNotification(id, args)
                    "window.setTitle",
                    "window.minimize",
                    "window.maximize" -> errorResponse(
                        id,
                        "NOT_SUPPORTED",
                        "The command is not supported on this platform."
                    )
                    "window.close" -> {
                        if (!requirePermission(id, "window.control")) {
                            return errorResponse(
                                id,
                                "PERMISSION_DENIED",
                                "The requested permission was denied."
                            )
                        }
                        val host = hostServices ?: return errorResponse(
                            id,
                            "NOT_SUPPORTED",
                            "The window service is not configured."
                        )
                        successResponse(
                            id,
                            JSONObject().put("closed", host.closeWindow())
                        )
                    }
                    "app.exit" -> {
                        if (!requirePermission(id, "window.control")) {
                            return errorResponse(
                                id,
                                "PERMISSION_DENIED",
                                "The requested permission was denied."
                            )
                        }
                        val host = hostServices ?: return errorResponse(
                            id,
                            "NOT_SUPPORTED",
                            "The application service is not configured."
                        )
                        successResponse(
                            id,
                            JSONObject().put("exited", host.exitApp())
                        )
                    }
                    else -> errorResponse(
                        id,
                        "NOT_SUPPORTED",
                        "The command is registered but not implemented on this host."
                    )
                }
            }
        } catch (_: JSONException) {
            errorResponse(JSONObject.NULL, "INVALID_ARGS", "Request is not valid JSON.")
        } catch (_: Exception) {
            errorResponse(JSONObject.NULL, "INTERNAL_ERROR", "The command failed unexpectedly.")
        }
    }

    private fun readText(id: Any?, args: JSONObject): String {
        if (!requirePermission(id, "fs.read")) {
            return errorResponse(id, "PERMISSION_DENIED", "The requested permission was denied.")
        }
        val path = args.opt("path")
        if (path !is String || path.isEmpty()) {
            return errorResponse(id, "INVALID_ARGS", "path is required.")
        }
        if (!KSMVirtualPath.isSafe(path)) {
            return errorResponse(id, "PERMISSION_DENIED", "The path is outside the sandbox.")
        }
        val isReadOnly = KSMVirtualPath.isReadOnly(path)
        val isPicked = KSMVirtualPath.isPicked(path)
        if ((isReadOnly || isPicked) && hostServices == null) {
            return errorResponse(
                id,
                "NOT_SUPPORTED",
                if (isReadOnly) {
                    "The bundled file service is not configured."
                } else {
                    "The picked file service is not configured."
                }
            )
        }
        val fs = fileSystem
        if (!isReadOnly && fs == null) {
            return errorResponse(
                id,
                "NOT_SUPPORTED",
                "The filesystem is not configured."
            )
        }
        val value = when {
            isReadOnly -> hostServices?.readBundledText(path)
            isPicked -> hostServices?.readPickedText(path)
            else -> fs?.readText(path)
        }
        if (value == null) {
            val exists = when {
                isReadOnly -> hostServices?.bundledExists(path) == true
                isPicked -> hostServices?.pickedExists(path) == true
                else -> fs?.exists(path) == true
            }
            return if (exists) {
                errorResponse(id, "IO_ERROR", "The file could not be read.")
            } else {
                errorResponse(id, "NOT_FOUND", "The file was not found.")
            }
        }
        return textResponse(id, value, path)
    }

    private fun writeText(id: Any?, args: JSONObject): String {
        if (!requirePermission(id, "fs.write")) {
            return errorResponse(id, "PERMISSION_DENIED", "The requested permission was denied.")
        }
        val path = args.opt("path")
        val content = args.opt("content")
        if (path !is String || path.isEmpty() || content !is String) {
            return errorResponse(id, "INVALID_ARGS", "path and content are required.")
        }
        if (!KSMVirtualPath.isSafe(path)) {
            return errorResponse(id, "PERMISSION_DENIED", "The path is outside the sandbox.")
        }
        if (KSMVirtualPath.isReadOnly(path)) {
            return errorResponse(id, "PERMISSION_DENIED", "The path is read-only.")
        }
        val fs = fileSystem ?: return errorResponse(
            id,
            "NOT_SUPPORTED",
            "The filesystem is not configured."
        )
        val isPicked = KSMVirtualPath.isPicked(path)
        if (isPicked && hostServices == null) {
            return errorResponse(
                id,
                "NOT_SUPPORTED",
                "The picked file service is not configured."
            )
        }
        val written = if (isPicked) {
            hostServices?.writePickedText(path, content) == true
        } else {
            fs.writeText(path, content)
        }
        if (!written) {
            return errorResponse(id, "IO_ERROR", "The file could not be written.")
        }
        return successResponse(id, JSONObject().put("written", true))
    }

    private fun exists(id: Any?, args: JSONObject): String {
        if (!requirePermission(id, "fs.read")) {
            return errorResponse(id, "PERMISSION_DENIED", "The requested permission was denied.")
        }
        val path = args.opt("path")
        if (path !is String || path.isEmpty()) {
            return errorResponse(id, "INVALID_ARGS", "path is required.")
        }
        if (!KSMVirtualPath.isSafe(path)) {
            return errorResponse(id, "PERMISSION_DENIED", "The path is outside the sandbox.")
        }
        val isReadOnly = KSMVirtualPath.isReadOnly(path)
        val isPicked = KSMVirtualPath.isPicked(path)
        if ((isReadOnly || isPicked) && hostServices == null) {
            return errorResponse(
                id,
                "NOT_SUPPORTED",
                if (isReadOnly) {
                    "The bundled file service is not configured."
                } else {
                    "The picked file service is not configured."
                }
            )
        }
        val fs = fileSystem
        if (!isReadOnly && fs == null) {
            return errorResponse(
                id,
                "NOT_SUPPORTED",
                "The filesystem is not configured."
            )
        }
        val exists = when {
            isReadOnly -> hostServices?.bundledExists(path) == true
            isPicked -> hostServices?.pickedExists(path) == true
            else -> fs?.exists(path) == true
        }
        return successResponse(id, JSONObject().put("exists", exists))
    }

    private fun remove(id: Any?, args: JSONObject): String {
        if (!requirePermission(id, "fs.remove")) {
            return errorResponse(id, "PERMISSION_DENIED", "The requested permission was denied.")
        }
        val path = args.opt("path")
        if (path !is String || path.isEmpty()) {
            return errorResponse(id, "INVALID_ARGS", "path is required.")
        }
        if (!KSMVirtualPath.isSafe(path)) {
            return errorResponse(id, "PERMISSION_DENIED", "The path is outside the sandbox.")
        }
        if (KSMVirtualPath.isReadOnly(path)) {
            return errorResponse(id, "PERMISSION_DENIED", "The path is read-only.")
        }
        val fs = fileSystem ?: return errorResponse(
            id,
            "NOT_SUPPORTED",
            "The filesystem is not configured."
        )
        val isPicked = KSMVirtualPath.isPicked(path)
        if (isPicked && hostServices == null) {
            return errorResponse(
                id,
                "NOT_SUPPORTED",
                "The picked file service is not configured."
            )
        }
        val removed = if (isPicked) {
            hostServices?.removePicked(path) == true
        } else {
            fs.remove(path)
        }
        return successResponse(id, JSONObject().put("removed", removed))
    }

    private fun readDir(id: Any?, args: JSONObject): String {
        if (!requirePermission(id, "fs.read")) {
            return errorResponse(id, "PERMISSION_DENIED", "The requested permission was denied.")
        }
        val path = args.opt("path")
        if (path !is String || path.isEmpty()) {
            return errorResponse(id, "INVALID_ARGS", "path is required.")
        }
        if (!KSMVirtualPath.isSafeDirectory(path) || KSMVirtualPath.isPicked(path)) {
            return errorResponse(
                id,
                "PERMISSION_DENIED",
                "The path is not a readable directory."
            )
        }
        if (KSMVirtualPath.isReadOnly(path) && hostServices == null) {
            return errorResponse(
                id,
                "NOT_SUPPORTED",
                "The bundled file service is not configured."
            )
        }
        if (!KSMVirtualPath.isReadOnly(path) && fileSystem == null) {
            return errorResponse(
                id,
                "NOT_SUPPORTED",
                "The filesystem is not configured."
            )
        }
        val entries = when {
            KSMVirtualPath.isReadOnly(path) -> hostServices?.readBundledDirectory(path)
            else -> fileSystem?.readDir(path)
        }
        if (entries == null) {
            val metadata = when {
                KSMVirtualPath.isReadOnly(path) -> hostServices?.bundledStat(path)
                else -> fileSystem?.stat(path)
            }
            return when {
                metadata != null && metadata.kind != KSMFileKind.DIRECTORY ->
                    errorResponse(id, "INVALID_ARGS", "The path is not a directory.")
                metadata != null ->
                    errorResponse(id, "IO_ERROR", "The directory could not be read.")
                else -> errorResponse(id, "NOT_FOUND", "The directory was not found.")
            }
        }
        val values = JSONArray()
        for (entry in entries) {
            values.put(
                JSONObject()
                    .put("name", entry.name)
                    .put("path", entry.path)
                    .put("kind", fileKindName(entry.kind))
                    .put("size", entry.size)
                    .put("modifiedAt", entry.modifiedAt ?: JSONObject.NULL)
            )
        }
        return successResponse(id, JSONObject().put("entries", values))
    }

    private fun makeDirectory(id: Any?, args: JSONObject): String {
        if (!requirePermission(id, "fs.write")) {
            return errorResponse(id, "PERMISSION_DENIED", "The requested permission was denied.")
        }
        val path = args.opt("path")
        if (path !is String || path.isEmpty()) {
            return errorResponse(id, "INVALID_ARGS", "path is required.")
        }
        if (
            !KSMVirtualPath.isSafeDirectory(path) ||
            KSMVirtualPath.isReadOnly(path) ||
            KSMVirtualPath.isPicked(path)
        ) {
            return errorResponse(
                id,
                "PERMISSION_DENIED",
                "The path is outside the writable sandbox."
            )
        }
        if (args.has("recursive") && args.opt("recursive") !is Boolean) {
            return errorResponse(id, "INVALID_ARGS", "recursive must be a boolean.")
        }
        val recursive = args.optBoolean("recursive", false)
        val fs = fileSystem ?: return errorResponse(
            id,
            "NOT_SUPPORTED",
            "The filesystem is not configured."
        )
        val created = fs.makeDirectory(path, recursive) ?: return errorResponse(
            id,
            "IO_ERROR",
            "The directory could not be created."
        )
        return successResponse(id, JSONObject().put("created", created))
    }

    private fun stat(id: Any?, args: JSONObject): String {
        if (!requirePermission(id, "fs.read")) {
            return errorResponse(id, "PERMISSION_DENIED", "The requested permission was denied.")
        }
        val path = args.opt("path")
        if (path !is String || path.isEmpty()) {
            return errorResponse(id, "INVALID_ARGS", "path is required.")
        }
        if (!KSMVirtualPath.isSafeStat(path) || path == "app://picked") {
            return errorResponse(id, "PERMISSION_DENIED", "The path is outside the sandbox.")
        }
        if (
            (KSMVirtualPath.isPicked(path) || KSMVirtualPath.isReadOnly(path)) &&
            hostServices == null
        ) {
            return errorResponse(
                id,
                "NOT_SUPPORTED",
                if (KSMVirtualPath.isPicked(path)) {
                    "The picked file service is not configured."
                } else {
                    "The bundled file service is not configured."
                }
            )
        }
        if (
            !KSMVirtualPath.isPicked(path) &&
            !KSMVirtualPath.isReadOnly(path) &&
            fileSystem == null
        ) {
            return errorResponse(
                id,
                "NOT_SUPPORTED",
                "The filesystem is not configured."
            )
        }
        val metadata = when {
            KSMVirtualPath.isPicked(path) -> hostServices?.pickedStat(path)
            KSMVirtualPath.isReadOnly(path) -> hostServices?.bundledStat(path)
            else -> fileSystem?.stat(path)
        } ?: return errorResponse(id, "NOT_FOUND", "The path was not found.")
        return successResponse(id, metadataJson(metadata))
    }

    private fun metadataJson(metadata: KSMFileMetadata): JSONObject {
        return JSONObject()
            .put("kind", fileKindName(metadata.kind))
            .put("size", metadata.size)
            .put("modifiedAt", metadata.modifiedAt ?: JSONObject.NULL)
    }

    private fun fileKindName(kind: KSMFileKind): String {
        return if (kind == KSMFileKind.DIRECTORY) "directory" else "file"
    }

    private fun showDialog(id: Any?, command: String, args: JSONObject): String {
        if (!requirePermission(id, "dialog")) {
            return errorResponse(id, "PERMISSION_DENIED", "The requested permission was denied.")
        }
        val host = hostServices ?: return errorResponse(
            id,
            "NOT_SUPPORTED",
            "The dialog service is not configured."
        )
        val title = args.opt("title")
        val message = args.opt("message")
        if (title !is String || message !is String) {
            return errorResponse(id, "INVALID_ARGS", "title and message are required.")
        }
        val confirmed = if (command == "dialog.message") {
            host.showMessage(title, message)
        } else {
            host.confirm(title, message)
        }
        return successResponse(id, JSONObject().put("confirmed", confirmed))
    }

    private fun openFile(id: Any?, args: JSONObject): String {
        if (!requirePermission(id, "dialog")) {
            return errorResponse(id, "PERMISSION_DENIED", "The requested permission was denied.")
        }
        val host = hostServices ?: return errorResponse(
            id,
            "NOT_SUPPORTED",
            "The dialog service is not configured."
        )
        val filters = mutableListOf<String>()
        if (args.has("filters")) {
            val raw = args.optJSONArray("filters")
                ?: return errorResponse(id, "INVALID_ARGS", "filters must be an array.")
            for (index in 0 until raw.length()) {
                val filter = raw.opt(index)
                if (filter !is String) {
                    return errorResponse(
                        id,
                        "INVALID_ARGS",
                        "filters must contain strings."
                    )
                }
                filters.add(filter)
            }
        }
        val file = host.openFile(filters)
        return successResponse(
            id,
            JSONObject().put("file", file ?: JSONObject.NULL)
        )
    }

    private fun saveFile(id: Any?, args: JSONObject): String {
        if (!requirePermission(id, "dialog")) {
            return errorResponse(id, "PERMISSION_DENIED", "The requested permission was denied.")
        }
        val host = hostServices ?: return errorResponse(
            id,
            "NOT_SUPPORTED",
            "The dialog service is not configured."
        )
        val suggested = if (args.has("suggestedName") && !args.isNull("suggestedName")) {
            args.opt("suggestedName")
        } else {
            null
        }
        if (suggested != null && suggested !is String) {
            return errorResponse(
                id,
                "INVALID_ARGS",
                "suggestedName must be a string."
            )
        }
        val file = host.saveFile(suggested ?: "")
        return successResponse(
            id,
            JSONObject().put("file", file ?: JSONObject.NULL)
        )
    }

    private fun readClipboard(id: Any?): String {
        if (!requirePermission(id, "clipboard.read")) {
            return errorResponse(id, "PERMISSION_DENIED", "The requested permission was denied.")
        }
        val host = hostServices ?: return errorResponse(
            id,
            "NOT_SUPPORTED",
            "The clipboard service is not configured."
        )
        return successResponse(
            id,
            JSONObject().put("text", host.readClipboardText() ?: "")
        )
    }

    private fun writeClipboard(id: Any?, args: JSONObject): String {
        if (!requirePermission(id, "clipboard.write")) {
            return errorResponse(id, "PERMISSION_DENIED", "The requested permission was denied.")
        }
        val host = hostServices ?: return errorResponse(
            id,
            "NOT_SUPPORTED",
            "The clipboard service is not configured."
        )
        val text = args.opt("text")
        if (text !is String) {
            return errorResponse(id, "INVALID_ARGS", "text is required.")
        }
        return successResponse(
            id,
            JSONObject().put("written", host.writeClipboardText(text))
        )
    }

    private fun showNotification(id: Any?, args: JSONObject): String {
        if (!requirePermission(id, "notification.show")) {
            return errorResponse(id, "PERMISSION_DENIED", "The requested permission was denied.")
        }
        val host = hostServices ?: return errorResponse(
            id,
            "NOT_SUPPORTED",
            "The notification service is not configured."
        )
        if (!host.hasRuntimePermission("notification.show")) {
            return errorResponse(
                id,
                "PERMISSION_DENIED",
                "The requested permission was denied."
            )
        }
        val title = args.opt("title")
        val body = args.opt("body")
        if (title !is String || body !is String) {
            return errorResponse(id, "INVALID_ARGS", "title and body are required.")
        }
        return successResponse(
            id,
            JSONObject().put("shown", host.showNotification(title, body))
        )
    }

    private fun requirePermission(id: Any?, permission: String): Boolean {
        return permission in grantedPermissions
    }

    private fun textResponse(id: Any?, value: String, path: String): String {
        if (value.toByteArray(Charsets.UTF_8).size <= MAX_INLINE_TEXT_BYTES) {
            return successResponse(id, value)
        }
        if (path.startsWith("app://tmp/read-")) {
            return successResponse(id, JSONObject().put("file", path))
        }
        val fs = fileSystem ?: return errorResponse(
            id,
            "NOT_SUPPORTED",
            "The filesystem is not configured."
        )
        val file = fs.createTemporaryText(value) ?: return errorResponse(
            id,
            "IO_ERROR",
            "The large text result could not be staged."
        )
        return successResponse(id, JSONObject().put("file", file))
    }

    private fun successResponse(id: Any?, data: Any): String {
        return JSONObject()
            .put("type", "response")
            .put("id", id ?: JSONObject.NULL)
            .put("ok", true)
            .put("data", data)
            .toString()
    }

    private fun errorResponse(id: Any?, code: String, message: String): String {
        return JSONObject()
            .put("type", "response")
            .put("id", id ?: JSONObject.NULL)
            .put("ok", false)
            .put("error", JSONObject().put("code", code).put("message", message))
            .toString()
    }

    private fun Any?.isJsonObject(): Boolean {
        return this is JSONObject
    }
}
