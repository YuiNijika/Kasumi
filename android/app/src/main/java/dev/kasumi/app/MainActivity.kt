package dev.kasumi.app

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import dev.kasumi.app.bridge.KSMFileSystem
import dev.kasumi.app.bridge.KSMWebViewHost
import java.io.File

class MainActivity : Activity() {
    private lateinit var hostServices: KSMAndroidHostServices
    private lateinit var webViewHost: KSMWebViewHost

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val fileSystem = KSMFileSystem(
            File(filesDir, "data"),
            cacheDir,
            File(cacheDir, "tmp")
        )
        hostServices = KSMAndroidHostServices(
            activity = this,
            allowedExternalOrigins = setOf("https://example.com"),
            fileSystem = fileSystem
        )
        webViewHost = KSMWebViewHost(
            activity = this,
            fileSystem = fileSystem,
            hostServices = hostServices,
            entryUrl = "https://app.local/index.html",
            assetRoot = "web",
            debug = BuildConfig.DEBUG
        )

        setContentView(webViewHost.webView)
        webViewHost.start()
    }

    override fun onResume() {
        super.onResume()
        if (::webViewHost.isInitialized) {
            webViewHost.onResume()
        }
    }

    override fun onPause() {
        if (::webViewHost.isInitialized) {
            webViewHost.onPause()
        }
        super.onPause()
    }

    override fun onDestroy() {
        if (::webViewHost.isInitialized) {
            webViewHost.close()
        }
        if (::hostServices.isInitialized) {
            hostServices.shutdown()
        }
        super.onDestroy()
    }

    override fun onTrimMemory(level: Int) {
        super.onTrimMemory(level)
        if (::webViewHost.isInitialized) {
            webViewHost.onTrimMemory(level)
        }
    }

    override fun onLowMemory() {
        if (::webViewHost.isInitialized) {
            webViewHost.onLowMemory()
        }
        super.onLowMemory()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (::webViewHost.isInitialized) {
            webViewHost.onWindowFocusChanged(hasFocus)
        }
    }

    override fun onActivityResult(
        requestCode: Int,
        resultCode: Int,
        data: Intent?
    ) {
        super.onActivityResult(requestCode, resultCode, data)
        if (!hostServices.handleActivityResult(requestCode, resultCode, data)) {
            hostServices.handleMultiSelectActivityResult(
                requestCode,
                resultCode,
                data
            )
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        hostServices.handleRequestPermissionsResult(requestCode, grantResults)
    }
}
