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
}
