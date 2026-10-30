package dev.kasumi.app.bridge

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

@RunWith(AndroidJUnit4::class)
class BridgeInstrumentedTest {
    private fun invoke(
        router: KSMRouter,
        command: String,
        args: JSONObject
    ): JSONObject {
        return JSONObject(
            router.handleMessage(
                JSONObject()
                    .put("type", "request")
                    .put("id", "instrumented")
                    .put("cmd", command)
                    .put("args", args)
                    .toString()
            )
        )
    }

    private fun assertError(response: JSONObject, code: String) {
        assertFalse(response.getBoolean("ok"))
        assertEquals(
            code,
            response.getJSONObject("error").getString("code")
        )
    }

    private class HostServices : KSMHostServices {
        var clipboardText = "android"
        var pickedText = "picked"

        override fun appVersion(): String = "0.1.0"

        override fun language(): String = "zh-CN"

        override fun showMessage(title: String, message: String): Boolean {
            return title == "Kasumi" && message == "android"
        }

        override fun confirm(title: String, message: String): Boolean {
            return showMessage(title, message)
        }

        override fun openFile(filters: List<String>): String? = null

        override fun saveFile(suggestedName: String): String? = null

        override fun readPickedText(path: String): String? {
            return if (path == "app://picked/1") pickedText else null
        }

        override fun writePickedText(path: String, content: String): Boolean {
            if (path != "app://picked/1") {
                return false
            }
            pickedText = content
            return true
        }

        override fun pickedExists(path: String): Boolean {
            return path == "app://picked/1"
        }

        override fun removePicked(path: String): Boolean {
            if (path != "app://picked/1") {
                return false
            }
            pickedText = ""
            return true
        }

        override fun readBundledText(path: String): String? {
            return if (path == "app://res/contract.txt") "bundled" else null
        }

        override fun bundledExists(path: String): Boolean {
            return path == "app://res/contract.txt"
        }

        override fun readClipboardText(): String = clipboardText

        override fun writeClipboardText(text: String): Boolean {
            clipboardText = text
            return true
        }

        override fun showNotification(title: String, body: String): Boolean {
            return title == "Kasumi" && body == "android"
        }

        override fun requestPermission(permission: String): Boolean {
            return permission == "clipboard.read"
        }

        override fun closeWindow(): Boolean = true

        override fun exitApp(): Boolean = true
    }

    @Test
    fun rootModulesWorkOnAndroidRuntime() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val root = File(context.cacheDir, "ksm-instrumented-${System.nanoTime()}")
        val fileSystem = KSMFileSystem(
            File(root, "data"),
            File(root, "cache"),
            File(root, "tmp")
        )
        val hostServices = HostServices()
        val router = KSMRouter(fileSystem, hostServices)

        try {
            KSM.init(router, fileSystem, hostServices)
            assertTrue(
                invoke(router, "ping", JSONObject())
                    .getJSONObject("data")
                    .getBoolean("pong")
            )
            assertError(
                invoke(router, "unknown.command", JSONObject()),
                "UNKNOWN_COMMAND"
            )
            assertError(
                invoke(
                    router,
                    "fs.writeText",
                    JSONObject().put("path", "app://data/missing.txt")
                ),
                "INVALID_ARGS"
            )
            for (path in listOf(
                "app://data/../secret.txt",
                "app://data/C:/secret.txt",
                "app://data/file.txt:stream",
                "app://data/\\server\\share",
                "app://other/file.txt",
                "file:///secret.txt"
            )) {
                assertError(
                    invoke(
                        router,
                        "fs.readText",
                        JSONObject().put("path", path)
                    ),
                    "PERMISSION_DENIED"
                )
            }
            assertError(
                JSONObject(router.handleMessage("{")),
                "INVALID_ARGS"
            )

            assertTrue(KSM.fs.writeText("app://data/instrumented.txt", "android"))
            assertEquals(
                "android",
                KSM.fs.readText("app://data/instrumented.txt")
            )
            assertTrue(
                KSM.fs.makeDirectory("app://data/instrumented-dir", true) == true
            )
            assertTrue(
                KSM.fs.writeText(
                    "app://data/instrumented-dir/nested.txt",
                    "nested"
                )
            )
            assertEquals(
                KSMFileKind.DIRECTORY,
                KSM.fs.stat("app://data/instrumented-dir")?.kind
            )
            assertEquals(
                KSMFileKind.FILE,
                KSM.fs.stat(
                    "app://data/instrumented-dir/nested.txt"
                )?.kind
            )
            assertTrue(
                KSM.fs.readDir("app://data")?.any {
                    it.name == "instrumented-dir"
                } == true
            )
            assertEquals(
                "bundled",
                KSM.fs.readText("app://res/contract.txt")
            )
            assertEquals("picked", KSM.fs.readText("app://picked/1"))
            assertTrue(KSM.fs.writeText("app://picked/1", "updated"))
            assertEquals("updated", KSM.fs.readText("app://picked/1"))
            val largeContent = "x".repeat(70 * 1024)
            assertTrue(
                fileSystem.writeText(
                    "app://data/instrumented-large.txt",
                    largeContent
                )
            )
            val large = invoke(
                router,
                "fs.readText",
                JSONObject().put(
                    "path",
                    "app://data/instrumented-large.txt"
                )
            )
            assertTrue(large.getBoolean("ok"))
            val temporary = large.getJSONObject("data").getString("file")
            assertTrue(temporary.startsWith("app://tmp/"))
            assertTrue(fileSystem.exists(temporary))

            assertTrue(KSM.dialog.message("Kasumi", "android"))
            assertEquals("android", KSM.clipboard.readText())
            assertTrue(KSM.clipboard.writeText("updated"))
            assertEquals("updated", KSM.clipboard.readText())
            assertTrue(KSM.notification.show("Kasumi", "android"))
            assertTrue(KSM.permission.request("clipboard.read"))
            assertThrows(IllegalArgumentException::class.java) {
                KSM.permission.request("unknown.permission")
            }
            assertTrue(KSM.window.close())
            assertEquals(KSMPlatform.ANDROID, KSM.app.getInfo().platform)
            assertTrue(KSM.app.exit())

            var eventReceived = false
            val unsubscribe = KSM.on("window.resized") { payload ->
                eventReceived =
                    payload.getInt("width") == 800 &&
                    payload.getInt("height") == 600
            }
            KSM.emitEvent(
                "window.resized",
                JSONObject().put("width", 800).put("height", 600)
            )
            unsubscribe()
            assertTrue(eventReceived)
        } finally {
            KSM.shutdown()
            root.deleteRecursively()
        }
    }
}
