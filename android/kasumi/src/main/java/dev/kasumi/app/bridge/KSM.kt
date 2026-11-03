package dev.kasumi.app.bridge

import dev.kasumi.app.generated.KSMGeneratedContract
import org.json.JSONObject
import java.util.concurrent.atomic.AtomicLong

object KSM {
    const val DEFAULT_WINDOW_ID = "default"

    private data class WindowState(
        val router: KSMRouter,
        val fileSystem: KSMFileSystem?,
        val hostServices: KSMHostServices?,
        var cachedCapabilities: KSMCapabilities
    )

    private data class ResolvedWindow(
        val id: String,
        val state: WindowState
    )

    private val stateLock = Any()
    private val listenerLock = Any()
    private val nextRequestId = AtomicLong(1)
    private val windows = mutableMapOf<String, WindowState>()
    private val eventListeners =
        mutableMapOf<String, MutableMap<String, MutableSet<(JSONObject) -> Unit>>>()

    val fs = KSMFs
    val dialog = KSMDialog
    val clipboard = KSMClipboard
    val notification = KSMNotification
    val permission = KSMPermission
    val http = KSMHttp
    val shell = KSMShell
    val device = KSMDevice
    val path = KSMPath
    val store = KSMStore
    val os = KSMOperatingSystem
    val log = KSMLog
    val window = KSMWindow
    val app = KSMApp

    fun init(
        router: KSMRouter,
        fileSystem: KSMFileSystem? = null,
        hostServices: KSMHostServices? = null
    ) {
        registerWindow(
            DEFAULT_WINDOW_ID,
            router,
            fileSystem,
            hostServices
        )
    }

    fun registerWindow(
        windowId: String,
        router: KSMRouter,
        fileSystem: KSMFileSystem? = null,
        hostServices: KSMHostServices? = null
    ) {
        require(windowId.isNotBlank()) {
            "A Kasumi window ID must not be blank."
        }
        synchronized(stateLock) {
            windows[windowId] = WindowState(
                router = router,
                fileSystem = fileSystem,
                hostServices = hostServices,
                cachedCapabilities = capabilitiesFor(router)
            )
        }
        synchronized(listenerLock) {
            eventListeners.remove(windowId)
        }
    }

    fun unregisterWindow(windowId: String) {
        synchronized(stateLock) {
            windows.remove(windowId)
        }
        synchronized(listenerLock) {
            eventListeners.remove(windowId)
        }
    }

    fun shutdown() {
        unregisterWindow(DEFAULT_WINDOW_ID)
    }

    fun invoke(
        windowId: String?,
        command: String,
        args: JSONObject
    ): JSONObject {
        val resolved = resolveWindow(windowId)
        val request = JSONObject()
            .put("type", "request")
            .put("id", "req-native-${nextRequestId.getAndIncrement()}")
            .put("cmd", command)
            .put("args", args)
        val response = JSONObject(resolved.state.router.handleMessage(request.toString()))
        if (response.optBoolean("ok") && command == "capabilities.query") {
            response.optJSONObject("data")?.let { data ->
                synchronized(stateLock) {
                    windows[resolved.id]?.let { state ->
                        state.cachedCapabilities = capabilitiesFrom(data)
                    }
                }
            }
        }
        return response
    }

    fun invoke(command: String, args: JSONObject): JSONObject {
        return invoke(null, command, args)
    }

    fun on(event: String, handler: (JSONObject) -> Unit): () -> Unit {
        return on(null, event, handler)
    }

    fun on(
        windowId: String?,
        event: String,
        handler: (JSONObject) -> Unit
    ): () -> Unit {
        require(event in KSMGeneratedContract.events) {
            "Unknown Kasumi event: $event"
        }
        val resolved = resolveWindow(windowId)
        if (!isCommandEnabled(resolved.id, "event.subscribe")) {
            return {}
        }
        synchronized(listenerLock) {
            eventListeners
                .getOrPut(resolved.id) { mutableMapOf() }
                .getOrPut(event) { mutableSetOf() }
                .add(handler)
        }
        return removeListener@{
            if (
                isCommandEnabledOrNull(
                    resolved.id,
                    "event.unsubscribe"
                ) != true
            ) {
                return@removeListener
            }
            synchronized(listenerLock) {
                val windowListeners = eventListeners[resolved.id]
                    ?: return@synchronized
                val listeners = windowListeners[event] ?: return@synchronized
                listeners.remove(handler)
                if (listeners.isEmpty()) {
                    windowListeners.remove(event)
                }
                if (windowListeners.isEmpty()) {
                    eventListeners.remove(resolved.id)
                }
            }
        }
    }

