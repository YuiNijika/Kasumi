package dev.kasumi.app.bridge

import org.json.JSONObject

data class KSMPluginContext(
    val fileSystem: KSMFileSystem?,
    val hostServices: KSMHostServices?,
    val hasPermission: (String) -> Boolean,
    val requestId: String? = null,
    val windowId: String? = null,
    val cancelToken: (() -> Boolean)? = null,
    val emitEvent: (String, JSONObject) -> Boolean = { _, _ -> false }
)

interface KSMPlugin {
    val id: String
    val name: String
        get() = id
    val version: String
        get() = "1.0.0"
    val description: String
        get() = ""
    val commands: Set<String>
    val permissions: Set<String>
        get() = emptySet()
    val events: Set<String>
        get() = emptySet()
    val frameworkEvents: Set<String>
        get() = emptySet()

    fun validateArgs(command: String, args: JSONObject): Boolean {
        return true
    }

    fun handle(
        command: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any

    fun start(context: KSMPluginContext) {
    }

    fun stop() {
    }

    fun onFrameworkEvent(event: String, payload: JSONObject) {
    }
}

open class KSMAppPluginRegistry(
    plugins: Collection<KSMPlugin> = emptyList()
) {
    private val pluginsById = linkedMapOf<String, KSMPlugin>()
    private val handlers = linkedMapOf<String, KSMPlugin>()
    private val startedPlugins = mutableListOf<KSMPlugin>()

    val ids: List<String>
    val commands: List<String>
    val permissions: Set<String>
    val events: Set<String>
    val frameworkEvents: Set<String>

    init {
        val declaredEvents = linkedSetOf<String>()
        val watchedFrameworkEvents = linkedSetOf<String>()
        for (plugin in plugins) {
            require(plugin.id.isNotBlank()) {
                "A Kasumi plugin must have an ID."
            }
            require(pluginsById.putIfAbsent(plugin.id, plugin) == null) {
                "Kasumi plugin IDs must be unique."
            }
            for (command in plugin.commands) {
                require(command.isNotBlank()) {
                    "A Kasumi plugin command cannot be empty."
                }
                require(handlers.putIfAbsent(command, plugin) == null) {
                    "Kasumi plugin commands must be unique."
                }
            }
            for (event in plugin.events) {
                require(event.isNotBlank()) {
                    "A Kasumi plugin event cannot be empty."
                }
                require(declaredEvents.add(event)) {
                    "Kasumi plugin events must be unique."
                }
            }
            for (event in plugin.frameworkEvents) {
                require(event.isNotBlank()) {
                    "A Kasumi plugin framework event cannot be empty."
                }
                watchedFrameworkEvents.add(event)
            }
        }
        ids = pluginsById.keys.toList()
        commands = handlers.keys.toList()
        permissions = pluginsById.values
            .flatMap { it.permissions }
            .toSet()
        events = declaredEvents
        frameworkEvents = watchedFrameworkEvents
    }

    fun start(context: KSMPluginContext) {
        if (startedPlugins.isNotEmpty()) {
            return
        }
        try {
            for (plugin in pluginsById.values) {
                plugin.start(context)
                startedPlugins.add(plugin)
            }
        } catch (error: Throwable) {
            stop()
            throw error
        }
    }

    fun stop() {
        for (index in startedPlugins.indices.reversed()) {
            runCatching { startedPlugins[index].stop() }
        }
        startedPlugins.clear()
    }

    fun notifyFrameworkEvent(event: String, payload: JSONObject) {
        for (plugin in pluginsById.values) {
            if (event in plugin.frameworkEvents) {
                plugin.onFrameworkEvent(event, payload)
            }
        }
    }

    fun owns(command: String): Boolean {
        return command in handlers
    }

    fun knows(permission: String): Boolean {
        return permission in permissions
    }

    fun knowsEvent(event: String): Boolean {
        return event in events
    }

    fun pluginIdForCommand(command: String): String? {
        return handlers[command]?.id
    }

    fun pluginName(id: String): String = pluginsById[id]?.name ?: id

    fun pluginVersion(id: String): String =
        pluginsById[id]?.version ?: "1.0.0"

    fun pluginDescription(id: String): String =
        pluginsById[id]?.description.orEmpty()

    fun permissionsForCommand(command: String): Set<String> {
        return handlers[command]?.permissions.orEmpty()
    }

    fun validateArgs(command: String, args: JSONObject): Boolean {
        val plugin = handlers[command] ?: return false
        return plugin.validateArgs(command, args)
    }

    fun handle(
        command: String,
        args: JSONObject,
        context: KSMPluginContext
    ): Any? {
        return handlers[command]?.handle(command, args, context)
    }
}

typealias KSMPluginRegistry = KSMAppPluginRegistry

fun ksmDefaultFrameworkPlugins(): List<KSMPlugin> {
    return listOf(
        KSMFileSystemPlugin(),
        KSMDialogPlugin(),
        KSMClipboardPlugin(),
        KSMNotificationPlugin(),
        KSMWindowPlugin(),
        KSMHttpPlugin(),
        KSMShellPlugin(),
        KSMDevicePlugin(),
        KSMSqlPlugin(),
        KSMPathPlugin(),
        KSMStorePlugin(),
        KSMOperatingSystemPlugin(),
        KSMLogPlugin()
    ) + KSMCapabilityPlugin()
}
