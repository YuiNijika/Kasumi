package dev.kasumi.app.bridge

import android.content.ComponentCallbacks2
import android.os.Build
import android.webkit.WebSettings
import android.webkit.WebView
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import dev.kasumi.app.MainActivity
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class KSMWebViewRuntimeTest {
    @get:Rule
    val activityRule = ActivityScenarioRule(MainActivity::class.java)

    @Test
    fun runtimeConfiguresMemorySensitiveDefaults() {
        activityRule.scenario.onActivity { activity ->
            val webView = WebView(activity)
            try {
                KSMWebViewRuntime.configure(webView, debug = false)
                val settings = webView.settings

                assertTrue(settings.javaScriptEnabled)
                assertTrue(settings.domStorageEnabled)
                assertFalse(settings.allowFileAccess)
                assertFalse(settings.allowContentAccess)
                assertFalse(settings.javaScriptCanOpenWindowsAutomatically)
                assertFalse(settings.supportMultipleWindows())
                assertTrue(settings.mediaPlaybackRequiresUserGesture)
                assertFalse(settings.offscreenPreRaster)
                assertTrue(settings.cacheMode == WebSettings.LOAD_DEFAULT)
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    assertTrue(webView.rendererPriorityWaivedWhenNotVisible)
                }

                KSMWebViewRuntime.pause(webView)
                KSMWebViewRuntime.resume(webView)
                KSMWebViewRuntime.trimMemory(
                    webView,
                    ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW
                )
            } finally {
                KSMWebViewRuntime.destroy(webView)
            }
        }
    }
}
