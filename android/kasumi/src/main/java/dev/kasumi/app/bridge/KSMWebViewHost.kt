package dev.kasumi.app.bridge

import android.app.Activity
import android.content.ComponentCallbacks2
import android.net.Uri
import android.view.View
import android.webkit.WebChromeClient
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.webkit.JavaScriptReplyProxy
import androidx.webkit.WebViewCompat
import dev.kasumi.app.KSMAndroidHostServices
import java.io.Closeable
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.RejectedExecutionException
import java.util.concurrent.ThreadFactory
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.sync.Semaphore
import kotlinx.coroutines.withTimeout
import org.json.JSONObject
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

class KSMWebViewHost(
    private val activity: Activity,
    private val fileSystem: KSMFileSystem,
    private val hostServices: KSMAndroidHostServices,
    private val entryUrl: String,
    assetRoot: String = "",
    allowedCommands: Collection<String> = emptyList(),
    allowedPermissions: Collection<String> = emptyList(),
    webViewConfig: KSMWebViewConfig = KSMWebViewConfig.default(),
    private val debug: Boolean = false,
    private val onNativeEvent: ((String) -> Unit)? = null,
    private val fullscreen: Boolean = true,
    plugins: Collection<KSMPlugin> = emptyList(),
    val windowId: String = nextWindowId()
) : Closeable {
    private class ActiveCommand {
        private val cancelled = AtomicBoolean(false)
        private val suppressResponse = AtomicBoolean(false)

        @Volatile
        private var job: Job? = null

        fun bind(job: Job) {
            this.job = job
            if (cancelled.get()) {
                job.cancel()
            }
        }

        fun cancel(silent: Boolean = false) {
            if (silent) {
                suppressResponse.set(true)
            }
            cancelled.set(true)
            job?.cancel()
        }

        fun isCancelled(): Boolean {
            return cancelled.get()
        }

        fun shouldSuppressResponse(): Boolean {
            return suppressResponse.get()
        }
    }

    val webView = WebView(activity)

    private val router = KSMRouter(
        fileSystem,
        hostServices,
        allowedCommands = allowedCommands,
        allowedPermissions = allowedPermissions,
        eventSender = { event ->
            dispatchNativeEvent(event)
            dispatchWebEvent(event)
        },
        plugins = plugins,
        requestScope = windowId
    ).also {
        hostServices?.setEventDispatcher { event, payload ->
            val json = try {
                org.json.JSONObject(payload)
            } catch (_: org.json.JSONException) {
                org.json.JSONObject()
            }
            it.emitEvent(event, json)
        }
    }
    private val commandScope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val commandExecutor = ThreadPoolExecutor(
        MAX_CONCURRENT_COMMANDS,
        MAX_CONCURRENT_COMMANDS,
        0L,
        TimeUnit.MILLISECONDS,
        LinkedBlockingQueue<Runnable>(MAX_QUEUED_COMMANDS),
        ThreadFactory { task ->
            Thread(task, "kasumi-command-${workerIdSequence.getAndIncrement()}").apply {
                isDaemon = true
            }
        },
        ThreadPoolExecutor.AbortPolicy()
    )
    private val navigationConfig = webViewConfig
    private val activeRequests = ConcurrentHashMap<String, ActiveCommand>()
    private val commandSlots = Semaphore(MAX_CONCURRENT_COMMANDS)
    private val layoutListener = object : View.OnLayoutChangeListener {
        override fun onLayoutChange(
            view: View,
            left: Int,
            top: Int,
            right: Int,
            bottom: Int,
            oldLeft: Int,
            oldTop: Int,
            oldRight: Int,
            oldBottom: Int
        ) {
            if (closed) {
                return
            }
            val width = right - left
            val height = bottom - top
            val oldWidth = oldRight - oldLeft
            val oldHeight = oldBottom - oldTop
            if (
                width > 0 &&
                height > 0 &&
                (width != oldWidth || height != oldHeight)
            ) {
                router.emitEvent(
                    "window.resized",
                    JSONObject()
                        .put("width", width)
                        .put("height", height)
                )
            }
        }
    }

    @Volatile
    private var closed = false

    @Volatile
    private var started = false

    @Volatile
    private var timersPaused = false

    private val pendingReload = AtomicBoolean(false)

    init {
        val entryUri = Uri.parse(entryUrl)
        require(
            navigationConfig.isAllowedNavigation(entryUrl) &&
                (
                    !debug ||
                        navigationConfig.productionOrigin ==
                            KSMWebViewConfig.originFor(
                                entryUri.scheme,
                                entryUri.host,
                                entryUri.port
                            ) ||
                        navigationConfig.isAllowedDevelopmentEntry(entryUrl)
                    )
        ) {
            "entryUrl must use the Kasumi application or development origin"
        }

        KSMWebViewRuntime.prepareWindow(activity, fullscreen)
        KSMWebViewRuntime.configure(webView, debug)
        configureWebView(assetRoot)
        KSM.registerWindow(windowId, router, fileSystem, hostServices)
    }

    fun start() {
        if (closed || started) {
            return
        }
        started = true
        if (fullscreen) {
            KSMWebViewRuntime.applyFullscreen(activity)
        }
        webView.loadUrl(entryUrl)
    }

    fun onResume() {
        if (!closed) {
            if (fullscreen) {
                KSMWebViewRuntime.applyFullscreen(activity)
            }
            if (timersPaused) {
                timersPaused = false
                KSMWebViewRuntime.resume(webView)
            } else {
                webView.onResume()
            }
        }
    }

    fun onPause() {
        if (!closed && !timersPaused) {
            timersPaused = true
            KSMWebViewRuntime.pause(webView)
        }
    }

    fun onTrimMemory(level: Int) {
        if (closed) {
            return
        }
        if (level >= ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW) {
            hostServices.cancelDownloads(windowId)
        }
        KSMWebViewRuntime.trimMemory(webView, level)
    }

    fun onLowMemory() {
        if (closed) {
            return
        }
        hostServices.cancelDownloads(windowId)
        KSMWebViewRuntime.trimMemory(
            webView,
            ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL
        )
    }

    fun onWindowFocusChanged(hasFocus: Boolean) {
        if (hasFocus && !closed && fullscreen) {
            KSMWebViewRuntime.applyFullscreen(activity)
        }
    }

    override fun close() {
        if (closed) {
            return
        }
        closed = true
        commandScope.cancel()
        resetSession(silent = true)
        commandExecutor.shutdownNow()
        if (timersPaused) {
            timersPaused = false
            KSMWebViewRuntime.resume(webView)
        }
        KSM.unregisterWindow(windowId)
        WebViewCompat.removeWebMessageListener(webView, MESSAGE_CHANNEL)
        webView.removeOnLayoutChangeListener(layoutListener)
        KSMWebViewRuntime.destroy(webView)
    }

    private fun configureWebView(assetRoot: String) {
        val assetLoader = KSMWebAssetLoader.create(
            activity,
            assetRoot,
            navigationConfig
        )
        webView.webViewClient = object : WebViewClient() {
            override fun shouldInterceptRequest(
                view: WebView,
                request: WebResourceRequest
            ): WebResourceResponse? {
                return assetLoader.shouldInterceptRequest(request.url)
            }

            override fun onPageFinished(view: WebView, url: String) {
                super.onPageFinished(view, url)
                if (!closed && isAllowedUrl(url)) {
                    router.emitSystemEvent("webview.navigated", JSONObject())
                    if (pendingReload.getAndSet(false)) {
                        router.emitSystemEvent("webview.reloaded", JSONObject())
                    }
                }
            }

            override fun onPageStarted(
                view: WebView,
                url: String,
                favicon: android.graphics.Bitmap?
            ) {
                super.onPageStarted(view, url, favicon)
                if (!closed) {
                    resetSession(silent = true)
                }
            }

            override fun doUpdateVisitedHistory(
                view: WebView,
                url: String?,
                isReload: Boolean
            ) {
                super.doUpdateVisitedHistory(view, url, isReload)
                pendingReload.set(isReload)
            }

            override fun shouldOverrideUrlLoading(
                view: WebView,
                request: WebResourceRequest
            ): Boolean {
                if (isAllowedUrl(request.url.toString())) {
                    return false
                }
                return true
            }
        }
        webView.webChromeClient = object : WebChromeClient() {
            override fun onCreateWindow(
                view: WebView,
                isDialog: Boolean,
                isUserGesture: Boolean,
                resultMsg: android.os.Message
            ): Boolean {
                return false
            }
        }
        webView.addOnLayoutChangeListener(layoutListener)
        WebViewCompat.addWebMessageListener(
            webView,
            MESSAGE_CHANNEL,
            if (debug) {
                setOf(navigationConfig.productionOrigin) +
                    navigationConfig.developmentOrigins
            } else {
                setOf(navigationConfig.productionOrigin)
            }
        ) { _, message, _, _, replyProxy ->
            handleWebMessage(message.data ?: "", replyProxy)
        }
    }

    private fun handleWebMessage(
        payload: String,
        replyProxy: JavaScriptReplyProxy
    ) {
        if (closed) {
            return
        }
        val request = parseRequest(payload)
        if (request?.optString("type") == "cancel") {
            cancelRequest(request.optString("id"))
            return
        }

        val id = request
            ?.optString("id")
            ?.takeIf { it.isNotEmpty() }
        val timeoutMs = requestTimeoutMs(request)
        if (timeoutMs == null) {
            postResponse(replyProxy, invalidArgsResponse(id))
            return
        }

        val active = if (id == null) {
            null
        } else {
            val command = ActiveCommand()
            if (activeRequests.putIfAbsent(id, command) != null) {
                postResponse(replyProxy, busyResponse(id))
                return
            }
            command
        }
        if (!commandSlots.tryAcquire()) {
            if (id != null && active != null) {
                activeRequests.remove(id, active)
            }
            postResponse(replyProxy, busyResponse(id))
            return
        }

        commandScope.launch {
            val job = coroutineContext[Job]
            if (job != null && active != null) {
                active.bind(job)
            }
            try {
                val response = executeRequest(payload, id, timeoutMs, active)
                if (response.isNotEmpty()) {
                    postResponse(replyProxy, response)
                }
            } finally {
                if (id != null && active != null) {
                    activeRequests.remove(id, active)
                }
                commandSlots.release()
            }
        }
    }

    private suspend fun executeRequest(
        payload: String,
        id: String?,
        timeoutMs: Long,
        active: ActiveCommand?
    ): String {
        if (active?.isCancelled() == true) {
            return if (active.shouldSuppressResponse()) "" else cancelledResponse(id)
        }
        return try {
            withTimeout(timeoutMs) {
                executeOnCommandThread(payload)
            }
        } catch (_: TimeoutCancellationException) {
            if (id != null) {
                hostServices.cancelRequestWithScope(scopedRequestId(id))
            }
            timeoutResponse(id)
        } catch (_: CancellationException) {
            if (id == null) {
                throw CancellationException("Command was cancelled.")
            }
            hostServices.cancelRequestWithScope(scopedRequestId(id))
            if (active?.shouldSuppressResponse() == true) "" else cancelledResponse(id)
        }
    }

    private suspend fun executeOnCommandThread(payload: String): String {
        return suspendCancellableCoroutine { continuation ->
            try {
                commandExecutor.execute {
                    try {
                        continuation.resume(router.handleMessage(payload))
                    } catch (error: Throwable) {
                        continuation.resumeWithException(error)
                    }
                }
            } catch (_: RejectedExecutionException) {
                continuation.resumeWithException(
                    KSMHostException(
                        "BUSY",
                        "Too many concurrent commands."
                    )
                )
            }
        }
    }

    private fun cancelRequest(id: String) {
        if (id.isEmpty()) {
            return
        }
        activeRequests[id]?.cancel()
        hostServices.cancelRequestWithScope(scopedRequestId(id))
    }

    private fun resetSession(silent: Boolean) {
        val requests = activeRequests.entries.toList()
        activeRequests.clear()
        for ((id, command) in requests) {
            command.cancel(silent)
            hostServices.cancelRequestWithScope(scopedRequestId(id))
        }
    }

    private fun scopedRequestId(id: String): String {
        return "$windowId:$id"
    }

    private fun dispatchNativeEvent(event: String) {
        if (closed) {
            return
        }
        try {
            val parsed = JSONObject(event)
            if (parsed.optString("type") == "event") {
                onNativeEvent?.invoke(event)
                KSM.emitEvent(
                    windowId,
                    parsed.getString("event"),
                    parsed.optJSONObject("payload") ?: JSONObject()
                )
            }
        } catch (_: Exception) {
            // Native event listeners are best effort.
        }
    }

    private fun dispatchWebEvent(event: String) {
        if (!isActive()) {
            return
        }
        activity.runOnUiThread {
            if (isActive()) {
                webView.evaluateJavascript(
                    "window.postMessage(${JSONObject.quote(event)}, " +
                        "window.location.origin)",
                    null
                )
            }
        }
    }

    private fun parseRequest(payload: String): JSONObject? {
        return try {
            JSONObject(payload)
        } catch (_: Exception) {
            null
        }
    }

    private fun postResponse(
        replyProxy: JavaScriptReplyProxy,
        response: String
    ) {
        if (!isActive()) {
            return
        }
        activity.runOnUiThread {
            if (isActive()) {
                replyProxy.postMessage(response)
            }
        }
    }

    private fun isActive(): Boolean {
        return !closed && !activity.isFinishing && !activity.isDestroyed
    }

    private fun isAllowedUrl(url: String): Boolean {
        val parsed = Uri.parse(url)
        return navigationConfig.isAllowedNavigation(url) &&
            (
                !debug ||
                    navigationConfig.productionOrigin ==
                        KSMWebViewConfig.originFor(
                            parsed.scheme,
                            parsed.host,
                            parsed.port
                        ) ||
                    navigationConfig.isAllowedDevelopmentEntry(url)
            )
    }

    private fun busyResponse(id: String?): String {
        return errorResponse(
            id,
            "BUSY",
            "Too many concurrent commands."
        )
    }

    private fun cancelledResponse(id: String?): String {
        return errorResponse(
            id,
            "CANCELLED",
            "The request was cancelled."
        )
    }

    private fun timeoutResponse(id: String?): String {
        return errorResponse(
            id,
            "TIMEOUT",
            "The request timed out."
        )
    }

    private fun invalidArgsResponse(id: String?): String {
        return errorResponse(
            id,
            "INVALID_ARGS",
            "Request fields are invalid."
        )
    }

    private fun errorResponse(
        id: String?,
        code: String,
        message: String
    ): String {
        return JSONObject()
            .put("type", "response")
            .put("id", id ?: JSONObject.NULL)
            .put("ok", false)
            .put(
                "error",
                JSONObject()
                    .put("code", code)
                    .put("message", message)
            )
            .toString()
    }

    private fun requestTimeoutMs(request: JSONObject?): Long? {
        if (request == null || !request.has("timeoutMs")) {
            return DEFAULT_TIMEOUT_MS
        }
        val raw = request.opt("timeoutMs") as? Number ?: return null
        val value = raw.toLong()
        if (
            value <= 0 ||
            value > MAX_TIMEOUT_MS ||
            raw.toDouble() != value.toDouble()
        ) {
            return null
        }
        return value
    }

    companion object {
        private const val MAX_CONCURRENT_COMMANDS = 8
        private const val MAX_QUEUED_COMMANDS = 32
        private const val DEFAULT_TIMEOUT_MS = 10000L
        private const val MAX_TIMEOUT_MS = 600000L
        private const val MESSAGE_CHANNEL = "ksm"
        private val windowIdSequence = AtomicLong(1)
        private val workerIdSequence = AtomicLong(1)

        private fun nextWindowId(): String {
            return "window-${windowIdSequence.getAndIncrement()}"
        }
    }
}
