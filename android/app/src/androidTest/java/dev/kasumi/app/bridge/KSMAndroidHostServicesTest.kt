package dev.kasumi.app.bridge

import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import dev.kasumi.app.MainActivity
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class KSMAndroidHostServicesTest {
    @get:Rule
    val activityRule = ActivityScenarioRule(MainActivity::class.java)

    @Test
    fun hostServicesExposeDeviceInfoAndExactOriginPolicy() {
        activityRule.scenario.onActivity { activity ->
            val services = KSMAndroidHostServices(
                activity,
                setOf("https://example.com")
            )
            try {
                assertTrue(
                    services.isExternalUrlAllowed(
                        "https://example.com/docs?q=1"
                    )
                )
                assertFalse(
                    services.isExternalUrlAllowed(
                        "https://example.com.evil.test/docs"
                    )
                )
                assertFalse(
                    services.isExternalUrlAllowed(
                        "https://example.com:444/docs"
                    )
                )
                assertFalse(
                    services.isExternalUrlAllowed(
                        "http://example.com/docs"
                    )
                )
                assertFalse(
                    services.isExternalUrlAllowed(
                        "file:///data/local/tmp/file"
                    )
                )

                val info = services.deviceInfo()
                assertEquals("android", info.platform)
                assertTrue(info.osVersion.isNotEmpty())
                assertTrue(info.model.isNotEmpty())
                assertTrue(info.manufacturer.isNotEmpty())
                assertTrue(info.appVersion.isNotEmpty())
                assertTrue(info.language.isNotEmpty())
                assertTrue(info.screenWidth > 0)
                assertTrue(info.screenHeight > 0)
                assertTrue(info.density > 0)
                assertTrue(info.deviceName.isNotEmpty())
                assertTrue(info.timezone.isNotEmpty())
                assertTrue(info.architecture.isNotEmpty())
            } finally {
                services.shutdown()
            }
        }
    }
}
