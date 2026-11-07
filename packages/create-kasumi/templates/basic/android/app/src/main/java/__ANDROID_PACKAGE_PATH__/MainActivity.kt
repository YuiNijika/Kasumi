package __ANDROID_PACKAGE_NAME__

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import dev.kasumi.app.KSMAndroidHostServices
import dev.kasumi.app.bridge.KSMDevelopmentEntry
import dev.kasumi.app.bridge.KSMFileSystem
import dev.kasumi.app.bridge.KSMWebViewHost
import dev.kasumi.app.bridge.KSMWebViewConfig
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
        val webViewConfig = KSMWebViewConfig(
            productionEntry = BuildConfig.KSM_WEBVIEW_PRODUCTION_ENTRY,
            developmentEntries = BuildConfig.KSM_WEBVIEW_DEVELOPMENT_ENTRIES.toList(),
            supportedHtmlFiles = BuildConfig.KSM_WEBVIEW_SUPPORTED_HTML_FILES.toList()
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
            entryUrl = KSMDevelopmentEntry.resolve(
                intent,
                BuildConfig.DEBUG,
                webViewConfig
            ),
            allowedCommands = BuildConfig.KSM_COMMANDS.toList(),
            allowedPermissions = BuildConfig.KSM_PERMISSIONS.toList(),
            webViewConfig = webViewConfig,
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

}
