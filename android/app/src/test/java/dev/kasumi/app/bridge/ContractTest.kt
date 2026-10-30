package dev.kasumi.app.bridge

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.nio.file.Files

class ContractTest {
    private class TestHostServices : KSMHostServices {
        var clipboardText = "clipboard"
        var pickedText = "picked"

        override fun appVersion(): String {
            return "9.9.9"
        }

        override fun language(): String {
            return "zh-CN"
        }

        override fun showMessage(title: String, message: String): Boolean {
            return title == "Kasumi" && message == "root"
        }

        override fun confirm(title: String, message: String): Boolean {
            return showMessage(title, message)
        }

        override fun openFile(filters: List<String>): String? {
            return null
        }

        override fun saveFile(suggestedName: String): String? {
            return null
        }

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

        override fun pickedStat(path: String): KSMFileMetadata? {
            return if (path == "app://picked/1") {
                KSMFileMetadata(
                    KSMFileKind.FILE,
                    pickedText.toByteArray(Charsets.UTF_8).size.toLong(),
                    null
                )
            } else {
                null
            }
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

        override fun readBundledDirectory(
            path: String
        ): List<KSMDirectoryEntry>? {
            return if (path == "app://res") {
                listOf(
                    KSMDirectoryEntry(
                        "contract.txt",
                        "app://res/contract.txt",
                        KSMFileKind.FILE,
                        7,
                        null
                    )
                )
            } else {
                null
            }
        }

        override fun bundledStat(path: String): KSMFileMetadata? {
            return when (path) {
                "app://res" -> KSMFileMetadata(
                    KSMFileKind.DIRECTORY,
                    0,
                    null
                )
                "app://res/contract.txt" -> KSMFileMetadata(
                    KSMFileKind.FILE,
                    7,
                    null
                )
                else -> null
            }
        }

        override fun readClipboardText(): String? {
            return clipboardText
        }

        override fun writeClipboardText(text: String): Boolean {
            clipboardText = text
            return true
        }

        override fun showNotification(title: String, body: String): Boolean {
            return title == "Kasumi" && body == "root"
        }

        override fun requestPermission(permission: String): Boolean {
            return permission == "clipboard.read"
        }

        override fun closeWindow(): Boolean {
            return true
        }

        override fun exitApp(): Boolean {
            return true
        }
    }

    private fun invoke(router: KSMRouter, command: String, args: JSONObject): JSONObject {
        val request = JSONObject()
            .put("type", "request")
            .put("id", "contract")
            .put("cmd", command)
            .put("args", args)
        return JSONObject(router.handleMessage(request.toString()))
    }

    private fun assertError(response: JSONObject, code: String) {
        assertFalse(response.getBoolean("ok"))
        assertEquals(code, response.getJSONObject("error").getString("code"))
    }

