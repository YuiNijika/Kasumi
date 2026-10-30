package dev.kasumi.app.bridge

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class KSMHttpPolicyTest {
    @Test
    fun validatesHttpUrlsAndCredentials() {
        assertTrue(
            KSMHttpPolicy.parseHttpUrl("https://example.com/file.bin") != null
        )
        assertTrue(
            KSMHttpPolicy.parseHttpUrl("http://example.com/file.bin") != null
        )
        assertNull(
            KSMHttpPolicy.parseHttpUrl(
                "http://example.com/file.bin",
                httpsOnly = true
            )
        )
        assertNull(KSMHttpPolicy.parseHttpUrl("file:///tmp/file.bin"))
        assertNull(KSMHttpPolicy.parseHttpUrl("javascript:alert(1)"))
 val credentialUrl = "https://user" + ":" + "invalid" + "@example.com/file"
 assertNull(KSMHttpPolicy.parseHttpUrl(credentialUrl))
        assertNull(KSMHttpPolicy.parseHttpUrl("https:///file"))
        assertNull(KSMHttpPolicy.parseHttpUrl("https://example.com:0/file"))
        assertNull(
            KSMHttpPolicy.parseHttpUrl(
                "https://example.com/" + "a".repeat(KSMHttpPolicy.MAX_URL_LENGTH)
            )
        )
    }

    @Test
    fun normalizesExactOrigins() {
        val origins = KSMHttpPolicy.normalizeOrigins(
            setOf(
                "https://example.com",
                "http://example.com:80",
                "https://EXAMPLE.com:443",
                "https://ignored.example/path"
            )
        )
        assertEquals(2, origins.size)
        assertTrue(
            KSMHttpPolicy.isAllowedExternalUrl(
                "https://example.com/docs?q=1#intro",
                origins
            )
        )
        assertTrue(
            KSMHttpPolicy.isAllowedExternalUrl(
                "http://example.com/docs",
                origins
            )
        )
        assertFalse(
            KSMHttpPolicy.isAllowedExternalUrl(
                "https://example.com:444/docs",
                origins
            )
        )
        assertFalse(
            KSMHttpPolicy.isAllowedExternalUrl(
                "https://sub.example.com/docs",
                origins
            )
        )
        assertFalse(
            KSMHttpPolicy.isAllowedExternalUrl(
                "http://example.com.evil.test/docs",
                origins
            )
        )
    }

    @Test
    fun redirectsRemainHttpsOnly() {
        val current = KSMHttpPolicy.parseHttpUrl(
            "https://example.com/start",
            httpsOnly = true
        )!!
        assertEquals(
            "https://cdn.example.com/file.bin",
            KSMHttpPolicy.resolveHttpsRedirect(
                current,
                "https://cdn.example.com/file.bin"
            ).toString()
        )
        assertNull(
            KSMHttpPolicy.resolveHttpsRedirect(
                current,
                "http://cdn.example.com/file.bin"
            )
        )
        assertNull(
            KSMHttpPolicy.resolveHttpsRedirect(
                current,
                "file:///tmp/file.bin"
            )
        )
    }
}