    internal fun isCommandEnabled(command: String): Boolean {
        val resolved = resolveWindow(null)
        return isCommandEnabled(resolved.id, command)
    }

    private fun isCommandEnabled(windowId: String, command: String): Boolean {
        return isCommandEnabledOrNull(windowId, command)
            ?: error("Kasumi window is not registered: $windowId")
    }

    private fun isCommandEnabledOrNull(
        windowId: String,
        command: String
    ): Boolean? {
        return synchronized(stateLock) {
            windows[windowId]?.router?.supportedCommands?.contains(command)
        }
    }

    fun emitEvent(event: String, payload: JSONObject) {
        emitEvent(null, event, payload)
    }

    fun emitEvent(
        windowId: String?,
        event: String,
        payload: JSONObject
    ) {
        val resolved = resolveWindow(windowId)
        val listeners = synchronized(listenerLock) {
            eventListeners[resolved.id]
                ?.get(event)
                ?.toList()
                .orEmpty()
        }
        for (listener in listeners) {
            try {
                listener(payload)
            } catch (_: Exception) {
                // One native listener must not prevent delivery to the others.
            }
        }
    }

    fun ping(): KSMPingResult {
        return KSMPingResult(pong = true)
    }

    fun get(): KSMCapabilities {
        val resolved = resolveWindow(null)
        return synchronized(stateLock) {
            requireNotNull(windows[resolved.id]?.cachedCapabilities) {
                "Kasumi window is not registered: ${resolved.id}"
            }
        }
    }

    fun queryCapabilities(): KSMCapabilities {
        return get()
    }

    internal fun requireFileSystem(): KSMFileSystem {
        return requireFileSystem(null)
    }

    internal fun requireFileSystem(windowId: String?): KSMFileSystem {
        val resolved = resolveWindow(windowId)
        return checkNotNull(resolved.state.fileSystem) {
            "KSM must be initialized with a filesystem before use."
        }
    }

    internal fun requireHostServices(): KSMHostServices {
        return requireHostServices(null)
    }

    internal fun requireHostServices(windowId: String?): KSMHostServices {
        val resolved = resolveWindow(windowId)
        return checkNotNull(resolved.state.hostServices) {
            "KSM must be initialized with host services before use."
        }
    }

    private fun resolveWindow(windowId: String?): ResolvedWindow {
        return checkNotNull(resolveWindowOrNull(windowId)) {
            if (windowId.isNullOrBlank()) {
                "KSM must be attached to exactly one WebView window before use."
            } else {
                "Kasumi window is not registered: $windowId"
            }
        }
    }

    private fun resolveWindowOrNull(windowId: String?): ResolvedWindow? {
        return synchronized(stateLock) {
            if (!windowId.isNullOrBlank()) {
                return@synchronized windows[windowId]?.let {
                    ResolvedWindow(windowId, it)
                }
            }
            if (windows.size != 1) {
                return@synchronized null
            }
            val entry = windows.entries.single()
            ResolvedWindow(entry.key, entry.value)
        }
    }

    private fun capabilitiesFor(router: KSMRouter): KSMCapabilities {
        return KSMCapabilities(
            platform = KSMPlatform.ANDROID,
            commands = router.capabilityCommands,
            permissions = router.grantedPermissionNames,
            plugins = router.supportedPlugins
        )
    }

    private fun capabilitiesFrom(data: JSONObject): KSMCapabilities {
        val commands = data.optJSONArray("commands")
            ?.let { values ->
                List(values.length()) { index -> values.getString(index) }
            }
            .orEmpty()
        val permissions = data.optJSONArray("permissions")
            ?.let { values ->
                List(values.length()) { index -> values.getString(index) }
            }
            .orEmpty()
        val plugins = data.optJSONArray("plugins")
            ?.let { values ->
                List(values.length()) { index -> values.getString(index) }
            }
            .orEmpty()
        return KSMCapabilities(
            platform = KSMPlatform.ANDROID,
            commands = commands,
            permissions = permissions,
            plugins = plugins
        )
    }
}
