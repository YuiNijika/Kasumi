package dev.kasumi.app.bridge

import android.system.ErrnoException
import android.system.Os
import android.system.OsConstants
import java.io.File
import java.util.concurrent.atomic.AtomicLong

enum class KSMFileKind {
    FILE,
    DIRECTORY
}

data class KSMFileMetadata(
    val kind: KSMFileKind,
    val size: Long,
    val modifiedAt: Long?
)

data class KSMDirectoryEntry(
    val name: String,
    val path: String,
    val kind: KSMFileKind,
    val size: Long,
    val modifiedAt: Long?
)

class KSMFileSystem(
    private val dataRoot: File,
    private val cacheRoot: File,
    private val tempRoot: File
) {
    private val nextTempId = AtomicLong(1)

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
        val file = resolveWritableFile(path) ?: return false
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

    fun readDir(path: String): List<KSMDirectoryEntry>? {
        val directory = resolveDirectory(path) ?: return null
        if (
            !directory.isDirectory ||
            isSymbolicLink(directory)
        ) {
            return null
        }
        val parent = KSMVirtualPath.normalizeDirectory(path)
        return directory.listFiles()
            ?.mapNotNull { child ->
                val metadata = readMetadata(child) ?: return@mapNotNull null
                KSMDirectoryEntry(
                    child.name,
                    "$parent/${child.name}",
                    metadata.kind,
                    metadata.size,
                    metadata.modifiedAt
                )
            }
            ?.sortedBy { it.name }
    }

    fun makeDirectory(path: String, recursive: Boolean): Boolean? {
        val directory = resolveDirectory(path) ?: return null
        if (directory.exists()) {
            return if (
                directory.isDirectory &&
                !isSymbolicLink(directory)
            ) {
                false
            } else {
                null
            }
        }
        val created = if (recursive) {
            directory.mkdirs()
        } else {
            directory.mkdir()
        }
        if (created) {
            return true
        }
        return if (
            directory.isDirectory &&
            !isSymbolicLink(directory)
        ) {
            false
        } else {
            null
        }
    }

    fun stat(path: String): KSMFileMetadata? {
        val resolved = resolveDirectory(path) ?: return null
        return readMetadata(resolved)
    }

    fun createTemporaryText(content: String): String? {
        val path = "app://tmp/read-${nextTempId.getAndIncrement()}.txt"
        return if (writeText(path, content)) path else null
    }

    fun resolveWritableFile(path: String): File? {
        val file = resolve(path) ?: return null
        if (isSymbolicLink(file)) {
            return null
        }
        val root = when {
            path.startsWith("app://data/") -> dataRoot
            path.startsWith("app://cache/") -> cacheRoot
            path.startsWith("app://tmp/") -> tempRoot
            else -> return null
        }
        val canonicalRoot = root.canonicalFile
        val canonicalFile = file.canonicalFile
        val rootPrefix = canonicalRoot.path + File.separator
        return file.takeIf {
            canonicalFile.path.startsWith(rootPrefix)
        }
    }

    private fun resolve(path: String): File? {
        return KSMVirtualPath.resolveWritable(path, dataRoot, cacheRoot, tempRoot)
    }

    private fun resolveDirectory(path: String): File? {
        return KSMVirtualPath.resolveWritableDirectory(
            path,
            dataRoot,
            cacheRoot,
            tempRoot
        )
    }

    private fun readMetadata(file: File): KSMFileMetadata? {
        if (
            !file.exists() ||
            isSymbolicLink(file)
        ) {
            return null
        }
        val modifiedAt = file.lastModified().takeIf { it > 0L }
        return when {
            file.isDirectory -> KSMFileMetadata(
                KSMFileKind.DIRECTORY,
                0,
                modifiedAt
            )
            file.isFile -> KSMFileMetadata(
                KSMFileKind.FILE,
                file.length(),
                modifiedAt
            )
            else -> null
        }
    }

    private fun isSymbolicLink(file: File): Boolean {
        return try {
            val status = Os.lstat(file.path)
            (status.st_mode and OsConstants.S_IFMT) == OsConstants.S_IFLNK
        } catch (_: ErrnoException) {
            false
        } catch (_: RuntimeException) {
            false
        }
    }
}
