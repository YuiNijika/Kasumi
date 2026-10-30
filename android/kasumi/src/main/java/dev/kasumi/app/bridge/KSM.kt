package dev.kasumi.app.bridge

import dev.kasumi.app.generated.KSMGeneratedContract
import org.json.JSONObject
import java.util.concurrent.atomic.AtomicLong

object KSM {
    private val stateLock = Any()
    private val listenerLock = Any()
    private val nextRequestId = AtomicLong(1)
    private val eventListeners =
        mutableMapOf<String, MutableSet<(JSONObject) -> Unit>>()

    private var router: KSMRouter? = null
    private var fileSystem: KSMFileSystem? = null
    private var hostServices: KSMHostServices? = null
    private var cachedCapabilities = defaultCapabilities()

    val fs = KSMFs
    val dialog = KSMDialog
    val clipboard = KSMClipboard
    val notification = KSMNotification
    val permission = KSMPermission
    val http = KSMHttp
    val shell = KSMShell
    val device = KSMDevice
    val window = KSMWindow
    val app = KSMApp

    fun init(
        router: KSMRouter,
        fileSystem: KSMFileSystem? = null,
        hostServices: KSMHostServices? = null
    ) {
        synchronized(stateLock) {
            this.router = router
            this.fileSystem = fileSystem
            this.hostServices = hostServices
            cachedCapabilities = defaultCapabilities()
        }
        synchronized(listenerLock) {
            eventListeners.clear()
        }
    }

    fun shutdown() {
        synchronized(stateLock) {
            router = null
            fileSystem = null
            hostServices = null
            cachedCapabilities = defaultCapabilities()
        }
        synchronized(listenerLock) {
            eventListeners.clear()
        }
    }

    fun invoke(
        windowId: String?,
        command: String,
        args: JSONObject
    ): JSONObject {
        val activeRouter = synchronized(stateLock) {
            checkNotNull(router) { "KSM must be initialized before use." }
        }
        val request = JSONObject()
            .put("type", "request")
            .put("id", "req-native-${nextRequestId.getAndIncrement()}")
            .put("cmd", command)
            .put("args", args)
        if (!windowId.isNullOrEmpty()) {
            request.put("windowId", windowId)
        }
        val response = JSONObject(activeRouter.handleMessage(request.toString()))
        if (response.optBoolean("ok") && command == "capabilities.query") {
            response.optJSONObject("data")?.let { data ->
                synchronized(stateLock) {
                    cachedCapabilities = capabilitiesFrom(data)
                }
            }
        }
        return response
    }

    fun invoke(command: String, args: JSONObject): JSONObject {
        return invoke(null, command, args)
    }

    fun on(event: String, handler: (JSONObject) -> Unit): () -> Unit {
        require(event in KSMGeneratedContract.events) {
            "Unknown Kasumi event: $event"
        }
        synchronized(listenerLock) {
            eventListeners
                .getOrPut(event) { mutableSetOf() }
                .add(handler)
        }
        return {
            synchronized(listenerLock) {
                val listeners = eventListeners[event] ?: return@synchronized
                listeners.remove(handler)
                if (listeners.isEmpty()) {
                    eventListeners.remove(event)
                }
            }
        }
    }

    fun emitEvent(event: String, payload: JSONObject) {
        val listeners = synchronized(listenerLock) {
            eventListeners[event]?.toList().orEmpty()
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
        return synchronized(stateLock) { cachedCapabilities }
    }

    fun queryCapabilities(): KSMCapabilities {
        return get()
    }

    internal fun requireFileSystem(): KSMFileSystem {
        return synchronized(stateLock) {
            checkNotNull(fileSystem) {
                "KSM must be initialized with a filesystem before use."
            }
        }
    }

    internal fun requireHostServices(): KSMHostServices {
        return synchronized(stateLock) {
            checkNotNull(hostServices) {
                "KSM must be initialized with host services before use."
            }
        }
    }

    private fun defaultCapabilities(): KSMCapabilities {
        return KSMCapabilities(
            platform = KSMPlatform.ANDROID,
            commands = KSMCommandRegistry.supportedCommands,
            permissions = KSMCommandRegistry.permissions
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
        return KSMCapabilities(
            platform = KSMPlatform.ANDROID,
            commands = commands,
            permissions = permissions
        )
    }
}
