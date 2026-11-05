package dev.kasumi.app.bridge

import dev.kasumi.app.generated.KSMGeneratedContract
import org.json.JSONArray
import org.json.JSONObject
import java.util.Locale

internal object KSMCapabilityNames {
    val commands = listOf(
        "http.request",
        "http.transfer.start",
        "http.transfer.cancel",
        "http.cookies.get",
        "http.cookies.set",
        "http.cookies.clear",
        "dialog.openFolder",
        "dialog.openFiles",
        "fs.walk",
        "fs.scope.release",
        "media.probe",
        "media.ffmpeg.detect",
        "media.ffmpeg.validate",
        "media.ffmpeg.setPath",
        "media.ffmpeg.pickExecutable",
        "audio.devices.list",
        "audio.devices.set",
        "audio.output.getMode",
        "audio.output.setExclusive",
        "audio.eq.set",
        "audio.speed.set",
        "audio.probe",
        "audio.load",
        "audio.play",
        "audio.pause",
        "audio.stop",
        "audio.seek",
        "audio.volume.set",
        "audio.state.get",
        "mediaSession.update",
        "mediaSession.clear",
        "window.create",
        "window.show",
        "window.hide",
        "window.setPosition",
        "window.setAlwaysOnTop",
        "window.setTransparent",
        "window.getState",
        "event.emit",
        "tray.configure",
        "updater.install",
        "app.openDevtools",
        "app.deepLink.getPending"
    )

    val events = listOf(
        "http.transfer.progress",
        "http.transfer.completed",
        "http.transfer.failed",
        "audio.tick",
        "audio.ended",
        "audio.stateChanged",
        "audio.error",
        "mediaSession.command",
        "window.visibilityChanged",
        "window.stateChanged",
        "tray.action",
        "tray.click",
        "globalShortcut.triggered",
        "updater.progress",
        "updater.status",
        "app.deepLink.received",
        "app.secondInstance.received"
    )

    val permissions = setOf(
        "http.request",
        "http.transfer",
        "http.cookies",
        "dialog",
        "fs.read",
        "media.read",
        "media.control",
        "mediaSession",
        "window.manage",
        "tray.configure",
        "updater.install",
        "app.devtools",
        "app.deepLink"
    )

    fun commands(): Set<String> {
        return commands.toSet()
    }

    fun events(): Set<String> {
        return events.toSet()
    }

    fun isCapabilityCommand(command: String): Boolean = command in commands

    fun permissionForCommand(command: String): String = when {
        command.startsWith("http.transfer.") -> "http.transfer"
        command.startsWith("http.cookies.") -> "http.cookies"
        command.startsWith("http.") -> "http.request"
        command.startsWith("dialog.") -> "dialog"
        command.startsWith("fs.") -> "fs.read"
        command == "media.probe" ||
            command.startsWith("media.ffmpeg.") ||
            command == "audio.probe" ||
            command == "audio.state.get" ||
            command == "audio.devices.list" ||
            command == "audio.output.getMode" -> "media.read"
        command.startsWith("mediaSession.") -> "mediaSession"
        command.startsWith("audio.") -> "media.control"
        command.startsWith("window.") || command == "event.emit" ->
            "window.manage"
        command.startsWith("tray.") -> "tray.configure"
        command.startsWith("updater.") -> "updater.install"
        command == "app.openDevtools" -> "app.devtools"
        command == "app.deepLink.getPending" -> "app.deepLink"
        else -> "window.control"
    }
}

internal class KSMCapabilityPlugin : KSMPlugin {
    override val id: String = "capabilities"
    override val commands: Set<String> = KSMCapabilityNames.commands()
    override val permissions: Set<String> = KSMCapabilityNames.permissions

