package dev.kasumi.app

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.webkit.WebView
import android.webkit.WebViewClient
import android.webkit.WebResourceRequest
import androidx.webkit.WebViewAssetLoader
import androidx.webkit.WebViewCompat
import dev.kasumi.app.bridge.KSMFileSystem
import dev.kasumi.app.bridge.KSMRouter
import java.io.File
import java.util.concurrent.ConcurrentHashMap
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Semaphore
import org.json.JSONObject

class MainActivity : Activity() {
    private data class ActiveCommand(
        val job: Job,
        val thread: Thread
    )

    private lateinit var webView: WebView
    private lateinit var router: KSMRouter
    private lateinit var hostServices: KSMAndroidHostServices
    private val commandScope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val activeRequests = ConcurrentHashMap<String, ActiveCommand>()
    private val cancelledRequests = ConcurrentHashMap.newKeySet<String>()
    private val commandSlots = Semaphore(MAX_CONCURRENT_COMMANDS)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        webView = WebView(this)
        hostServices = KSMAndroidHostServices(this)
        router = KSMRouter(
            KSMFileSystem(
                File(filesDir, "data"),
                cacheDir,
                File(cacheDir, "tmp")
            ),
            hostServices
        ) { event ->
            runOnUiThread {
                if (!isFinishing && !isDestroyed) {
                    webView.evaluateJavascript(
                        "window.postMessage(${JSONObject.quote(event)}, window.location.origin)",
                        null
                    )
                }
            }
        }

        webView.settings.javaScriptEnabled = true
        webView.settings.allowFileAccess = false
        webView.settings.allowContentAccess = false
        webView.settings.mixedContentMode =
            android.webkit.WebSettings.MIXED_CONTENT_NEVER_ALLOW
        WebView.setWebContentsDebuggingEnabled(BuildConfig.DEBUG)
        val assetLoader = WebViewAssetLoader.Builder()
            .addPathHandler("/assets/", WebViewAssetLoader.AssetsPathHandler(this))
            .build()
        webView.webViewClient = object : android.webkit.WebViewClient() {
            override fun shouldInterceptRequest(
                view: WebView,
                request: WebResourceRequest
            ): android.webkit.WebResourceResponse? {
                return assetLoader.shouldInterceptRequest(request.url)
            }

            override fun onPageFinished(view: WebView, url: String) {
                super.onPageFinished(view, url)
                if (isAllowedUrl(url)) {
                    router.emitEvent("webview.reloaded", JSONObject())
                }
            }

            override fun shouldOverrideUrlLoading(
                view: WebView,
                request: WebResourceRequest
            ): Boolean {
                val url = request.url.toString()
                if (isAllowedUrl(url)) {
                    return false
                }
                startActivity(Intent(Intent.ACTION_VIEW, request.url))
                return true
            }
        }
        webView.webChromeClient = object : android.webkit.WebChromeClient() {
            override fun onCreateWindow(
                view: WebView,
                isDialog: Boolean,
                isUserGesture: Boolean,
                resultMsg: android.os.Message
            ): Boolean {
                return false
            }
        }
        webView.addOnLayoutChangeListener {
            _,
            left,
            top,
            right,
            bottom,
            oldLeft,
            oldTop,
            oldRight,
            oldBottom ->
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
                    JSONObject().put("width", width).put("height", height)
                )
            }
        }
        WebViewCompat.addWebMessageListener(
            webView,
            "ksm",
            setOf("https://app.local", "http://localhost:5173")
        ) { _, message, _, _, replyProxy ->
            val payload = message.data ?: ""
            commandScope.launch {
                val request = try {
                    JSONObject(payload)
                } catch (_: Exception) {
                    null
                }
                if (request?.optString("type") == "cancel") {
                    request.optString("id").takeIf { it.isNotEmpty() }?.let { id ->
                        cancelledRequests.add(id)
                        activeRequests[id]?.let { active ->
                            active.job.cancel()
                            active.thread.interrupt()
                        }
                    }
                    return@launch
                }
                val id = request?.optString("id")?.takeIf { it.isNotEmpty() }
                if (!commandSlots.tryAcquire()) {
                    postResponse(
                        { response -> replyProxy.postMessage(response) },
                        busyResponse(id)
                    )
                    return@launch
                }
                val job = coroutineContext[Job]
                if (id != null && job != null) {
                    activeRequests[id] = ActiveCommand(job, Thread.currentThread())
                }
                try {
                    val response = if (
                        id != null && cancelledRequests.remove(id)
                    ) {
                        cancelledResponse(id)
                    } else {
                        val result = router.handleMessage(payload)
                        if (id != null && cancelledRequests.remove(id)) {
                            cancelledResponse(id)
                        } else {
                            result
                        }
                    }
                    if (response.isNotEmpty()) {
                        postResponse(
                            { value -> replyProxy.postMessage(value) },
                            response
                        )
                    }
                } finally {
                    if (id != null) {
                        val active = activeRequests[id]
                        if (active?.job === job) {
                            activeRequests.remove(id, active)
                        }
                    }
                    commandSlots.release()
                }
            }
        }
        setContentView(webView)
        webView.loadUrl("https://app.local/assets/index.html")
    }

    override fun onDestroy() {
        commandScope.cancel()
        webView.destroy()
        super.onDestroy()
    }

    override fun onActivityResult(
        requestCode: Int,
        resultCode: Int,
        data: Intent?
    ) {
        super.onActivityResult(requestCode, resultCode, data)
        hostServices.handleActivityResult(requestCode, resultCode, data)
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        hostServices.handleRequestPermissionsResult(requestCode, grantResults)
    }

    private fun postResponse(
        send: (String) -> Unit,
        response: String
    ) {
        runOnUiThread {
            if (!isFinishing && !isDestroyed) {
                send(response)
            }
        }
    }

    private fun busyResponse(id: String?): String {
        return JSONObject()
            .put("type", "response")
            .put("id", id ?: JSONObject.NULL)
            .put("ok", false)
            .put(
                "error",
                JSONObject()
                    .put("code", "BUSY")
                    .put("message", "Too many concurrent commands.")
            )
            .toString()
    }

    private fun cancelledResponse(id: String): String {
        return JSONObject()
            .put("type", "response")
            .put("id", id)
            .put("ok", false)
            .put(
                "error",
                JSONObject()
                    .put("code", "CANCELLED")
                    .put("message", "The request was cancelled.")
            )
            .toString()
    }

    private fun isAllowedUrl(url: String): Boolean {
        val parsed = Uri.parse(url)
        if (parsed.scheme == "https" && parsed.host == "app.local") {
            return true
        }
        return parsed.scheme == "http" &&
            parsed.host == "localhost" &&
            (parsed.port == 5173 || parsed.port == -1)
    }

    companion object {
        private const val MAX_CONCURRENT_COMMANDS = 8
    }
}
