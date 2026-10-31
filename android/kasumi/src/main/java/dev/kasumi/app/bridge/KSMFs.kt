package dev.kasumi.app.bridge

object KSMFs {
    fun readText(path: String): String? {
        if (!KSM.isCommandEnabled("fs.readText")) {
            return null
        }
        if (KSMVirtualPath.isReadOnly(path)) {
            return KSM.requireHostServices().readBundledText(path)
        }
        if (KSMVirtualPath.isPicked(path)) {
            return KSM.requireHostServices().readPickedText(path)
        }
        return KSM.requireFileSystem().readText(path)
    }

    fun writeText(path: String, content: String): Boolean {
        if (!KSM.isCommandEnabled("fs.writeText")) {
            return false
        }
        if (KSMVirtualPath.isReadOnly(path)) {
            return false
        }
        if (KSMVirtualPath.isPicked(path)) {
            return KSM.requireHostServices().writePickedText(path, content)
        }
        return KSM.requireFileSystem().writeText(path, content)
    }

    fun exists(path: String): Boolean {
        if (!KSM.isCommandEnabled("fs.exists")) {
            return false
        }
        if (KSMVirtualPath.isReadOnly(path)) {
            return KSM.requireHostServices().bundledExists(path)
        }
        if (KSMVirtualPath.isPicked(path)) {
            return KSM.requireHostServices().pickedExists(path)
        }
        return KSM.requireFileSystem().exists(path)
    }

    fun remove(path: String): Boolean {
        if (!KSM.isCommandEnabled("fs.remove")) {
            return false
        }
        if (KSMVirtualPath.isReadOnly(path)) {
            return false
        }
        if (KSMVirtualPath.isPicked(path)) {
            return KSM.requireHostServices().removePicked(path)
        }
        return KSM.requireFileSystem().remove(path)
    }

    fun readDir(path: String): List<KSMDirectoryEntry>? {
        if (!KSM.isCommandEnabled("fs.readDir")) {
            return null
        }
        if (KSMVirtualPath.isReadOnly(path)) {
            return KSM.requireHostServices().readBundledDirectory(path)
        }
        if (KSMVirtualPath.isPicked(path)) {
            return null
        }
        return KSM.requireFileSystem().readDir(path)
    }

    fun makeDirectory(path: String, recursive: Boolean): Boolean? {
        if (!KSM.isCommandEnabled("fs.mkdir")) {
            return null
        }
        if (
            KSMVirtualPath.isReadOnly(path) ||
            KSMVirtualPath.isPicked(path)
        ) {
            return null
        }
        return KSM.requireFileSystem().makeDirectory(path, recursive)
    }

    fun stat(path: String): KSMFileMetadata? {
        if (!KSM.isCommandEnabled("fs.stat")) {
            return null
        }
        if (KSMVirtualPath.isPicked(path)) {
            return KSM.requireHostServices().pickedStat(path)
        }
        if (KSMVirtualPath.isReadOnly(path)) {
            return KSM.requireHostServices().bundledStat(path)
        }
        return KSM.requireFileSystem().stat(path)
    }
}
