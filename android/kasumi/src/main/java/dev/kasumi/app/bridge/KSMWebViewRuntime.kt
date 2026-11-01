package dev.kasumi.app.bridge

import android.content.ComponentCallbacks2
import android.os.Build
import android.view.View
import android.view.ViewGroup
import android.webkit.WebView
import android.webkit.WebViewClient

object KSMWebViewRuntime {
    fun configure(webView: WebView, debug: Boolean = false) {
        webView.settings.apply {
            javaScriptEnabled = true
            domStorageEnabled = true
            allowFileAccess = false
            allowContentAccess = false
            javaScriptCanOpenWindowsAutomatically = false
            setSupportMultipleWindows(false)
            mediaPlaybackRequiresUserGesture = true
            builtInZoomControls = false
            displayZoomControls = false
            cacheMode = android.webkit.WebSettings.LOAD_DEFAULT
            offscreenPreRaster = false
            mixedContentMode = android.webkit.WebSettings.MIXED_CONTENT_NEVER_ALLOW
            setGeolocationEnabled(false)
        }
        webView.setOverScrollMode(View.OVER_SCROLL_NEVER)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            webView.setRendererPriorityPolicy(
                WebView.RENDERER_PRIORITY_BOUND,
                true
            )
        }
        WebView.setWebContentsDebuggingEnabled(debug)
    }

    fun pause(webView: WebView) {
        webView.onPause()
        webView.pauseTimers()
    }

    fun resume(webView: WebView) {
        webView.resumeTimers()
        webView.onResume()
    }

    fun trimMemory(webView: WebView, level: Int) {
        if (level >= ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN) {
            webView.clearCache(false)
        }
        if (level >= ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW) {
            webView.freeMemory()
        }
    }

    fun destroy(webView: WebView) {
        (webView.parent as? ViewGroup)?.removeView(webView)
        webView.resumeTimers()
        webView.stopLoading()
        webView.clearHistory()
        webView.removeAllViews()
        webView.webViewClient = WebViewClient()
        webView.webChromeClient = null
        webView.destroy()
    }
}
