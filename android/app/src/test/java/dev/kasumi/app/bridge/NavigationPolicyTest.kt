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
        assertTrue(
            KSMNavigationPolicy.isAllowedOrigin(
                "http",
                "localhost",
                5173
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
    fun allowsOnlyHttpAndHttpsExternalNavigation() {
        assertTrue(KSMNavigationPolicy.isAllowedExternalScheme("http"))
        assertTrue(KSMNavigationPolicy.isAllowedExternalScheme("HTTPS"))
        assertFalse(KSMNavigationPolicy.isAllowedExternalScheme("intent"))
        assertFalse(KSMNavigationPolicy.isAllowedExternalScheme("file"))
        assertFalse(KSMNavigationPolicy.isAllowedExternalScheme(null))
    }
}
