package dev.kasumi.app.bridge

import dev.kasumi.app.generated.KSMGeneratedContract
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class CapabilityFoundationTest {
    private class TestHostServices(
        private val fileSystem: KSMFileSystem? = null,
        private val allowedOrigins: Set<String> = setOf("https://example.com")
    ) : KSMHostServices {
        private val jars = mutableMapOf<String, MutableList<KSMCookie>>()
        private val scopes = mutableMapOf<String, Boolean>()
        private val cancelled = mutableSetOf<String>()
        private val transfers = mutableMapOf<String, Boolean>()
        private var dispatcher: (String, String) -> Unit = { _, _ -> }

        override fun showMessage(title: String, message: String): Boolean = false
        override fun confirm(title: String, message: String): Boolean = false
        override fun openFile(filters: List<String>): String? = null
        override fun saveFile(suggestedName: String): String? = null
        override fun readPickedText(path: String): String? = null
        override fun writePickedText(path: String, content: String): Boolean = false
        override fun pickedExists(path: String): Boolean = false
        override fun removePicked(path: String): Boolean = false
        override fun readBundledText(path: String): String? = null
        override fun bundledExists(path: String): Boolean = false
        override fun readClipboardText(): String? = null
        override fun writeClipboardText(text: String): Boolean = false
        override fun showNotification(title: String, body: String): Boolean = false
        override fun requestPermission(permission: String): Boolean = true
        override fun closeWindow(): Boolean = false
        override fun exitApp(): Boolean = false

        override fun isHttpRequestUrlAllowed(url: String): Boolean {
            return KSMHttpPolicy.isAllowedDownloadUrl(
                url,
                KSMHttpPolicy.normalizeOrigins(allowedOrigins)
            )
        }

        override fun setCookies(
            jarId: String,
            url: String,
            cookies: List<KSMCookie>
        ): Boolean {
            jars[jarId] = cookies.toMutableList()
            return true
        }

        override fun getCookies(jarId: String, url: String): List<KSMCookie> {
            return jars[jarId].orEmpty().toList()
        }

        override fun clearCookies(jarId: String, url: String): Boolean {
            return jars.remove(jarId) != null
        }

        override fun startHttpTransfer(
            request: KSMHttpTransferSpec,
            requestId: String?
        ): String? {
            val id = "transfer-${transfers.size + 1}"
            transfers[id] = false
            return id
        }

        override fun cancelHttpTransfer(transferId: String): Boolean {
            return transfers[transferId]?.let {
                transfers[transferId] = true
                true
            } ?: false
        }

        override fun releaseScope(id: String): Boolean {
            return scopes.remove(id) != null
        }

        override fun isRequestCancelled(requestId: String): Boolean {
            return requestId in cancelled
        }

        override fun cancelRequestWithScope(requestId: String) {
            cancelled.add(requestId)
            cancelRequest(requestId)
        }

        override fun setEventDispatcher(
            dispatcher: (event: String, payload: String) -> Unit
        ) {
            this.dispatcher = dispatcher
        }

        override fun emitWindowEvent(
            windowId: String?,
            event: String,
            payload: String
        ): Boolean {
            dispatcher(event, payload)
            return true
        }

        override fun takePendingDeepLink(): String? = null
        override fun openDevtools(enabled: Boolean): Boolean = false
        override fun configureTray(config: KSMTrayConfig): Boolean = false
        override fun installUpdate(
            packagePath: String,
            expectedHash: String,
            signature: String,
            requestId: String?
        ): Boolean = false
        override fun audioDevices(): List<KSMAudioDevice>? = null
        override fun audioState(): KSMAudioState? = null
        override fun probeAudio(path: String): KSMAudioProbe? = null
        override fun loadAudio(path: String): Boolean = false
        override fun setAudioPlayback(playing: Boolean): Boolean = false
        override fun seekAudio(positionMs: Long): Boolean = false
        override fun updateMediaSession(
            metadata: KSMMediaSessionMetadata
        ): Boolean = false
        override fun clearMediaSession(): Boolean = false
        override fun createWindow(
            route: String,
            state: KSMWindowState
        ): String? = null
        override fun showWindowById(windowId: String, visible: Boolean): Boolean = false
        override fun setWindowPosition(
            windowId: String,
            x: Int,
            y: Int
        ): Boolean = false
        override fun setWindowAlwaysOnTop(
            windowId: String,
            enabled: Boolean
        ): Boolean = false
        override fun setWindowTransparent(
            windowId: String,
            enabled: Boolean
        ): Boolean = false
        override fun windowState(windowId: String): KSMWindowState? = null

        fun addScope(id: String) {
            scopes[id] = true
        }

        fun dispatcher(): (String, String) -> Unit = dispatcher
    }

    private fun allPermissions(): List<String> {
        val capabilityPlugin = ksmDefaultPlugins().first {
            it.id == "capabilities"
        }
        return (
            KSMGeneratedContract.permissions +
                capabilityPlugin.permissions
            ).distinct()
    }

    private fun capabilityCommands(): List<String> {
        val capabilityPlugin = ksmDefaultPlugins().first {
            it.id == "capabilities"
        }
        return (
            KSMGeneratedContract.androidCommands +
                capabilityPlugin.commands
            ).distinct()
    }

    private fun router(
        host: KSMHostServices,
        events: MutableList<String> = mutableListOf()
    ): KSMRouter {
        return KSMRouter(
            fileSystem = KSMFileSystem(
                File(System.getProperty("java.io.tmpdir"), "kasumi-capability-data"),
                File(System.getProperty("java.io.tmpdir"), "kasumi-capability-cache"),
                File(System.getProperty("java.io.tmpdir"), "kasumi-capability-tmp")
            ),
            hostServices = host,
            allowedCommands = capabilityCommands(),
            allowedPermissions = allPermissions(),
            eventSender = { events.add(it) },
            plugins = emptyList(),
            requestScope = "window-1"
        )
    }

    private fun invoke(
        router: KSMRouter,
        command: String,
        args: JSONObject = JSONObject()
    ): JSONObject {
        return JSONObject(
            router.handleMessage(
                JSONObject()
                    .put("type", "request")
                    .put("id", "request-1")
                    .put("cmd", command)
                    .put("args", args)
                    .toString()
            )
        )
    }

    @Test
    fun httpValidationRejectsUnsafeUrlsAndHeaders() {
        val host = TestHostServices()
        val router = router(host)
        val denied = invoke(
            router,
            "http.request",
            JSONObject().put("url", "https://not-allowed.example/path")
        )
        assertEquals("PERMISSION_DENIED", denied.getJSONObject("error").getString("code"))
        val invalidHeader = invoke(
            router,
            "http.request",
            JSONObject()
                .put("url", "https://example.com/path")
                .put("headers", JSONObject().put("Cookie", "session=1"))
        )
        assertEquals("PERMISSION_DENIED", invalidHeader.getJSONObject("error").getString("code"))
        val invalidCookieArgs = invoke(
            router,
            "http.cookies.get",
            JSONObject().put("url", "https://example.com")
        )
        assertEquals(
            "INVALID_ARGS",
            invalidCookieArgs.getJSONObject("error").getString("code")
        )
    }

    @Test
    fun cookieJarsAreIsolated() {
        val host = TestHostServices()
        val router = router(host)
        val url = JSONObject().put("url", "https://example.com")
        val cookie = JSONObject().put("name", "session").put("value", "one")
        assertTrue(
            invoke(
                router,
                "http.cookies.set",
                JSONObject()
                    .put("jarId", "left")
                    .put("url", url.getString("url"))
                    .put("cookies", org.json.JSONArray().put(cookie))
            ).getBoolean("ok")
        )
        val left = invoke(
            router,
            "http.cookies.get",
            JSONObject().put("jarId", "left").put("url", url.getString("url"))
        )
        val right = invoke(
            router,
            "http.cookies.get",
            JSONObject().put("jarId", "right").put("url", url.getString("url"))
        )
        assertEquals(1, left.getJSONObject("data").getJSONArray("cookies").length())
        assertEquals(0, right.getJSONObject("data").getJSONArray("cookies").length())
    }

    @Test
    fun pathScopeCommandsFailClosedWithoutActiveScope() {
        val host = TestHostServices()
        val router = router(host)
        val walk = invoke(
            router,
            "fs.walk",
            JSONObject().put("path", "app://scope/missing/file.bin")
        )
        assertEquals("NOT_SUPPORTED", walk.getJSONObject("error").getString("code"))
        val invalid = invoke(
            router,
            "fs.walk",
            JSONObject().put("path", "app://data/../secret")
        )
        assertEquals("PERMISSION_DENIED", invalid.getJSONObject("error").getString("code"))
    }

    @Test
    fun scopedPathsRejectTraversalBeforeHostResolution() {
        val router = router(TestHostServices())
        val traversal = invoke(
            router,
            "fs.walk",
            JSONObject().put("path", "app://scope/active/../outside")
        )
        assertEquals(
            "PERMISSION_DENIED",
            traversal.getJSONObject("error").getString("code")
        )
    }

    @Test
    fun capabilityEventsRequireSubscriptionAndCancelFailsClosed() {
        val host = TestHostServices()
        val events = mutableListOf<String>()
        val router = router(host, events)
        val subscribed = invoke(
            router,
            "event.subscribe",
            JSONObject().put("event", "http.transfer.completed")
        )
        assertTrue(subscribed.getBoolean("ok"))
        host.setEventDispatcher { event, payload ->
            router.emitEvent(event, JSONObject(payload))
        }
        host.dispatcher()(
            "http.transfer.completed",
            JSONObject().put("transferId", "transfer-1").toString()
        )
        assertTrue(events.any { it.contains("http.transfer.completed") })
        val cancelled = invoke(
            router,
            "http.transfer.cancel",
            JSONObject().put("transferId", "missing")
        )
        assertEquals("NOT_SUPPORTED", cancelled.getJSONObject("error").getString("code"))
        val emitted = invoke(
            router,
            "event.emit",
            JSONObject()
                .put("event", "http.transfer.completed")
                .put("payload", JSONObject().put("transferId", "transfer-2"))
        )
        assertTrue(emitted.getBoolean("ok"))
        assertTrue(
            events.any {
                it.contains("http.transfer.completed") &&
                    it.contains("transfer-2")
            }
        )
    }

    @Test
    fun unsupportedNativeCapabilitiesDoNotClaimSuccess() {
        val host = TestHostServices()
        val router = router(host)
        val audio = invoke(
            router,
            "audio.devices.list",
            JSONObject()
        )
        assertEquals("NOT_SUPPORTED", audio.getJSONObject("error").getString("code"))
        val media = invoke(
            router,
            "media.probe",
            JSONObject().put("path", "app://scope/missing/file.mp3")
        )
        assertEquals("NOT_SUPPORTED", media.getJSONObject("error").getString("code"))
        val window = invoke(
            router,
            "window.create",
            JSONObject().put("route", "about")
        )
        assertFalse(window.getBoolean("ok"))
        val pending = invoke(
            router,
            "app.deepLink.getPending",
            JSONObject()
        )
        assertTrue(pending.getBoolean("ok"))
        assertTrue(pending.getJSONObject("data").isNull("url"))
        val unsupportedEmit = invoke(
            router,
            "event.emit",
            JSONObject().put("event", "app.secondInstance.received")
        )
        assertEquals(
            "NOT_SUPPORTED",
            unsupportedEmit.getJSONObject("error").getString("code")
        )
    }
}
