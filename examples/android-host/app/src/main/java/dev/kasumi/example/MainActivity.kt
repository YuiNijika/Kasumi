package dev.kasumi.example

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import dev.kasumi.app.KSMAndroidHostServices
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
            allowedExternalOrigins = BuildConfig.KSM_SHELL_ALLOWED_ORIGINS.toSet(),
            fileSystem = fileSystem,
            appVersion = BuildConfig.VERSION_NAME,
            allowedHttpOrigins = BuildConfig.KSM_HTTP_ALLOWED_ORIGINS.toSet()
        )
        webViewHost = KSMWebViewHost(
            activity = this,
            fileSystem = fileSystem,
            hostServices = hostServices,
            entryUrl = entryUrl(),
            allowedCommands = BuildConfig.KSM_COMMANDS.toList(),
            allowedPermissions = BuildConfig.KSM_PERMISSIONS.toList(),
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

    private fun entryUrl(): String {
        val requested = intent?.getStringExtra(EXTRA_DEV_URL)
            ?: return PRODUCTION_ENTRY_URL
        if (!BuildConfig.DEBUG) {
            return PRODUCTION_ENTRY_URL
        }
        val parsed = Uri.parse(requested)
        val isDevelopmentEntry =
            parsed.scheme == "http" &&
                parsed.host == "localhost" &&
                parsed.port == DEV_PORT &&
                parsed.path == "/index.html"
        return if (isDevelopmentEntry) requested else PRODUCTION_ENTRY_URL
    }

    companion object {
        private const val DEV_PORT = 5173
        private const val EXTRA_DEV_URL = "ksmDevUrl"
        private const val PRODUCTION_ENTRY_URL = "https://app.local/index.html"
    }
}
