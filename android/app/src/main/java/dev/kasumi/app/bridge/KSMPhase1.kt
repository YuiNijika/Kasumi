package dev.kasumi.app.bridge

enum class KSMPlatform {
    ANDROID
}

data class KSMCapabilities(
    val platform: KSMPlatform,
    val commands: List<String>,
    val permissions: List<String>
)

data class KSMPingResult(
    val pong: Boolean
)

object KSM {
    private val cachedCapabilities: KSMCapabilities by lazy {
        KSMCapabilities(
            platform = KSMPlatform.ANDROID,
            commands = KSMCommandRegistry.supportedCommands,
            permissions = KSMCommandRegistry.permissions
        )
    }

    fun ping(): KSMPingResult {
        return KSMPingResult(pong = true)
    }

    fun get(): KSMCapabilities = cachedCapabilities

    fun queryCapabilities(): KSMCapabilities {
        return get()
    }

    object fs {
        fun readText(fileSystem: KSMFileSystem, path: String): String? {
            return fileSystem.readText(path)
        }

        fun writeText(
            fileSystem: KSMFileSystem,
            path: String,
            content: String
        ): Boolean {
            return fileSystem.writeText(path, content)
        }

        fun exists(fileSystem: KSMFileSystem, path: String): Boolean {
            return fileSystem.exists(path)
        }

        fun remove(fileSystem: KSMFileSystem, path: String): Boolean {
            return fileSystem.remove(path)
        }
    }

    object dialog {
        fun message(
            hostServices: KSMHostServices,
            title: String,
            message: String
        ): Boolean {
            return hostServices.showMessage(title, message)
        }

        fun confirm(
            hostServices: KSMHostServices,
            title: String,
            message: String
        ): Boolean {
            return hostServices.confirm(title, message)
        }

        fun openFile(
            hostServices: KSMHostServices,
            filters: List<String> = emptyList()
        ): String? {
            return hostServices.openFile(filters)
        }

        fun saveFile(
            hostServices: KSMHostServices,
            suggestedName: String = ""
        ): String? {
            return hostServices.saveFile(suggestedName)
        }
    }
}
