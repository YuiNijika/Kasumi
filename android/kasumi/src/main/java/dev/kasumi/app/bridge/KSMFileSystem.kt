package dev.kasumi.app.bridge

import android.system.ErrnoException
import android.system.Os
import android.system.OsConstants
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.nio.file.Files
import java.nio.file.StandardCopyOption
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
        private const val TEMPORARY_MAX_BYTES = 64L * 1024L * 1024L
        private const val TEMPORARY_MAX_AGE_MS = 24L * 60L * 60L * 1000L
        private val nextAtomicWriteId = AtomicLong(1)
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
        if (file.parentFile?.mkdirs() == false && file.parentFile?.isDirectory != true) {
            return false
        }
        val rechecked = resolveWritableFile(path) ?: return false
        rechecked.writeText(content)
        return true
    }

    fun writeTextAtomic(path: String, content: String): Boolean {
        if (content.toByteArray(Charsets.UTF_8).size > MAX_TEXT_BYTES) {
            return false
        }
        val target = resolveWritableFile(path) ?: return false
        val parent = target.parentFile ?: return false
        if (parent.mkdirs() == false && !parent.isDirectory) {
            return false
        }
        val temporary = File(
            parent,
            "${target.name}.tmp-${nextAtomicWriteId.getAndIncrement()}"
        )
        return try {
            FileOutputStream(temporary).use { output ->
                output.write(content.toByteArray(Charsets.UTF_8))
                output.flush()
                output.fd.sync()
            }
            replaceAtomically(temporary, target)
        } catch (_: Exception) {
            temporary.delete()
            false
        }
    }

    private fun replaceAtomically(source: File, target: File): Boolean {
        try {
            Os.rename(source.path, target.path)
            return true
        } catch (_: Exception) {
            // Local JVM tests do not provide the Android native filesystem bridge.
        }
        return try {
            Files.move(
                source.toPath(),
                target.toPath(),
                StandardCopyOption.REPLACE_EXISTING,
                StandardCopyOption.ATOMIC_MOVE
            )
            true
        } catch (_: Exception) {
            source.renameTo(target)
        }
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
        pruneTemporaryText(content.toByteArray(Charsets.UTF_8).size.toLong())
        val path = "app://tmp/read-${nextTempId.getAndIncrement()}.txt"
        return if (writeText(path, content)) path else null
    }

    fun resolveWritableFile(path: String): File? {
        val file = resolve(path) ?: return null
        val root = writableRoot(path) ?: return null
        if (isSymbolicLink(file) || escapesRoot(root, file)) {
            return null
        }
        return file
    }

    private fun resolve(path: String): File? {
        val file = KSMVirtualPath.resolveWritable(
            path,
            dataRoot,
            cacheRoot,
            tempRoot
        ) ?: return null
        val root = writableRoot(path) ?: return null
        if (isSymbolicLink(file) || escapesRoot(root, file)) {
            return null
        }
        return file
    }

    private fun resolveDirectory(path: String): File? {
        val directory = KSMVirtualPath.resolveWritableDirectory(
            path,
            dataRoot,
            cacheRoot,
            tempRoot
        ) ?: return null
        val root = writableDirectoryRoot(path) ?: return null
        if (isSymbolicLink(directory) || escapesRoot(root, directory)) {
            return null
        }
        return directory
    }

    private fun writableRoot(path: String): File? {
        return when {
            path.startsWith("app://data/") -> dataRoot
            path.startsWith("app://cache/") -> cacheRoot
            path.startsWith("app://tmp/") -> tempRoot
            else -> null
        }
    }

    private fun writableDirectoryRoot(path: String): File? {
        val normalized = KSMVirtualPath.normalizeDirectory(path)
        return when {
            normalized == "app://data" ||
                normalized.startsWith("app://data/") -> dataRoot
            normalized == "app://cache" ||
                normalized.startsWith("app://cache/") -> cacheRoot
            normalized == "app://tmp" ||
                normalized.startsWith("app://tmp/") -> tempRoot
            else -> null
        }
    }

    private fun escapesRoot(root: File, target: File): Boolean {
        return try {
            val canonicalRoot = root.canonicalFile
            val canonicalTarget = target.canonicalFile
            canonicalTarget.path != canonicalRoot.path &&
                !canonicalTarget.path.startsWith(
                    canonicalRoot.path + File.separator
                )
        } catch (_: IOException) {
            true
        }
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
        if (Files.isSymbolicLink(file.toPath())) {
            return true
        }
        return try {
            val status = Os.lstat(file.path)
            (status.st_mode and OsConstants.S_IFMT) == OsConstants.S_IFLNK
        } catch (_: ErrnoException) {
            false
        } catch (_: RuntimeException) {
            false
        } catch (_: UnsatisfiedLinkError) {
            false
        }
    }

    private fun pruneTemporaryText(incomingBytes: Long) {
        val cutoff = System.currentTimeMillis() - TEMPORARY_MAX_AGE_MS
        val files = tempRoot.listFiles()
            ?.filter { file ->
                file.isFile &&
                    !isSymbolicLink(file) &&
                    file.name.startsWith("read-") &&
                    file.name.endsWith(".txt")
            }
            ?.sortedBy { file -> file.lastModified() }
            ?.toMutableList()
            ?: return
        var total = 0L
        files.removeAll { file ->
            val expired = file.lastModified() < cutoff
            if (expired) {
                file.delete()
            } else if (file.exists()) {
                total += file.length()
            }
            expired
        }
        for (file in files) {
            if (total + incomingBytes <= TEMPORARY_MAX_BYTES) {
                break
            }
            val size = file.length()
            if (file.delete()) {
                total -= size
            }
        }
    }
}
