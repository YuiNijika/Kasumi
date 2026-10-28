package dev.kasumi.app.bridge

import java.io.File

class KSMFileSystem(
    private val dataRoot: File,
    private val cacheRoot: File,
    private val tempRoot: File
) {
    companion object {
        const val MAX_TEXT_BYTES = 16 * 1024 * 1024
    }

    fun readText(path: String): String? {
        val file = resolve(path) ?: return null
        return if (file.isFile && file.length() <= MAX_TEXT_BYTES) {
            file.readText()
        } else {
            null
        }
    }

    fun writeText(path: String, content: String): Boolean {
        if (content.toByteArray(Charsets.UTF_8).size > MAX_TEXT_BYTES) {
            return false
        }
        val file = resolve(path) ?: return false
        file.parentFile?.mkdirs()
        file.writeText(content)
        return true
    }

    fun exists(path: String): Boolean {
        return resolve(path)?.exists() == true
    }

    fun remove(path: String): Boolean {
        return resolve(path)?.delete() == true
    }

    private fun resolve(path: String): File? {
        return KSMVirtualPath.resolveWritable(path, dataRoot, cacheRoot, tempRoot)
    }
}