    override fun validateArgs(command: String, args: JSONObject): Boolean {
        return when {
            command == "http.transfer.cancel" ->
                args.opt("transferId") is String
            command == "fs.scope.release" -> args.has("scopeId")
            command.startsWith("http.cookies.") ->
                args.opt("jarId") is String &&
                    args.opt("url") is String
            command == "http.request" ||
                command == "http.transfer.start" ->
                args.opt("url") is String
            command == "media.probe" ||
                command == "audio.probe" ||
                command == "audio.load" ||
                command == "fs.walk" ->
                args.opt("path") is String
            command.startsWith("window.") ->
                command == "window.create" || args.has("windowId")
            command == "event.emit" -> args.has("event")
            command == "audio.eq.set" -> args.has("gains")
            command == "audio.speed.set" -> args.has("speed")
            command == "audio.volume.set" -> args.has("volume")
            command == "audio.seek" -> args.has("positionMs")
            command == "audio.devices.set" -> args.has("deviceId")
            command == "audio.output.setExclusive" -> args.has("exclusive")
            command == "window.setAlwaysOnTop" ||
                command == "window.setTransparent" -> args.has("enabled")
            else -> true
        }
    }

    override fun handle(
        command: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any {
        requirePermission(command, context)
        val host = context.hostServices ?: failClosed(command)
        if (context.cancelToken?.invoke() == true) {
            throw KSMHostException("CANCELLED", "The command was cancelled.")
        }
        return when {
            command.startsWith("http.") -> httpCommand(command, args, context, host)
            command.startsWith("dialog.") ||
                command.startsWith("fs.") ||
                command.startsWith("media.") ->
                fileCommand(command, args, context, host)
            command.startsWith("audio.") -> audioCommand(command, args, host)
            command.startsWith("window.") -> windowCommand(command, args, host)
            else -> systemCommand(command, args, context, host)
        }
    }

    private fun requirePermission(
        command: String,
        context: KSMPluginContext
    ) {
        val permission = KSMCapabilityNames.permissionForCommand(command)
        if (!context.hasPermission(permission)) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The requested permission was denied."
            )
        }
    }

    private fun failClosed(command: String): Nothing {
        throw KSMHostException(
            "NOT_SUPPORTED",
            "$command is not implemented on this host."
        )
    }

    private fun requireResult(command: String, result: Boolean) {
        if (!result) {
            failClosed(command)
        }
    }

    private fun requiredString(args: JSONObject, field: String): String {
        val value = args.opt(field)
        if (value !is String || value.isEmpty()) {
            throw KSMHostException(
                "INVALID_ARGS",
                "$field is required."
            )
        }
        return value
    }

    private fun optionalString(args: JSONObject, field: String): String {
        val value = args.opt(field)
        if (value == null || value === JSONObject.NULL) {
            return ""
        }
        if (value !is String) {
            throw KSMHostException(
                "INVALID_ARGS",
                "$field must be a string."
            )
        }
        return value
    }

    private fun stringArray(args: JSONObject, field: String): List<String> {
        val raw = args.optJSONArray(field) ?: return emptyList()
        return (0 until raw.length()).map { index ->
            raw.opt(index) as? String
                ?: throw KSMHostException(
                    "INVALID_ARGS",
                    "$field must contain strings."
                )
        }
    }

    private fun headers(args: JSONObject): List<KSMHttpHeader> {
        val raw = args.optJSONObject("headers") ?: return emptyList()
        val blocked = setOf(
            "cookie",
            "host",
            "proxy-authorization",
            "set-cookie",
            "user-agent",
            "referer"
        )
        return raw.keys().asSequence().map { name ->
            val value = raw.opt(name)
            if (value !is String || name.lowercase(Locale.ROOT) in blocked) {
                throw KSMHostException(
                    "PERMISSION_DENIED",
                    "The header is controlled by the host."
                )
            }
            KSMHttpHeader(name, value)
        }.toList()
    }

    private fun httpUrl(args: JSONObject, host: KSMHostServices): String {
        val url = requiredString(args, "url")
        if (
            KSMHttpPolicy.parseHttpUrl(url, httpsOnly = true) == null ||
            !host.isHttpRequestUrlAllowed(url)
        ) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The URL origin is not allowlisted."
            )
        }
        return url
    }

    private fun httpMethod(args: JSONObject): String {
        val method = optionalString(args, "method").ifEmpty { "GET" }
        if (method != "GET" && method != "POST") {
            throw KSMHostException(
                "INVALID_ARGS",
                "method must be GET or POST."
            )
        }
        return method
    }

    private fun httpBody(args: JSONObject): String {
        val body = optionalString(args, "body")
        if (body.toByteArray(Charsets.UTF_8).size > 60 * 1024) {
            throw KSMHostException(
                "RESOURCE_LIMIT",
                "The request body exceeds the inline message limit."
            )
        }
        return body
    }

    private fun safePath(args: JSONObject, writable: Boolean): String {
        val path = requiredString(args, "path")
        if (!KSMVirtualPath.isSafe(path)) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The path is outside the sandbox."
            )
        }
        if (
            writable &&
            (KSMVirtualPath.isReadOnly(path) || KSMVirtualPath.isPicked(path))
        ) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The path is outside the writable sandbox."
            )
        }
        return path
    }

    private fun scopePath(args: JSONObject): String {
        val path = requiredString(args, "path")
        val prefix = "app://scope/"
        if (!path.startsWith(prefix)) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The path is outside an active file scope."
            )
        }
        val relative = path.removePrefix(prefix)
        if (
            relative.isEmpty() ||
            relative.startsWith("/") ||
            relative.contains("..") ||
            relative.contains('\u0000') ||
            relative.contains('\\') ||
            relative.contains(':') ||
            relative.contains("//")
        ) {
            throw KSMHostException(
                "PERMISSION_DENIED",
                "The path is outside an active file scope."
            )
        }
        return path
    }

    private fun cookies(args: JSONObject): List<KSMCookie> {
        val raw = args.optJSONArray("cookies")
            ?: throw KSMHostException(
                "INVALID_ARGS",
                "cookies must be an array."
            )
        return (0 until raw.length()).map { index ->
            val value = raw.optJSONObject(index)
                ?: throw KSMHostException(
                    "INVALID_ARGS",
                    "cookies must contain objects."
                )
            KSMCookie(
                name = requiredString(value, "name"),
                value = optionalString(value, "value"),
                domain = optionalString(value, "domain"),
                path = optionalString(value, "path").ifEmpty { "/" },
                secure = value.optBoolean("secure", true),
                httpOnly = value.optBoolean("httpOnly", false)
            )
        }
    }

    private fun httpCommand(
        command: String,
        args: JSONObject,
        context: KSMPluginContext,
        host: KSMHostServices
    ): Any {
        val optional = fun(field: String): String? {
            return optionalString(args, field).takeIf { it.isNotEmpty() }
        }
        if (command == "http.transfer.cancel") {
            requireResult(
                command,
                host.cancelHttpTransfer(requiredString(args, "transferId"))
            )
            return JSONObject().put("cancelled", true)
        }
        val url = httpUrl(args, host)
        val method = httpMethod(args)
        val headers = headers(args)
        val body = httpBody(args)
        val jarId = optional("jarId")
        val userAgent = optional("userAgent")
        val referer = optional("referer")
        if (command == "http.request") {
            val response = host.httpRequest(
                KSMHttpRequestSpec(
                    url,
                    method,
                    headers,
                    body,
                    jarId,
                    userAgent,
                    referer
                ),
                context.requestId
            ) ?: failClosed(command)
            val headersJson = JSONObject()
            response.headers.forEach { headersJson.put(it.name, it.value) }
            return JSONObject()
                .put("status", response.status)
                .put("headers", headersJson)
                .put("body", response.body)
                .put("file", response.file ?: JSONObject.NULL)
                .put("bytes", response.bytes)
        }
        if (command == "http.transfer.start") {
            val path = safePath(args, true)
            val transferId = host.startHttpTransfer(
                KSMHttpTransferSpec(
                    url,
                    method,
                    path,
                    headers,
                    body,
                    jarId,
                    userAgent,
                    referer,
                    args.optBoolean("overwrite", false)
                ),
                context.requestId
            ) ?: failClosed(command)
            return JSONObject().put("transferId", transferId)
        }
        val cookieJarId = requiredString(args, "jarId")
        if (command == "http.cookies.get") {
            val values = host.getCookies(cookieJarId, url)
                ?: failClosed(command)
            val result = JSONArray()
            values.forEach {
                result.put(
                    JSONObject()
                        .put("name", it.name)
                        .put("value", it.value)
                        .put("domain", it.domain)
                        .put("path", it.path)
                        .put("secure", it.secure)
                        .put("httpOnly", it.httpOnly)
                )
            }
            return JSONObject().put("cookies", result)
        }
        if (command == "http.cookies.set") {
            requireResult(
                command,
                host.setCookies(cookieJarId, url, cookies(args))
            )
            return JSONObject().put("updated", true)
        }
        requireResult(command, host.clearCookies(cookieJarId, url))
        return JSONObject().put("cleared", true)
    }

    private fun fileCommand(
        command: String,
        args: JSONObject,
        context: KSMPluginContext,
        host: KSMHostServices
    ): Any {
        if (command == "dialog.openFolder" || command == "dialog.openFiles") {
            val filters = if (command == "dialog.openFolder") {
                folderFilters(args)
            } else {
                stringArray(args, "filters")
            }
            val scope = if (command == "dialog.openFolder") {
                host.openFolder(filters)
            } else {
                host.openFiles(filters)
            } ?: failClosed(command)
            return JSONObject()
                .put("id", scope.id)
                .put("path", scope.path)
                .put("files", JSONArray(scope.files))
        }
        if (command == "fs.walk") {
            val entries = host.walk(scopePath(args), context.requestId)
                ?: failClosed(command)
            val result = JSONArray()
            entries.forEach {
                result.put(
                    JSONObject()
                        .put("path", it.path)
                        .put("name", it.name)
                        .put("directory", it.directory)
                        .put("size", it.size)
                        .put("modifiedAt", it.modifiedAt ?: JSONObject.NULL)
                )
            }
            return JSONObject().put("entries", result)
        }
        if (command == "fs.scope.release") {
            requireResult(
                command,
                host.releaseScope(requiredString(args, "scopeId"))
            )
            return JSONObject().put("released", true)
        }
        if (command == "media.probe") {
            val probe = host.probeMedia(scopePath(args))
                ?: failClosed(command)
            return JSONObject()
                .put("title", probe.title ?: JSONObject.NULL)
                .put("artist", probe.artist ?: JSONObject.NULL)
                .put("album", probe.album ?: JSONObject.NULL)
                .put("durationMs", probe.durationMs ?: JSONObject.NULL)
                .put("cover", probe.cover ?: JSONObject.NULL)
                .put("lyric", probe.lyrics ?: JSONObject.NULL)
                .put("format", probe.format ?: JSONObject.NULL)
        }
        val name = optionalString(args, "name").ifEmpty { "ffmpeg" }
        val path = if (
            command == "media.ffmpeg.validate" ||
            command == "media.ffmpeg.setPath"
        ) {
            safePath(args, false)
        } else {
            ""
        }
        return when (command) {
            "media.ffmpeg.detect" -> {
                val tool = host.detectExternalTool(name)
                    ?: failClosed(command)
                externalToolJson(tool)
            }
            "media.ffmpeg.validate" -> {
                val tool = host.validateExternalTool(path)
                    ?: failClosed(command)
                externalToolJson(tool)
            }
            "media.ffmpeg.setPath" -> {
                requireResult(
                    command,
                    host.setExternalToolPath(name, path)
                )
                JSONObject().put("updated", true)
            }
            else -> {
                val picked = host.pickExternalTool(name)
                    ?: failClosed(command)
                JSONObject().put("path", picked)
            }
        }
    }

    private fun externalToolJson(tool: KSMExternalTool): JSONObject {
        return JSONObject()
            .put("available", tool.valid)
            .put("path", tool.path.ifEmpty { JSONObject.NULL })
            .put("version", tool.version.ifEmpty { JSONObject.NULL })
            .put("source", tool.source ?: JSONObject.NULL)
    }

    private fun folderFilters(args: JSONObject): List<String> {
        val raw = args.opt("filters") ?: return emptyList()
        if (raw is JSONArray) {
            return (0 until raw.length()).map { index ->
                raw.getString(index)
            }
        }
        if (raw !is JSONObject) {
            throw KSMHostException("INVALID_ARGS", "filters must be an object.")
        }
        val mimeTypes = raw.optJSONArray("mimeTypes")
        if (mimeTypes != null && mimeTypes.length() > 0) {
            return (0 until mimeTypes.length()).map { index ->
                mimeTypes.getString(index)
            }
        }
        val extensions = raw.optJSONArray("extensions")
        if (extensions != null && extensions.length() > 0) {
            return listOf("*/*")
        }
        return emptyList()
    }

    private fun audioCommand(
        command: String,
        args: JSONObject,
        host: KSMHostServices
    ): Any {
        when (command) {
            "audio.devices.list" -> {
                val devices = host.audioDevices() ?: failClosed(command)
                val result = JSONArray()
                devices.forEach {
                    result.put(
                        JSONObject()
                            .put("id", it.id)
                            .put("name", it.name)
                            .put("isDefault", it.defaultDevice)
                    )
                }
                return JSONObject().put("devices", result)
            }
            "audio.output.getMode" -> {
                host.audioState() ?: failClosed(command)
                return JSONObject().put("mode", "shared")
            }
            "audio.state.get" -> {
                val state = host.audioState() ?: failClosed(command)
                return JSONObject()
                    .put(
                        "playback",
                        state.playback
                    )
                    .put("positionMs", state.positionMs)
                    .put("durationMs", state.durationMs ?: JSONObject.NULL)
                    .put("volume", state.volume)
                    .put("muted", state.muted)
                    .put("speed", state.speed)
                    .put("outputMode", state.outputMode)
                    .put("deviceId", state.deviceId ?: JSONObject.NULL)
                    .put("source", state.source ?: JSONObject.NULL)
            }
            "audio.probe" -> {
                val probe = host.probeAudio(scopePath(args))
                    ?: failClosed(command)
                return JSONObject()
                    .put("durationMs", probe.durationMs ?: JSONObject.NULL)
                    .put("sampleRate", probe.sampleRate ?: JSONObject.NULL)
                    .put("channels", probe.channels ?: JSONObject.NULL)
                    .put("codec", probe.codec ?: JSONObject.NULL)
            }
        }
        val result = when (command) {
            "audio.devices.set" ->
                host.setAudioDevice(requiredString(args, "deviceId"))
            "audio.output.setExclusive" ->
                host.setExclusiveAudioOutput(args.getBoolean("exclusive"))
            "audio.eq.set" ->
                host.setAudioEqualizer(doubleArray(args, "gains"))
            "audio.speed.set" -> host.setAudioSpeed(args.getDouble("speed"))
            "audio.volume.set" -> host.setAudioVolume(args.getDouble("volume"))
            "audio.load" -> host.loadAudio(scopePath(args))
            "audio.play", "audio.pause" ->
                host.setAudioPlayback(command == "audio.play")
            "audio.stop" ->
                host.setAudioPlayback(false) && host.seekAudio(0L)
            else -> host.seekAudio(args.getLong("positionMs"))
        }
        requireResult(command, result)
        return JSONObject().put("updated", true)
    }

    private fun doubleArray(args: JSONObject, field: String): List<Double> {
        val raw = args.optJSONArray(field)
            ?: throw KSMHostException(
                "INVALID_ARGS",
                "$field must be an array."
            )
        return (0 until raw.length()).map { raw.getDouble(it) }
    }

    private fun windowCommand(
        command: String,
        args: JSONObject,
        host: KSMHostServices
    ): Any {
        if (command == "window.create") {
            val windowId = host.createWindow(
                optionalString(args, "route"),
                KSMWindowState(
                    args.optBoolean("visible", true),
                    args.optBoolean("alwaysOnTop", false),
                    args.optBoolean("transparent", false),
                    args.optInt("x", 0),
                    args.optInt("y", 0),
                    args.optInt("width", 0),
                    args.optInt("height", 0)
                )
            ) ?: failClosed(command)
            return JSONObject().put("windowId", windowId)
        }
        val windowId = requiredString(args, "windowId")
        if (command == "window.getState") {
            val state = host.windowState(windowId) ?: failClosed(command)
            return JSONObject()
                .put("visible", state.visible)
                .put("alwaysOnTop", state.alwaysOnTop)
                .put("transparent", state.transparent)
                .put("x", state.x)
                .put("y", state.y)
                .put("width", state.width)
                .put("height", state.height)
        }
        val result = when (command) {
            "window.show", "window.hide" ->
                host.showWindowById(windowId, command == "window.show")
            "window.setPosition" ->
                host.setWindowPosition(
                    windowId,
                    args.getInt("x"),
                    args.getInt("y")
                )
            "window.setAlwaysOnTop" ->
                host.setWindowAlwaysOnTop(windowId, args.getBoolean("enabled"))
            else ->
                host.setWindowTransparent(windowId, args.getBoolean("enabled"))
        }
        requireResult(command, result)
        return JSONObject().put("updated", true)
    }

    private fun systemCommand(
        command: String,
        args: JSONObject,
        context: KSMPluginContext,
        host: KSMHostServices
    ): Any {
        if (command == "mediaSession.update") {
            requireResult(
                command,
                host.updateMediaSession(
                    KSMMediaSessionMetadata(
                        optionalString(args, "title"),
                        optionalString(args, "artist"),
                        optionalString(args, "album"),
                        optionalString(args, "artwork").takeIf {
                            it.isNotEmpty()
                        },
                        args.optLong("durationMs", -1L)
                            .takeIf { it >= 0L },
                        args.optLong("positionMs", -1L)
                            .takeIf { it >= 0L },
                        optionalString(args, "state").ifEmpty { "stopped" }
                    )
                )
            )
            return JSONObject().put("updated", true)
        }
        if (command == "mediaSession.clear") {
            requireResult(command, host.clearMediaSession())
            return JSONObject().put("cleared", true)
        }
        if (command == "event.emit") {
            val event = requiredString(args, "event")
            if (
                event !in KSMGeneratedContract.events &&
                event !in KSMCapabilityNames.events
            ) {
                throw KSMHostException(
                    "INVALID_ARGS",
                    "event is not registered."
                )
            }
            val windowId = optionalString(args, "windowId").takeIf {
                it.isNotEmpty()
            }
            val payload = args.optJSONObject("payload") ?: JSONObject()
            if (payload.toString().toByteArray(Charsets.UTF_8).size > 60 * 1024) {
                throw KSMHostException(
                    "RESOURCE_LIMIT",
                    "The event payload exceeds the inline message limit."
                )
            }
            if (windowId == null || windowId == context.windowId) {
                requireResult(command, context.emitEvent(event, payload))
                return JSONObject().put("emitted", true)
            }
            requireResult(
                command,
                host.emitWindowEvent(
                    windowId,
                    event,
                    payload.toString()
                )
            )
            return JSONObject().put("emitted", true)
        }
        if (command == "tray.configure") {
            val itemsRaw = args.optJSONArray("items")
            val items = (0 until (itemsRaw?.length() ?: 0)).map { index ->
                val item = itemsRaw?.optJSONObject(index)
                    ?: throw KSMHostException(
                        "INVALID_ARGS",
                        "items must contain objects."
                    )
                KSMTrayMenuItem(
                    requiredString(item, "id"),
                    optionalString(item, "title"),
                    item.optBoolean("enabled", true)
                )
            }
            requireResult(
                command,
                host.configureTray(
                    KSMTrayConfig(
                        optionalString(args, "icon").takeIf {
                            it.isNotEmpty()
                        },
                        optionalString(args, "tooltip").takeIf {
                            it.isNotEmpty()
                        },
                        items
                    )
                )
            )
            return JSONObject().put("configured", true)
        }
        if (command == "updater.install") {
            requireResult(
                command,
                host.installUpdate(
                    safePath(args, false),
                    optionalString(args, "hash"),
                    optionalString(args, "signature"),
                    context.requestId
                )
            )
            return JSONObject().put("installed", true)
        }
        if (command == "app.openDevtools") {
            requireResult(
                command,
                host.openDevtools(args.optBoolean("enabled", true))
            )
            return JSONObject().put("opened", true)
        }
        return JSONObject().put(
            "url",
            host.takePendingDeepLink() ?: JSONObject.NULL
        )
    }
}
