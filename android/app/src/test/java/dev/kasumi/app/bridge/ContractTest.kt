package dev.kasumi.app.bridge

import dev.kasumi.app.generated.KSMGeneratedContract
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.io.IOException
import java.nio.file.Files
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean

class ContractTest {
    private class TestHostServices(
        private val version: String = "9.9.9",
        private val languageTag: String = "zh-CN"
    ) : KSMHostServices {
        var clipboardText = "clipboard"
        var pickedText = "picked"
        var downloadedPath = ""
        var downloadRequestId = ""
        var loggedLevel = ""
        var loggedTarget = ""
        var loggedMessage = ""

        override fun appVersion(): String {
            return version
        }

        override fun language(): String {
            return languageTag
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

        override fun download(
            url: String,
            path: String,
            overwrite: Boolean,
            requestId: String?
        ): KSMHttpDownloadResult? {
            if (url != "https://example.com/file.bin") {
                return null
            }
            downloadedPath = path
            downloadRequestId = requestId ?: ""
            return KSMHttpDownloadResult(path, 42)
        }

        override fun isExternalUrlAllowed(url: String): Boolean {
            return url.startsWith("https://example.com/")
        }

        override fun openExternal(url: String): Boolean {
            return isExternalUrlAllowed(url)
        }

        override fun deviceInfo(): KSMDeviceInfo {
            return KSMDeviceInfo(
                platform = "android",
                osVersion = "14",
                model = "Pixel",
                manufacturer = "Google",
                appVersion = "9.9.9",
                language = "zh-CN",
                screenWidth = 1080,
                screenHeight = 2400,
                density = 2.75f,
                deviceName = "Contract",
                timezone = "Asia/Shanghai",
                architecture = "arm64-v8a"
            )
        }

        override fun operatingSystemInfo(): KSMOperatingSystemInfo {
            return KSMOperatingSystemInfo(
                platform = "android",
                architecture = "arm64-v8a",
                version = "14",
                locale = "zh-CN",
                timezone = "Asia/Shanghai"
            )
        }

        override fun writeLog(
            level: String,
            target: String,
            message: String
        ): Boolean {
            loggedLevel = level
            loggedTarget = target
            loggedMessage = message
            return true
        }

        override fun closeWindow(): Boolean {
            return true
        }

        override fun exitApp(): Boolean {
            return true
        }
    }

    private class EchoPlugin : KSMPlugin {
        override val id: String = "contract.echo"
        override val commands: Set<String> = setOf("contract.echo")
        override val permissions: Set<String> = setOf("contract.echo")

        override fun validateArgs(command: String, args: JSONObject): Boolean {
            return command == "contract.echo" &&
                args.opt("value") is String
        }

        override fun handle(
            command: String,
            args: JSONObject,
            context: KSMPluginContext
        ): Any {
            if (
                command != "contract.echo" ||
                !context.hasPermission("contract.echo")
            ) {
                throw KSMHostException(
                    "PERMISSION_DENIED",
                    "The custom plugin was not authorized."
                )
            }
            return JSONObject().put("value", args.getString("value"))
        }
    }

    private class ReplacementFileSystemPlugin : KSMPlugin {
        override val id: String = "fs"
        override val commands: Set<String> = setOf(
            "fs.readText",
            "fs.writeText",
            "fs.exists",
            "fs.remove",
            "fs.readDir",
            "fs.mkdir",
            "fs.stat"
        )
        override val permissions: Set<String> = setOf(
            "fs.read",
            "fs.write",
            "fs.remove"
        )

        override fun handle(
            command: String,
            args: JSONObject,
            context: KSMPluginContext
        ): Any {
            if (command == "fs.exists") {
                return JSONObject().put("exists", true)
            }
            throw KSMHostException(
                "NOT_SUPPORTED",
                "The replacement filesystem only supports fs.exists."
            )
        }
    }

    private class ReplacementClipboardPlugin : KSMPlugin {
        override val id: String = "clipboard"
        override val commands: Set<String> = setOf(
            "clipboard.readText",
            "clipboard.writeText"
        )
        override val permissions: Set<String> = setOf(
            "clipboard.read",
            "clipboard.write"
        )

        override fun handle(
            command: String,
            args: JSONObject,
            context: KSMPluginContext
        ): Any {
            return if (command == "clipboard.readText") {
                JSONObject().put("text", "replacement")
            } else {
                JSONObject().put("written", true)
            }
        }
    }

    private class GeneratedCommandHijackPlugin : KSMPlugin {
        override val id: String = "contract.hijack"
        override val commands: Set<String> = setOf("fs.exists")

        override fun handle(
            command: String,
            args: JSONObject,
            context: KSMPluginContext
        ): Any {
            return JSONObject().put("exists", false)
        }
    }

    private class LimitSqlConnection : KSMSqlConnection {
        override fun query(
            statement: String,
            params: List<Any?>
        ): KSMSqlQueryResult {
            return when (statement) {
                "rows" -> KSMSqlQueryResult(
                    listOf("value"),
                    List(10_001) { listOf<Any?>(1L) }
                )
                "bytes" -> KSMSqlQueryResult(
                    listOf("value"),
                    listOf(listOf<Any?>("x".repeat(16 * 1024 * 1024 + 1)))
                )
                else -> KSMSqlQueryResult(
                    listOf("value"),
                    listOf(listOf<Any?>(1L))
                )
            }
        }

        override fun execute(
            statement: String,
            params: List<Any?>
        ): KSMSqlExecuteResult {
            return KSMSqlExecuteResult(0, null)
        }

        override fun close() {
        }
    }

    private class LimitSqlDriver : KSMSqlDriver {
        override val name: String = "limits"

        override fun open(url: String): KSMSqlConnection {
            return LimitSqlConnection()
        }
    }

    private class BlockingSqlConnection : KSMSqlConnection {
        val started = CountDownLatch(1)
        private val cancelled = AtomicBoolean(false)

        override fun query(
            statement: String,
            params: List<Any?>
        ): KSMSqlQueryResult {
            return query(statement, params, null)
        }

        override fun query(
            statement: String,
            params: List<Any?>,
            requestId: String?
        ): KSMSqlQueryResult {
            started.countDown()
            while (!cancelled.get()) {
                Thread.sleep(5)
            }
            throw KSMHostException(
                "CANCELLED",
                "The SQL request was cancelled."
            )
        }

        override fun execute(
            statement: String,
            params: List<Any?>
        ): KSMSqlExecuteResult {
            return execute(statement, params, null)
        }

        override fun execute(
            statement: String,
            params: List<Any?>,
            requestId: String?
        ): KSMSqlExecuteResult {
            query(statement, params, requestId)
            return KSMSqlExecuteResult(0, null)
        }

        override fun cancel(requestId: String) {
            cancelled.set(true)
        }

        override fun close() {
            cancelled.set(true)
        }
    }

    private class BlockingSqlDriver(
        private val connection: BlockingSqlConnection
    ) : KSMSqlDriver {
        override val name: String = "blocking"

        override fun open(url: String): KSMSqlConnection {
            return connection
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
        val router = KSMRouter(
            fileSystem,
            hostServices,
            allowedCommands = KSMCommandRegistry.supportedCommands,
            allowedPermissions = KSMGeneratedContract.permissions
        )
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
            val capabilityCommands = capabilities.getJSONObject("data")
                .getJSONArray("commands")
                .let { values ->
                    List(values.length()) { index ->
                        values.getString(index)
                    }
                }
            assertFalse(capabilityCommands.contains("window.setSize"))

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

            val download = invoke(
                router,
                "http.download",
                JSONObject()
                    .put("url", "https://example.com/file.bin")
                    .put("path", "app://data/file.bin")
            )
            assertTrue(download.getBoolean("ok"))
            assertEquals(
                "app://data/file.bin",
                download.getJSONObject("data").getString("path")
            )
            assertEquals(42L, download.getJSONObject("data").getLong("bytes"))
            assertEquals("app://data/file.bin", hostServices.downloadedPath)
            assertEquals("contract", hostServices.downloadRequestId)
            assertError(
                invoke(
                    router,
                    "http.download",
                    JSONObject()
                        .put("url", "http://example.com/file.bin")
                        .put("path", "app://data/file.bin")
                ),
                "INVALID_ARGS"
            )
            assertError(
                invoke(
                    router,
                    "http.download",
                    JSONObject()
                        .put("url", "https://example.com/file.bin")
                        .put("path", "app://picked/file.bin")
                ),
                "PERMISSION_DENIED"
            )
            assertError(
                invoke(
                    router,
                    "http.download",
                    JSONObject()
                        .put("url", "https://example.com/file.bin")
                        .put("path", "app://data/file.bin")
                        .put("overwrite", "yes")
                ),
                "INVALID_ARGS"
            )

            val external = invoke(
                router,
                "shell.openExternal",
                JSONObject().put("url", "https://example.com/docs")
            )
            assertTrue(external.getBoolean("ok"))
            assertTrue(external.getJSONObject("data").getBoolean("opened"))
            assertError(
                invoke(
                    router,
                    "shell.openExternal",
                    JSONObject().put("url", "https://other.example/docs")
                ),
                "PERMISSION_DENIED"
            )

            val device = invoke(router, "device.getInfo", JSONObject())
            assertTrue(device.getBoolean("ok"))
            assertEquals(
                "android",
                device.getJSONObject("data").getString("platform")
            )
            assertEquals(
                "arm64-v8a",
                device.getJSONObject("data").getString("architecture")
            )

            val pathDirectories = invoke(
                router,
                "path.dirs",
                JSONObject()
            )
            assertTrue(pathDirectories.getBoolean("ok"))
            assertEquals(
                "app://data",
                pathDirectories.getJSONObject("data").getString("data")
            )
            val joinedPath = invoke(
                router,
                "path.join",
                JSONObject()
                    .put("base", "app://data")
                    .put("segments", org.json.JSONArray().put("notes").put("entry.json"))
            )
            assertTrue(joinedPath.getBoolean("ok"))
            assertEquals(
                "app://data/notes/entry.json",
                joinedPath.getJSONObject("data").getString("path")
            )
            assertError(
                invoke(
                    router,
                    "path.normalize",
                    JSONObject().put("path", "app://data/notes/../entry.json")
                ),
                "INVALID_ARGS"
            )
            val dirname = invoke(
                router,
                "path.dirname",
                JSONObject().put("path", "app://data/notes/entry.json")
            )
            assertEquals(
                "app://data/notes",
                dirname.getJSONObject("data").getString("path")
            )
            val basename = invoke(
                router,
                "path.basename",
                JSONObject()
                    .put("path", "app://data/notes/entry.json")
                    .put("suffix", ".json")
            )
            assertEquals(
                "entry",
                basename.getJSONObject("data").getString("name")
            )
            val extension = invoke(
                router,
                "path.extname",
                JSONObject().put("path", "app://data/notes/entry.json")
            )
            assertEquals(
                ".json",
                extension.getJSONObject("data").getString("extension")
            )
            assertTrue(
                invoke(
                    router,
                    "path.isAbsolute",
                    JSONObject().put("path", "app://data")
                ).getJSONObject("data").getBoolean("absolute")
            )

            val profile = JSONObject()
                .put("theme", "light")
                .put("notifications", true)
            assertTrue(
                invoke(
                    router,
                    "store.set",
                    JSONObject()
                        .put("key", "profile")
                        .put("value", profile)
                ).getBoolean("ok")
            )
            val stored = invoke(
                router,
                "store.get",
                JSONObject().put("key", "profile")
            )
            assertTrue(stored.getJSONObject("data").getBoolean("found"))
            assertEquals(
                "light",
                stored.getJSONObject("data")
                    .getJSONObject("value")
                    .getString("theme")
            )
            val storedKeys = invoke(
                router,
                "store.keys",
                JSONObject()
            )
            assertEquals(
                "profile",
                storedKeys.getJSONObject("data")
                    .getJSONArray("keys")
                    .getString(0)
            )
            assertTrue(
                invoke(
                    router,
                    "store.delete",
                    JSONObject().put("key", "profile")
                ).getJSONObject("data").getBoolean("deleted")
            )
            assertTrue(
                fileSystem.writeText(
                    "app://data/.kasumi/store.json",
                    "{"
                )
            )
            assertError(
                invoke(
                    router,
                    "store.get",
                    JSONObject().put("key", "profile")
                ),
                "IO_ERROR"
            )
            assertTrue(
                invoke(router, "store.clear", JSONObject()).getBoolean("ok")
            )

            val operatingSystem = invoke(
                router,
                "os.getInfo",
                JSONObject()
            )
            assertTrue(operatingSystem.getBoolean("ok"))
            assertEquals(
                "android",
                operatingSystem.getJSONObject("data").getString("platform")
            )
            assertEquals(
                "Asia/Shanghai",
                operatingSystem.getJSONObject("data").getString("timezone")
            )
            assertTrue(
                invoke(
                    router,
                    "log.write",
                    JSONObject()
                        .put("level", "info")
                        .put("message", "contract")
                        .put("target", "kasumi")
                ).getBoolean("ok")
            )
            assertEquals("info", hostServices.loggedLevel)
            assertEquals("kasumi", hostServices.loggedTarget)
            assertEquals("contract", hostServices.loggedMessage)

            assertError(
                invoke(
                    router,
                    "window.setSize",
                    JSONObject().put("width", 800).put("height", 600)
                ),
                "NOT_SUPPORTED"
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
                KSM.DEFAULT_WINDOW_ID,
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
            assertEquals(
                "app://data/notes/entry.json",
                KSM.path.join(
                    "app://data",
                    listOf("notes", "entry.json")
                )
            )
            assertEquals(
                "app://data/notes",
                KSM.path.dirname("app://data/notes/entry.json")
            )
            assertEquals(
                "entry",
                KSM.path.basename("app://data/notes/entry.json", ".json")
            )
            assertEquals(
                ".json",
                KSM.path.extname("app://data/notes/entry.json")
            )
            assertTrue(KSM.path.isAbsolute("app://data") == true)
            assertTrue(
                KSM.store.set(
                    "profile",
                    JSONObject().put("theme", "light")
                )
            )
            assertEquals(
                "light",
                (KSM.store.get("profile")?.value as JSONObject)
                    .getString("theme")
            )
            assertTrue(KSM.store.keys()?.contains("profile") == true)
            assertTrue(KSM.store.delete("profile") == true)
            assertEquals("android", KSM.os.getInfo()?.platform)
            assertTrue(
                KSM.log.write(
                    level = "info",
                    message = "native",
                    target = "kasumi"
                )
            )
            assertTrue(KSM.get().commands.isNotEmpty())
            assertTrue(KSM.dialog.message("Kasumi", "root"))
            assertEquals("clipboard", KSM.clipboard.readText())
            assertTrue(KSM.clipboard.writeText("updated"))
            assertEquals("updated", KSM.clipboard.readText())
            assertTrue(KSM.notification.show("Kasumi", "root"))
            assertEquals(
                42L,
                KSM.http.download(
                    "https://example.com/file.bin",
                    "app://data/root-download.bin"
                )?.bytes
            )
            assertTrue(
                KSM.shell.openExternal("https://example.com/docs")
            )
            assertEquals("android", KSM.device.getInfo()?.platform)
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

    @Test
    fun windowRegistryRoutesRequestsAndEvents() {
        val leftRoot = Files.createTempDirectory("kasumi-left-window").toFile()
        val rightRoot = Files.createTempDirectory("kasumi-right-window").toFile()
        val leftFileSystem = KSMFileSystem(
            File(leftRoot, "data"),
            File(leftRoot, "cache"),
            File(leftRoot, "tmp")
        )
        val rightFileSystem = KSMFileSystem(
            File(rightRoot, "data"),
            File(rightRoot, "cache"),
            File(rightRoot, "tmp")
        )
        val leftRouter = KSMRouter(
            leftFileSystem,
            TestHostServices("1.0.0", "en-US"),
            allowedCommands = listOf(
                "app.getInfo",
                "event.subscribe",
                "event.unsubscribe"
            )
        )
        val rightRouter = KSMRouter(
            rightFileSystem,
            TestHostServices("2.0.0", "ja-JP"),
            allowedCommands = listOf(
                "app.getInfo",
                "event.subscribe",
                "event.unsubscribe"
            )
        )
        var leftEvent = false
        var rightEvent = false
        var unsubscribeLeft: () -> Unit = {}
        var unsubscribeRight: () -> Unit = {}

        try {
            KSM.registerWindow(
                "left",
                leftRouter,
                leftFileSystem,
                TestHostServices("1.0.0", "en-US")
            )
            KSM.registerWindow(
                "right",
                rightRouter,
                rightFileSystem,
                TestHostServices("2.0.0", "ja-JP")
            )

            val leftInfo = KSM.invoke(
                "left",
                "app.getInfo",
                JSONObject()
            )
            val rightInfo = KSM.invoke(
                "right",
                "app.getInfo",
                JSONObject()
            )
            assertEquals(
                "1.0.0",
                leftInfo.getJSONObject("data").getString("version")
            )
            assertEquals(
                "2.0.0",
                rightInfo.getJSONObject("data").getString("version")
            )
            assertThrows(IllegalStateException::class.java) {
                KSM.app.getInfo()
            }
            assertThrows(IllegalStateException::class.java) {
                KSM.get()
            }

            unsubscribeLeft = KSM.on("left", "window.resized") {
                leftEvent = true
            }
            unsubscribeRight = KSM.on("right", "window.resized") {
                rightEvent = true
            }
            KSM.emitEvent(
                "left",
                "window.resized",
                JSONObject().put("width", 800).put("height", 600)
            )
            assertTrue(leftEvent)
            assertFalse(rightEvent)

            KSM.unregisterWindow("left")
            assertEquals(
                "2.0.0",
                KSM.app.getInfo().version
            )
            assertTrue(KSM.get().commands.contains("app.getInfo"))
            assertThrows(IllegalStateException::class.java) {
                KSM.invoke("left", "ping", JSONObject())
            }
        } finally {
            unsubscribeLeft()
            unsubscribeRight()
            KSM.unregisterWindow("left")
            KSM.unregisterWindow("right")
            leftRoot.deleteRecursively()
            rightRoot.deleteRecursively()
        }
    }

    @Test
    fun sqlResourceLimits() {
        val registry = KSMSqlDriverRegistry()
        registry.register(LimitSqlDriver())
        val connectionId = registry.open("limits", ":memory:")
        assertTrue(connectionId != null)
        val connection = requireNotNull(connectionId)

        val rowLimit = assertThrows(KSMHostException::class.java) {
            registry.query(connection, "rows", emptyList())
        }
        assertEquals("RESOURCE_LIMIT", rowLimit.code)
        val byteLimit = assertThrows(KSMHostException::class.java) {
            registry.query(connection, "bytes", emptyList())
        }
        assertEquals("RESOURCE_LIMIT", byteLimit.code)

        val connections = KSMSqlDriverRegistry()
        connections.register(LimitSqlDriver())
        repeat(32) {
            assertTrue(connections.open("limits", ":memory:") != null)
        }
        val connectionLimit = assertThrows(KSMHostException::class.java) {
            connections.open("limits", ":memory:")
        }
        assertEquals("RESOURCE_LIMIT", connectionLimit.code)
        connections.closeAll()
        registry.closeAll()
    }

    @Test
    fun sqlCancellationAndDuplicateRequestIds() {
        val connection = BlockingSqlConnection()
        val registry = KSMSqlDriverRegistry()
        registry.register(BlockingSqlDriver(connection))
        val connectionId = requireNotNull(
            registry.open("blocking", ":memory:")
        )
        var failure: Throwable? = null
        val query = Thread {
            try {
                registry.query(
                    connectionId,
                    "wait",
                    emptyList(),
                    "sql-cancel"
                )
            } catch (error: Throwable) {
                failure = error
            }
        }

        try {
            query.start()
            assertTrue(connection.started.await(1, TimeUnit.SECONDS))
            val duplicate = assertThrows(KSMHostException::class.java) {
                registry.query(
                    connectionId,
                    "wait",
                    emptyList(),
                    "sql-cancel"
                )
            }
            assertEquals("BUSY", duplicate.code)
            registry.cancel("sql-cancel")
            query.join(TimeUnit.SECONDS.toMillis(2))
            assertFalse(query.isAlive)
            val error = failure as? KSMHostException
            assertEquals("CANCELLED", error?.code)
        } finally {
            registry.closeAll()
        }
    }

    @Test
    fun storeWritesAreSerializedAcrossPluginInstances() {
        val root = Files.createTempDirectory("kasumi-store-concurrency").toFile()
        val fileSystem = KSMFileSystem(
            File(root, "data"),
            File(root, "cache"),
            File(root, "tmp")
        )
        val context = KSMPluginContext(
            fileSystem = fileSystem,
            hostServices = null,
            hasPermission = { it == "store" }
        )
        val leftPlugin = KSMStorePlugin()
        val rightPlugin = KSMStorePlugin()
        val start = CountDownLatch(1)
        var leftFailure: Throwable? = null
        var rightFailure: Throwable? = null
        val leftThread = Thread {
            try {
                start.await()
                repeat(20) { index ->
                    leftPlugin.handle(
                        "store.set",
                        JSONObject()
                            .put("key", "left-$index")
                            .put("value", index),
                        context
                    )
                }
            } catch (error: Throwable) {
                leftFailure = error
            }
        }
        val rightThread = Thread {
            try {
                start.await()
                repeat(20) { index ->
                    rightPlugin.handle(
                        "store.set",
                        JSONObject()
                            .put("key", "right-$index")
                            .put("value", index),
                        context
                    )
                }
            } catch (error: Throwable) {
                rightFailure = error
            }
        }

        try {
            leftThread.start()
            rightThread.start()
            start.countDown()
            leftThread.join(TimeUnit.SECONDS.toMillis(5))
            rightThread.join(TimeUnit.SECONDS.toMillis(5))
            assertFalse(leftThread.isAlive)
            assertFalse(rightThread.isAlive)
            assertTrue(leftFailure?.stackTraceToString(), leftFailure == null)
            assertTrue(rightFailure?.stackTraceToString(), rightFailure == null)
            val keys = leftPlugin.handle(
                "store.keys",
                JSONObject(),
                context
            ).let { result ->
                (result as JSONObject).getJSONArray("keys")
            }
            assertEquals(40, keys.length())
        } finally {
            root.deleteRecursively()
        }
    }

    @Test
    fun filesystemRejectsSymlinksOutsideTheSandbox() {
        val root = Files.createTempDirectory("kasumi-symlink").toFile()
        val data = File(root, "data").apply { mkdirs() }
        val fileSystem = KSMFileSystem(
            data,
            File(root, "cache"),
            File(root, "tmp")
        )
        val outside = File(root, "outside.txt").apply {
            writeText("secret")
        }
        val link = File(data, "contract-link.txt").toPath()
        try {
            Files.createSymbolicLink(link, outside.toPath())
        } catch (_: IOException) {
            root.deleteRecursively()
            return
        } catch (_: UnsupportedOperationException) {
            root.deleteRecursively()
            return
        }
        try {
            assertFalse(fileSystem.exists("app://data/contract-link.txt"))
            assertEquals(null, fileSystem.readText("app://data/contract-link.txt"))
            assertFalse(
                fileSystem.writeText(
                    "app://data/contract-link.txt",
                    "blocked"
                )
            )
        } finally {
            root.deleteRecursively()
        }
    }

    @Test
    fun pluginRules() {
        val root = Files.createTempDirectory("kasumi-plugin-rules").toFile()
        val fileSystem = KSMFileSystem(
            File(root, "data"),
            File(root, "cache"),
            File(root, "tmp")
        )
        try {
            val replacement = KSMRouter(
                fileSystem,
                null,
                allowedCommands = KSMCommandRegistry.supportedCommands,
                allowedPermissions = KSMGeneratedContract.permissions,
                plugins = listOf(ReplacementFileSystemPlugin())
            )
            val exists = invoke(
                replacement,
                "fs.exists",
                JSONObject().put("path", "app://data/plugin.txt")
            )
            assertTrue(exists.getBoolean("ok"))
            assertTrue(exists.getJSONObject("data").getBoolean("exists"))

            val clipboard = KSMRouter(
                fileSystem,
                null,
                allowedCommands = KSMCommandRegistry.supportedCommands,
                allowedPermissions = KSMGeneratedContract.permissions,
                plugins = listOf(ReplacementClipboardPlugin())
            )
            val clipboardText = invoke(
                clipboard,
                "clipboard.readText",
                JSONObject()
            )
            assertTrue(clipboardText.getBoolean("ok"))
            assertEquals(
                "replacement",
                clipboardText.getJSONObject("data").getString("text")
            )

            assertThrows(IllegalArgumentException::class.java) {
                KSMRouter(
                    fileSystem,
                    null,
                    allowedCommands = KSMCommandRegistry.supportedCommands,
                    allowedPermissions = KSMGeneratedContract.permissions,
                    plugins = listOf(GeneratedCommandHijackPlugin())
                )
            }
        } finally {
            root.deleteRecursively()
        }
    }

    @Test
    fun customPlugin() {
        val root = Files.createTempDirectory("kasumi-plugin").toFile()
        val fileSystem = KSMFileSystem(
            File(root, "data"),
            File(root, "cache"),
            File(root, "tmp")
        )
        val router = KSMRouter(
            fileSystem,
            null,
            allowedCommands = KSMCommandRegistry.supportedCommands + "contract.echo",
            allowedPermissions = KSMGeneratedContract.permissions + "contract.echo",
            plugins = listOf(EchoPlugin())
        )
        try {
            val echoed = invoke(
                router,
                "contract.echo",
                JSONObject().put("value", "plugin")
            )
            assertTrue(echoed.getBoolean("ok"))
            assertEquals(
                "plugin",
                echoed.getJSONObject("data").getString("value")
            )

            val capabilities = invoke(
                router,
                "capabilities.query",
                JSONObject()
            )
            val plugins = capabilities.getJSONObject("data")
                .getJSONArray("plugins")
                .let { values ->
                    List(values.length()) { index ->
                        values.getString(index)
                    }
                }
            assertTrue(plugins.contains("contract.echo"))
        } finally {
            root.deleteRecursively()
        }
    }

    @Test
    fun commandAllowlist() {
        val root = Files.createTempDirectory("kasumi-allowlist").toFile()
        val fileSystem = KSMFileSystem(
            File(root, "data"),
            File(root, "cache"),
            File(root, "tmp")
        )
        val allowed = listOf("ping", "capabilities.query")
        val router = KSMRouter(
            fileSystem,
            TestHostServices(),
            allowedCommands = allowed
        )
        try {
            assertEquals(allowed, router.supportedCommands)

            val capabilities = invoke(
                router,
                "capabilities.query",
                JSONObject()
            )
            assertTrue(capabilities.getBoolean("ok"))
            assertEquals(
                allowed,
                capabilities.getJSONObject("data")
                    .getJSONArray("commands")
                    .let { commands ->
                        List(commands.length()) { index ->
                            commands.getString(index)
                        }
                    }
            )
            assertError(
                invoke(
                    router,
                    "fs.writeText",
                    JSONObject()
                        .put("path", "app://data/blocked.txt")
                        .put("content", "blocked")
                ),
                "NOT_SUPPORTED"
            )

            KSM.init(router, fileSystem, TestHostServices())
            assertEquals(allowed, KSM.get().commands)
            assertFalse(KSM.fs.writeText("app://data/blocked.txt", "blocked"))
        } finally {
            KSM.shutdown()
            root.deleteRecursively()
        }
    }

    @Test
    fun capabilityPermissionFilter() {
        val root = Files.createTempDirectory("kasumi-capability-permission").toFile()
        val fileSystem = KSMFileSystem(
            File(root, "data"),
            File(root, "cache"),
            File(root, "tmp")
        )
        try {
            val denied = KSMRouter(
                fileSystem,
                null,
                allowedCommands = listOf(
                    "capabilities.query",
                    "fs.readText"
                )
            )
            val deniedCapabilities = invoke(
                denied,
                "capabilities.query",
                JSONObject()
            )
            assertTrue(deniedCapabilities.getBoolean("ok"))
            val deniedCommands = deniedCapabilities.getJSONObject("data")
                .getJSONArray("commands")
                .let { values ->
                    List(values.length()) { index ->
                        values.getString(index)
                    }
                }
            assertFalse(deniedCommands.contains("fs.readText"))
            assertError(
                invoke(
                    denied,
                    "fs.readText",
                    JSONObject().put("path", "app://data/contract.txt")
                ),
                "PERMISSION_DENIED"
            )

            val granted = KSMRouter(
                fileSystem,
                null,
                allowedCommands = listOf(
                    "capabilities.query",
                    "fs.readText"
                ),
                allowedPermissions = listOf("fs.read")
            )
            val grantedCapabilities = invoke(
                granted,
                "capabilities.query",
                JSONObject()
            )
            assertTrue(grantedCapabilities.getBoolean("ok"))
            val grantedCommands = grantedCapabilities.getJSONObject("data")
                .getJSONArray("commands")
                .let { values ->
                    List(values.length()) { index ->
                        values.getString(index)
                    }
                }
            assertTrue(grantedCommands.contains("fs.readText"))
        } finally {
            root.deleteRecursively()
        }
    }
}
