package dev.kasumi.app.bridge

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class NavigationPolicyTest {
    @Test
    fun allowsOnlyConfiguredWebViewOrigins() {
        assertTrue(
            KSMNavigationPolicy.isAllowedOrigin(
                "https",
                "app.local",
                -1
            )
        )
        assertFalse(
            KSMNavigationPolicy.isAllowedOrigin(
                "http",
                "localhost",
                5173
            )
        )
        assertTrue(
            KSMNavigationPolicy.isAllowedOrigin(
                "http",
                "localhost",
                5173,
                allowDevelopmentOrigin = true
            )
        )
        assertFalse(
            KSMNavigationPolicy.isAllowedOrigin(
                "https",
                "app.local.evil.example",
                -1
            )
        )
        assertFalse(
            KSMNavigationPolicy.isAllowedOrigin(
                "http",
                "localhost",
                -1
            )
        )
        assertFalse(
            KSMNavigationPolicy.isAllowedOrigin(
                "https",
                "app.local",
                444
            )
        )
    }

    @Test
    fun allowsFrameworkDevelopmentEntriesOnly() {
        val config = KSMWebViewConfig.default()
        assertTrue(
            KSMNavigationPolicy.isAllowedDevelopmentUrl(
                "http://localhost:5173/index.html",
                config
            )
        )
        assertTrue(
            KSMNavigationPolicy.isAllowedDevelopmentUrl(
                "http://localhost:5173/",
                config
            )
        )
        assertTrue(
            KSMNavigationPolicy.isAllowedDevelopmentUrl(
                "http://localhost:5173",
                config
            )
        )
        assertFalse(
            KSMNavigationPolicy.isAllowedDevelopmentUrl(
                "http://localhost:5173/app/page",
                config
            )
        )
        assertFalse(
            KSMNavigationPolicy.isAllowedDevelopmentUrl(
                "http://localhost:5173/index.html?smoke=1",
                config
            )
        )
        assertFalse(
            KSMNavigationPolicy.isAllowedDevelopmentUrl(
                "http://localhost:5174/",
                config
            )
        )
        assertFalse(
            KSMNavigationPolicy.isAllowedDevelopmentUrl(
                "https://example.com/",
                config
            )
        )
    }

    @Test
    fun followsConfiguredWebViewEntries() {
        val config = KSMWebViewConfig(
            productionEntry = "https://app.local/start.html",
            developmentEntries = listOf(
                "http://localhost:5173/start.html",
                "http://localhost:5173/"
            ),
            supportedHtmlFiles = listOf("index.html", "start.html")
        )
        assertTrue(config.isAllowedNavigation("https://app.local/start.html"))
        assertTrue(config.isAllowedNavigation("https://app.local/index.html"))
        assertTrue(config.isAllowedDevelopmentEntry("http://localhost:5173/start.html"))
        assertTrue(config.isAllowedDevelopmentEntry("http://localhost:5173/"))
        assertFalse(config.isAllowedNavigation("https://app.local/secret.html"))
        assertFalse(config.isAllowedDevelopmentEntry("http://localhost:5173/index.html"))
        assertFalse(config.isAllowedNavigation("http://localhost:5173/secret.html"))
        assertTrue(
            KSMNavigationPolicy.isAllowedOrigin(
                "http",
                "localhost",
                5173,
                allowDevelopmentOrigin = true,
                config = config
            )
        )
        assertTrue(
            KSMNavigationPolicy.isAllowedOrigin(
                "https",
                "app.local",
                -1,
                config = config
            )
        )
    }
}
