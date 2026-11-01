package dev.kasumi.app.bridge

import android.app.Activity
import android.content.ComponentCallbacks2
import android.graphics.Color
import android.os.Build
import android.view.View
import android.view.ViewGroup
import android.view.Window
import android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
import android.webkit.WebView
import android.webkit.WebViewClient
import android.webkit.WebSettings.LOAD_DEFAULT
import android.webkit.WebSettings.MIXED_CONTENT_NEVER_ALLOW
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.core.view.WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE

object KSMWebViewRuntime {
    fun prepareWindow(activity: Activity, fullscreen: Boolean = true) {
        activity.requestWindowFeature(Window.FEATURE_NO_TITLE)
        if (!fullscreen) {
            return
        }
        @Suppress("DEPRECATION")
        activity.window.apply {
            statusBarColor = Color.TRANSPARENT
            navigationBarColor = Color.TRANSPARENT
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            val attributes = activity.window.attributes
            attributes.layoutInDisplayCutoutMode =
                LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            activity.window.attributes = attributes
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            activity.window.isStatusBarContrastEnforced = false
            activity.window.isNavigationBarContrastEnforced = false
        }
        applyFullscreen(activity)
    }

    fun applyFullscreen(activity: Activity) {
        val window = activity.window
        WindowCompat.setDecorFitsSystemWindows(window, false)
        val controller = WindowInsetsControllerCompat(window, window.decorView)
        controller.hide(WindowInsetsCompat.Type.systemBars())
        controller.systemBarsBehavior =
            BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
    }

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
            cacheMode = LOAD_DEFAULT
            offscreenPreRaster = false
            mixedContentMode = MIXED_CONTENT_NEVER_ALLOW
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