    @Test
    fun commandContract() {
        val root = Files.createTempDirectory("kasumi-contract").toFile()
        val fileSystem = KSMFileSystem(
            File(root, "data"),
            File(root, "cache"),
            File(root, "tmp")
        )
        val hostServices = TestHostServices()
        val router = KSMRouter(fileSystem, hostServices)
        try {
            val ping = invoke(router, "ping", JSONObject())
            assertTrue(ping.getBoolean("ok"))
            assertTrue(ping.getJSONObject("data").getBoolean("pong"))

            val capabilities = invoke(router, "capabilities.query", JSONObject())
            assertTrue(capabilities.getBoolean("ok"))
            assertEquals(
                "android",
                capabilities.getJSONObject("data").getString("platform")
            )
            assertTrue(
                capabilities.getJSONObject("data")
                    .getJSONArray("commands")
                    .length() > 0
            )

            assertError(
                invoke(router, "unknown.command", JSONObject()),
                "UNKNOWN_COMMAND"
            )
            assertError(
                invoke(
                    router,
                    "fs.writeText",
                    JSONObject().put("path", "app://data/x.txt")
                ),
                "INVALID_ARGS"
            )
            assertError(
                invoke(
                    router,
                    "fs.readText",
                    JSONObject().put("path", "app://data/../secret.txt")
                ),
                "PERMISSION_DENIED"
            )
            for (path in listOf(
                "app://data/C:/secret.txt",
                "app://data/C:secret.txt",
                "app://data/file.txt:stream",
                "app://data/\\server\\share",
                "app://data//server",
                "app://picked/1/2",
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
                invoke(
                    router,
                    "event.subscribe",
                    JSONObject().put("event", "unknown.event")
                ),
                "INVALID_ARGS"
            )
            assertError(
                invoke(
                    router,
                    "permission.request",
                    JSONObject().put("permission", "unknown.permission")
                ),
                "INVALID_ARGS"
            )
            assertError(
                invoke(
                    router,
                    "fs.readText",
                    JSONObject().put("path", "app://assets/index.html")
                ),
                "NOT_FOUND"
            )
            assertError(
                invoke(
                    router,
                    "fs.writeText",
                    JSONObject()
                        .put("path", "app://assets/index.html")
                        .put("content", "blocked")
                ),
                "PERMISSION_DENIED"
            )

            val write = invoke(
                router,
                "fs.writeText",
                JSONObject()
                    .put("path", "app://data/contract.txt")
                    .put("content", "kasumi")
            )
            assertTrue(write.getBoolean("ok"))
            val read = invoke(
                router,
                "fs.readText",
                JSONObject().put("path", "app://data/contract.txt")
            )
            assertTrue(read.getBoolean("ok"))
            assertEquals(
                "kasumi",
                read.getString("data")
            )

            val mkdir = invoke(
                router,
                "fs.mkdir",
                JSONObject()
                    .put("path", "app://data/contract-dir")
                    .put("recursive", true)
            )
            assertTrue(mkdir.getBoolean("ok"))
            assertTrue(mkdir.getJSONObject("data").getBoolean("created"))
            val mkdirAgain = invoke(
                router,
                "fs.mkdir",
                JSONObject()
                    .put("path", "app://data/contract-dir")
                    .put("recursive", true)
            )
            assertTrue(mkdirAgain.getBoolean("ok"))
            assertFalse(mkdirAgain.getJSONObject("data").getBoolean("created"))
            assertTrue(
                invoke(
                    router,
                    "fs.writeText",
                    JSONObject()
                        .put("path", "app://data/contract-dir/nested.txt")
                        .put("content", "nested")
                ).getBoolean("ok")
            )
            val directoryStat = invoke(
                router,
                "fs.stat",
                JSONObject().put("path", "app://data/contract-dir")
            )
            assertEquals(
                "directory",
                directoryStat.getJSONObject("data").getString("kind")
            )
            assertEquals(0L, directoryStat.getJSONObject("data").getLong("size"))
            val fileStat = invoke(
                router,
                "fs.stat",
                JSONObject().put("path", "app://data/contract-dir/nested.txt")
            )
            assertEquals(
                "file",
                fileStat.getJSONObject("data").getString("kind")
            )
            assertEquals(6L, fileStat.getJSONObject("data").getLong("size"))
            val listing = invoke(
                router,
                "fs.readDir",
                JSONObject().put("path", "app://data")
            )
            val listedNames = listing.getJSONObject("data")
                .getJSONArray("entries")
                .let { entries ->
                    (0 until entries.length()).map { index ->
                        entries.getJSONObject(index).getString("name")
                    }
                }
            assertTrue(listedNames.contains("contract-dir"))
            val nestedListing = invoke(
                router,
                "fs.readDir",
                JSONObject().put("path", "app://data/contract-dir")
            )
            assertEquals(
                1,
                nestedListing.getJSONObject("data")
                    .getJSONArray("entries")
                    .length()
            )
            val resourceStat = invoke(
                router,
                "fs.stat",
                JSONObject().put("path", "app://res/contract.txt")
            )
            assertEquals(
                "file",
                resourceStat.getJSONObject("data").getString("kind")
            )
            val resourceListing = invoke(
                router,
                "fs.readDir",
                JSONObject().put("path", "app://res")
            )
            assertTrue(
                resourceListing.getJSONObject("data")
                    .getJSONArray("entries")
                    .length() == 1
            )
            assertTrue(
                invoke(
                    router,
                    "fs.remove",
                    JSONObject().put(
                        "path",
                        "app://data/contract-dir/nested.txt"
                    )
                ).getBoolean("ok")
            )
            assertTrue(
                invoke(
                    router,
                    "fs.remove",
                    JSONObject().put("path", "app://data/contract-dir")
                ).getBoolean("ok")
            )

            val largeContent = "x".repeat(70 * 1024)
            assertTrue(fileSystem.writeText("app://data/large.txt", largeContent))
            val largeRead = invoke(
                router,
                "fs.readText",
                JSONObject().put("path", "app://data/large.txt")
            )
            assertTrue(largeRead.getBoolean("ok"))
            val temporary = largeRead.getJSONObject("data").getString("file")
            assertTrue(temporary.startsWith("app://tmp/"))
            assertTrue(fileSystem.exists(temporary))
            val largeAgain = invoke(
                router,
                "fs.readText",
                JSONObject().put("path", temporary)
            )
            assertTrue(largeAgain.getBoolean("ok"))
            assertEquals(
                temporary,
                largeAgain.getJSONObject("data").getString("file")
            )

            val large = invoke(
                router,
                "fs.writeText",
                JSONObject()
                    .put("path", "app://data/large.txt")
                    .put("content", largeContent)
            )
            assertError(large, "INVALID_ARGS")

            val malformed = JSONObject(router.handleMessage("{"))
            assertError(malformed, "INVALID_ARGS")

            KSM.init(router, fileSystem, hostServices)
            val rootPing = KSM.invoke(
                "window-1",
                "ping",
                JSONObject()
            )
            assertTrue(rootPing.getBoolean("ok"))
            assertTrue(
                rootPing.getJSONObject("data").getBoolean("pong")
            )
            assertTrue(KSM.fs.writeText("app://data/root.txt", "root"))
            assertEquals("root", KSM.fs.readText("app://data/root.txt"))
            assertEquals(
                "bundled",
                KSM.fs.readText("app://res/contract.txt")
            )
            assertTrue(KSM.fs.exists("app://res/contract.txt"))
            assertFalse(KSM.fs.writeText("app://res/contract.txt", "blocked"))
            assertEquals("picked", KSM.fs.readText("app://picked/1"))
            assertTrue(KSM.fs.writeText("app://picked/1", "updated"))
            assertEquals("updated", KSM.fs.readText("app://picked/1"))
            assertEquals(
                KSMFileKind.FILE,
                KSM.fs.stat("app://picked/1")?.kind
            )
            assertTrue(KSM.fs.remove("app://picked/1"))
            assertTrue(KSM.fs.makeDirectory("app://data/root-dir", true) == true)
            assertEquals(
                KSMFileKind.DIRECTORY,
                KSM.fs.stat("app://data/root-dir")?.kind
            )
            assertTrue(KSM.fs.readDir("app://data")?.isNotEmpty() == true)
            assertTrue(KSM.get().commands.isNotEmpty())
            assertTrue(KSM.dialog.message("Kasumi", "root"))
            assertEquals("clipboard", KSM.clipboard.readText())
            assertTrue(KSM.clipboard.writeText("updated"))
            assertEquals("updated", KSM.clipboard.readText())
            assertTrue(KSM.notification.show("Kasumi", "root"))
            assertTrue(KSM.permission.request("clipboard.read"))
            assertThrows(IllegalArgumentException::class.java) {
                KSM.permission.request("unknown.permission")
            }
            assertTrue(KSM.window.close())
            val appInfo = KSM.app.getInfo()
            assertEquals("Kasumi", appInfo.name)
            assertEquals("9.9.9", appInfo.version)
            assertEquals(KSMPlatform.ANDROID, appInfo.platform)
            assertEquals("zh-CN", appInfo.language)
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

            KSM.shutdown()
            assertThrows(IllegalStateException::class.java) {
                KSM.fs.readText("app://data/root.txt")
            }
            assertThrows(IllegalStateException::class.java) {
                KSM.dialog.message("Kasumi", "root")
            }
            assertThrows(IllegalStateException::class.java) {
                KSM.clipboard.readText()
            }
            assertThrows(IllegalStateException::class.java) {
                KSM.notification.show("Kasumi", "root")
            }
            assertThrows(IllegalStateException::class.java) {
                KSM.permission.request("clipboard.read")
            }
            assertThrows(IllegalStateException::class.java) {
                KSM.window.close()
            }
            assertThrows(IllegalStateException::class.java) {
                KSM.app.getInfo()
            }
        } finally {
            KSM.shutdown()
            root.deleteRecursively()
        }
    }
}
