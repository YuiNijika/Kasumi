package dev.kasumi.app.bridge

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.nio.file.Files

class ContractTest {
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
        val router = KSMRouter(fileSystem)
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
            assertError(
                invoke(
                    router,
                    "event.subscribe",
                    JSONObject().put("event", "unknown.event")
                ),
                "INVALID_ARGS"
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
                read.getJSONObject("data").getString("text")
            )

            val large = invoke(
                router,
                "fs.writeText",
                JSONObject()
                    .put("path", "app://data/large.txt")
                    .put("content", "x".repeat(70 * 1024))
            )
            assertError(large, "INVALID_ARGS")
        } finally {
            root.deleteRecursively()
        }
    }
}
