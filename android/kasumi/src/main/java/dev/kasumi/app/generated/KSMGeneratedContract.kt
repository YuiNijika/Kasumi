package dev.kasumi.app.generated

import org.json.JSONArray
import org.json.JSONObject

object KSMGeneratedContract {
    val commands: List<String> = listOf(
        "ping",
        "capabilities.query",
        "app.getInfo",
        "device.getInfo",
        "fs.readText",
        "fs.writeText",
        "fs.exists",
        "fs.remove",
        "fs.readDir",
        "fs.mkdir",
        "fs.stat",
        "fs.appendText",
        "fs.copy",
        "fs.move",
        "fs.readBytes",
        "fs.writeBytes",
        "dialog.message",
        "dialog.confirm",
        "dialog.openFile",
        "dialog.saveFile",
        "clipboard.readText",
        "clipboard.writeText",
        "notification.show",
        "permission.request",
        "event.subscribe",
        "event.unsubscribe",
        "window.setTitle",
        "window.minimize",
        "window.maximize",
        "window.setSize",
        "window.close",
        "shell.openExternal",
        "shell.openPath",
        "shell.showInFolder",
        "app.singleInstance.state",
        "app.singleInstance.focus",
        "tray.show",
        "tray.hide",
        "globalShortcut.register",
        "globalShortcut.unregister",
        "updater.check",
        "updater.download",
        "http.download",
        "sql.drivers",
        "sql.open",
        "sql.query",
        "sql.execute",
        "sql.close",
        "path.dirs",
        "path.join",
        "path.normalize",
        "path.dirname",
        "path.basename",
        "path.extname",
        "path.isAbsolute",
        "store.get",
        "store.set",
        "store.delete",
        "store.clear",
        "store.keys",
        "os.getInfo",
        "log.write",
        "app.exit",
        "http.request",
        "http.transfer.start",
        "http.transfer.cancel",
        "http.cookies.get",
        "http.cookies.set",
        "http.cookies.clear",
        "dialog.openFolder",
        "dialog.openFiles",
        "fs.walk",
        "fs.scope.release",
        "media.probe",
        "media.ffmpeg.detect",
        "media.ffmpeg.validate",
        "media.ffmpeg.setPath",
        "media.ffmpeg.pickExecutable",
        "audio.devices.list",
        "audio.devices.set",
        "audio.output.getMode",
        "audio.output.setExclusive",
        "audio.eq.set",
        "audio.speed.set",
        "audio.probe",
        "audio.load",
        "audio.play",
        "audio.pause",
        "audio.stop",
        "audio.seek",
        "audio.volume.set",
        "audio.state.get",
        "mediaSession.update",
        "mediaSession.clear",
        "window.create",
        "window.show",
        "window.hide",
        "window.setPosition",
        "window.setAlwaysOnTop",
        "window.setTransparent",
        "window.getState",
        "event.emit",
        "tray.configure",
        "updater.install",
        "app.openDevtools",
        "app.deepLink.getPending"
    )

    val windowsCommands: List<String> = listOf(
        "ping",
        "capabilities.query",
        "app.getInfo",
        "fs.readText",
        "fs.writeText",
        "fs.exists",
        "fs.remove",
        "fs.readDir",
        "fs.mkdir",
        "fs.stat",
        "fs.appendText",
        "fs.copy",
        "fs.move",
        "fs.readBytes",
        "fs.writeBytes",
        "dialog.message",
        "dialog.confirm",
        "dialog.openFile",
        "dialog.saveFile",
        "clipboard.readText",
        "clipboard.writeText",
        "notification.show",
        "permission.request",
        "event.subscribe",
        "event.unsubscribe",
        "window.setTitle",
        "window.minimize",
        "window.maximize",
        "window.setSize",
        "window.close",
        "shell.openExternal",
        "shell.openPath",
        "shell.showInFolder",
        "app.singleInstance.state",
        "app.singleInstance.focus",
        "tray.show",
        "tray.hide",
        "globalShortcut.register",
        "globalShortcut.unregister",
        "updater.check",
        "updater.download",
        "http.download",
        "sql.drivers",
        "sql.open",
        "sql.query",
        "sql.execute",
        "sql.close",
        "path.dirs",
        "path.join",
        "path.normalize",
        "path.dirname",
        "path.basename",
        "path.extname",
        "path.isAbsolute",
        "store.get",
        "store.set",
        "store.delete",
        "store.clear",
        "store.keys",
        "os.getInfo",
        "log.write",
        "app.exit",
        "http.request",
        "http.transfer.start",
        "http.transfer.cancel",
        "http.cookies.get",
        "http.cookies.set",
        "http.cookies.clear",
        "dialog.openFolder",
        "dialog.openFiles",
        "fs.walk",
        "fs.scope.release",
        "media.probe",
        "media.ffmpeg.detect",
        "media.ffmpeg.validate",
        "media.ffmpeg.setPath",
        "media.ffmpeg.pickExecutable",
        "audio.devices.list",
        "audio.devices.set",
        "audio.output.getMode",
        "audio.output.setExclusive",
        "audio.eq.set",
        "audio.speed.set",
        "audio.probe",
        "audio.load",
        "audio.play",
        "audio.pause",
        "audio.stop",
        "audio.seek",
        "audio.volume.set",
        "audio.state.get",
        "window.create",
        "window.show",
        "window.hide",
        "window.setPosition",
        "window.setAlwaysOnTop",
        "window.setTransparent",
        "window.getState",
        "event.emit",
        "tray.configure",
        "updater.install",
        "app.openDevtools"
    )

    val androidCommands: List<String> = listOf(
        "ping",
        "capabilities.query",
        "app.getInfo",
        "device.getInfo",
        "fs.readText",
        "fs.writeText",
        "fs.exists",
        "fs.remove",
        "fs.readDir",
        "fs.mkdir",
        "fs.stat",
        "fs.appendText",
        "fs.copy",
        "fs.move",
        "fs.readBytes",
        "fs.writeBytes",
        "dialog.message",
        "dialog.confirm",
        "dialog.openFile",
        "dialog.saveFile",
        "clipboard.readText",
        "clipboard.writeText",
        "notification.show",
        "permission.request",
        "event.subscribe",
        "event.unsubscribe",
        "window.close",
        "shell.openExternal",
        "shell.openPath",
        "shell.showInFolder",
        "app.singleInstance.state",
        "app.singleInstance.focus",
        "updater.check",
        "updater.download",
        "http.download",
        "sql.drivers",
        "sql.open",
        "sql.query",
        "sql.execute",
        "sql.close",
        "path.dirs",
        "path.join",
        "path.normalize",
        "path.dirname",
        "path.basename",
        "path.extname",
        "path.isAbsolute",
        "store.get",
        "store.set",
        "store.delete",
        "store.clear",
        "store.keys",
        "os.getInfo",
        "log.write",
        "app.exit",
        "http.request",
        "http.transfer.start",
        "http.transfer.cancel",
        "http.cookies.get",
        "http.cookies.set",
        "http.cookies.clear",
        "dialog.openFolder",
        "dialog.openFiles",
        "fs.walk",
        "fs.scope.release",
        "media.probe",
        "media.ffmpeg.detect",
        "media.ffmpeg.validate",
        "audio.devices.list",
        "audio.devices.set",
        "audio.eq.set",
        "audio.speed.set",
        "audio.probe",
        "audio.load",
        "audio.play",
        "audio.pause",
        "audio.stop",
        "audio.seek",
        "audio.volume.set",
        "audio.state.get",
        "mediaSession.update",
        "mediaSession.clear",
        "event.emit",
        "updater.install",
        "app.openDevtools",
        "app.deepLink.getPending"
    )

    val events: List<String> = listOf(
        "webview.navigated",
        "webview.reloaded",
        "window.resized",
        "http.transfer.progress",
        "http.transfer.completed",
        "http.transfer.failed",
        "audio.tick",
        "audio.ended",
        "audio.stateChanged",
        "audio.error",
        "mediaSession.command",
        "window.visibilityChanged",
        "window.stateChanged",
        "tray.action",
        "tray.click",
        "globalShortcut.triggered",
        "updater.progress",
        "updater.status",
        "app.deepLink.received",
        "app.secondInstance.received"
    )

    val permissions: List<String> = listOf(
        "fs.read",
        "fs.write",
        "fs.remove",
        "dialog",
        "clipboard.read",
        "clipboard.write",
        "notification.show",
        "window.control",
        "http.download",
        "shell.open",
        "device.info",
        "sql",
        "store",
        "os.info",
        "log.write",
        "updater",
        "globalShortcut",
        "tray",
        "http.request",
        "http.cookies",
        "http.transfer",
        "media.read",
        "media.control",
        "media.probe",
        "audio.play",
        "mediaSession",
        "window.manage",
        "tray.configure",
        "updater.install",
        "app.deepLink",
        "app.devtools"
    )

    val plugins: List<String> = listOf(
        "core",
        "fs",
        "dialog",
        "clipboard",
        "notification",
        "window",
        "http",
        "shell",
        "device",
        "sql",
        "path",
        "store",
        "os",
        "log",
        "appInstance",
        "updater",
        "globalShortcut",
        "tray",
        "media",
        "audio",
        "mediaSession"
    )

    val pluginMetadata: JSONObject = JSONObject("""{
  "core": {
    "name": "Core",
    "version": "1.0.0",
    "description": "Application lifecycle, permissions, events and capability discovery.",
    "frontendPackage": "kasumi-bridge",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "app.deepLink",
      "app.devtools",
      "window.control"
    ],
    "defaultPermissions": [
      "app.deepLink",
      "app.devtools",
      "window.control"
    ],
    "commands": [
      "ping",
      "capabilities.query",
      "app.getInfo",
      "app.exit",
      "permission.request",
      "event.subscribe",
      "event.unsubscribe",
      "app.openDevtools",
      "app.deepLink.getPending"
    ]
  },
  "fs": {
    "name": "Filesystem",
    "version": "1.0.0",
    "description": "Sandboxed virtual filesystem access.",
    "frontendPackage": "kasumi-plugin-fs",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "fs.read",
      "fs.remove",
      "fs.write"
    ],
    "defaultPermissions": [
      "fs.read",
      "fs.remove",
      "fs.write"
    ],
    "commands": [
      "fs.readText",
      "fs.writeText",
      "fs.appendText",
      "fs.copy",
      "fs.move",
      "fs.readBytes",
      "fs.writeBytes",
      "fs.exists",
      "fs.remove",
      "fs.readDir",
      "fs.mkdir",
      "fs.stat",
      "fs.walk",
      "fs.scope.release"
    ]
  },
  "dialog": {
    "name": "Dialog",
    "version": "1.0.0",
    "description": "Native message, confirmation and file picker dialogs.",
    "frontendPackage": "kasumi-plugin-dialog",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "dialog"
    ],
    "defaultPermissions": [
      "dialog"
    ],
    "commands": [
      "dialog.message",
      "dialog.confirm",
      "dialog.openFile",
      "dialog.saveFile",
      "dialog.openFolder",
      "dialog.openFiles"
    ]
  },
  "clipboard": {
    "name": "Clipboard",
    "version": "1.0.0",
    "description": "System clipboard text access.",
    "frontendPackage": "kasumi-plugin-clipboard",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "clipboard.read",
      "clipboard.write"
    ],
    "defaultPermissions": [
      "clipboard.read",
      "clipboard.write"
    ],
    "commands": [
      "clipboard.readText",
      "clipboard.writeText"
    ]
  },
  "notification": {
    "name": "Notification",
    "version": "1.0.0",
    "description": "Native system notifications.",
    "frontendPackage": "kasumi-plugin-notification",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "notification.show"
    ],
    "defaultPermissions": [
      "notification.show"
    ],
    "commands": [
      "notification.show"
    ]
  },
  "window": {
    "name": "Window",
    "version": "1.0.0",
    "description": "Window title, size, lifecycle and application exit controls.",
    "frontendPackage": "kasumi-plugin-window",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "window.control",
      "window.manage"
    ],
    "defaultPermissions": [
      "window.control",
      "window.manage"
    ],
    "commands": [
      "window.setTitle",
      "window.minimize",
      "window.maximize",
      "window.setSize",
      "window.close",
      "window.create",
      "window.show",
      "window.hide",
      "window.setPosition",
      "window.setAlwaysOnTop",
      "window.setTransparent",
      "window.getState",
      "event.emit"
    ]
  },
  "http": {
    "name": "HTTP",
    "version": "1.0.0",
    "description": "HTTPS downloads into the virtual filesystem.",
    "frontendPackage": "kasumi-plugin-http",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "http.cookies",
      "http.download",
      "http.request"
    ],
    "defaultPermissions": [
      "http.cookies",
      "http.download",
      "http.request"
    ],
    "commands": [
      "http.download",
      "http.request",
      "http.transfer.start",
      "http.transfer.cancel",
      "http.cookies.get",
      "http.cookies.set",
      "http.cookies.clear"
    ]
  },
  "shell": {
    "name": "Shell",
    "version": "1.4.0",
    "description": "Allowlisted external URL launching.",
    "frontendPackage": "kasumi-plugin-shell",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "shell.open"
    ],
    "defaultPermissions": [
      "shell.open"
    ],
    "commands": [
      "shell.openExternal",
      "shell.openPath",
      "shell.showInFolder"
    ]
  },
  "device": {
    "name": "Device",
    "version": "1.0.0",
    "description": "Non-identifying Android device information.",
    "frontendPackage": "kasumi-plugin-device",
    "platforms": [
      "android"
    ],
    "permissions": [
      "device.info"
    ],
    "defaultPermissions": [
      "device.info"
    ],
    "commands": [
      "device.getInfo"
    ]
  },
  "sql": {
    "name": "SQL",
    "version": "1.2.0",
    "description": "Database connections and parameterised SQL statements.",
    "frontendPackage": "kasumi-plugin-sql",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "sql"
    ],
    "defaultPermissions": [
      "sql"
    ],
    "commands": [
      "sql.drivers",
      "sql.open",
      "sql.query",
      "sql.execute",
      "sql.close"
    ]
  },
  "path": {
    "name": "Path",
    "version": "1.3.0",
    "description": "Virtual application path inspection and construction.",
    "frontendPackage": "kasumi-plugin-path",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [],
    "defaultPermissions": [],
    "commands": [
      "path.dirs",
      "path.join",
      "path.normalize",
      "path.dirname",
      "path.basename",
      "path.extname",
      "path.isAbsolute"
    ]
  },
  "store": {
    "name": "Store",
    "version": "1.3.0",
    "description": "Persistent JSON key-value storage inside application data.",
    "frontendPackage": "kasumi-plugin-store",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "store"
    ],
    "defaultPermissions": [
      "store"
    ],
    "commands": [
      "store.get",
      "store.set",
      "store.delete",
      "store.clear",
      "store.keys"
    ]
  },
  "os": {
    "name": "OS",
    "version": "1.3.0",
    "description": "Non-identifying operating system and runtime information.",
    "frontendPackage": "kasumi-plugin-os",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "os.info"
    ],
    "defaultPermissions": [
      "os.info"
    ],
    "commands": [
      "os.getInfo"
    ]
  },
  "log": {
    "name": "Log",
    "version": "1.3.0",
    "description": "Platform logger access with bounded messages.",
    "frontendPackage": "kasumi-plugin-log",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "log.write"
    ],
    "defaultPermissions": [
      "log.write"
    ],
    "commands": [
      "log.write"
    ]
  },
  "appInstance": {
    "name": "App Instance",
    "version": "1.4.0",
    "description": "Single instance state and focus coordination.",
    "frontendPackage": "kasumi-bridge",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [],
    "defaultPermissions": [],
    "commands": [
      "app.singleInstance.state",
      "app.singleInstance.focus"
    ]
  },
  "updater": {
    "name": "Updater",
    "version": "1.4.0",
    "description": "Check and download application update packages.",
    "frontendPackage": "kasumi-plugin-updater",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "updater",
      "updater.install"
    ],
    "defaultPermissions": [
      "updater",
      "updater.install"
    ],
    "commands": [
      "updater.check",
      "updater.download",
      "updater.install"
    ]
  },
  "globalShortcut": {
    "name": "Global Shortcut",
    "version": "1.4.0",
    "description": "Register and unregister global keyboard shortcuts.",
    "frontendPackage": "kasumi-plugin-global-shortcut",
    "platforms": [
      "windows"
    ],
    "permissions": [
      "globalShortcut"
    ],
    "defaultPermissions": [
      "globalShortcut"
    ],
    "commands": [
      "globalShortcut.register",
      "globalShortcut.unregister"
    ]
  },
  "tray": {
    "name": "Tray",
    "version": "1.4.0",
    "description": "Show and hide the system tray icon.",
    "frontendPackage": "kasumi-plugin-tray",
    "platforms": [
      "windows"
    ],
    "permissions": [
      "tray",
      "tray.configure"
    ],
    "defaultPermissions": [
      "tray",
      "tray.configure"
    ],
    "commands": [
      "tray.show",
      "tray.hide",
      "tray.configure"
    ]
  },
  "media": {
    "name": "Media",
    "version": "1.5.0",
    "description": "Media metadata, external tool discovery, and virtual media scopes.",
    "frontendPackage": "kasumi-plugin-media",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "media.read"
    ],
    "defaultPermissions": [
      "media.read"
    ],
    "commands": [
      "media.probe",
      "media.ffmpeg.detect",
      "media.ffmpeg.validate",
      "media.ffmpeg.setPath",
      "media.ffmpeg.pickExecutable"
    ]
  },
  "audio": {
    "name": "Audio",
    "version": "1.5.0",
    "description": "Audio playback, device selection, effects, and state.",
    "frontendPackage": "kasumi-plugin-audio",
    "platforms": [
      "windows",
      "android"
    ],
    "permissions": [
      "media.control",
      "media.read"
    ],
    "defaultPermissions": [
      "media.control",
      "media.read"
    ],
    "commands": [
      "audio.devices.list",
      "audio.devices.set",
      "audio.output.getMode",
      "audio.output.setExclusive",
      "audio.eq.set",
      "audio.speed.set",
      "audio.probe",
      "audio.load",
      "audio.play",
      "audio.pause",
      "audio.stop",
      "audio.seek",
      "audio.volume.set",
      "audio.state.get"
    ]
  },
  "mediaSession": {
    "name": "Media Session",
    "version": "1.5.0",
    "description": "System media session metadata and transport commands.",
    "frontendPackage": "kasumi-plugin-media-session",
    "platforms": [
      "android"
    ],
    "permissions": [
      "mediaSession"
    ],
    "defaultPermissions": [
      "mediaSession"
    ],
    "commands": [
      "mediaSession.update",
      "mediaSession.clear"
    ]
  }
}""")

    val pluginCommands: Map<String, List<String>> = mapOf(
        "core" to listOf(
            "ping",
            "capabilities.query",
            "app.getInfo",
            "app.exit",
            "permission.request",
            "event.subscribe",
            "event.unsubscribe",
            "app.openDevtools",
            "app.deepLink.getPending"
        ),
        "fs" to listOf(
            "fs.readText",
            "fs.writeText",
            "fs.appendText",
            "fs.copy",
            "fs.move",
            "fs.readBytes",
            "fs.writeBytes",
            "fs.exists",
            "fs.remove",
            "fs.readDir",
            "fs.mkdir",
            "fs.stat",
            "fs.walk",
            "fs.scope.release"
        ),
        "dialog" to listOf(
            "dialog.message",
            "dialog.confirm",
            "dialog.openFile",
            "dialog.saveFile",
            "dialog.openFolder",
            "dialog.openFiles"
        ),
        "clipboard" to listOf(
            "clipboard.readText",
            "clipboard.writeText"
        ),
        "notification" to listOf(
            "notification.show"
        ),
        "window" to listOf(
            "window.setTitle",
            "window.minimize",
            "window.maximize",
            "window.setSize",
            "window.close",
            "window.create",
            "window.show",
            "window.hide",
            "window.setPosition",
            "window.setAlwaysOnTop",
            "window.setTransparent",
            "window.getState",
            "event.emit"
        ),
        "http" to listOf(
            "http.download",
            "http.request",
            "http.transfer.start",
            "http.transfer.cancel",
            "http.cookies.get",
            "http.cookies.set",
            "http.cookies.clear"
        ),
        "shell" to listOf(
            "shell.openExternal",
            "shell.openPath",
            "shell.showInFolder"
        ),
        "device" to listOf(
            "device.getInfo"
        ),
        "sql" to listOf(
            "sql.drivers",
            "sql.open",
            "sql.query",
            "sql.execute",
            "sql.close"
        ),
        "path" to listOf(
            "path.dirs",
            "path.join",
            "path.normalize",
            "path.dirname",
            "path.basename",
            "path.extname",
            "path.isAbsolute"
        ),
        "store" to listOf(
            "store.get",
            "store.set",
            "store.delete",
            "store.clear",
            "store.keys"
        ),
        "os" to listOf(
            "os.getInfo"
        ),
        "log" to listOf(
            "log.write"
        ),
        "appInstance" to listOf(
            "app.singleInstance.state",
            "app.singleInstance.focus"
        ),
        "updater" to listOf(
            "updater.check",
            "updater.download",
            "updater.install"
        ),
        "globalShortcut" to listOf(
            "globalShortcut.register",
            "globalShortcut.unregister"
        ),
        "tray" to listOf(
            "tray.show",
            "tray.hide",
            "tray.configure"
        ),
        "media" to listOf(
            "media.probe",
            "media.ffmpeg.detect",
            "media.ffmpeg.validate",
            "media.ffmpeg.setPath",
            "media.ffmpeg.pickExecutable"
        ),
        "audio" to listOf(
            "audio.devices.list",
            "audio.devices.set",
            "audio.output.getMode",
            "audio.output.setExclusive",
            "audio.eq.set",
            "audio.speed.set",
            "audio.probe",
            "audio.load",
            "audio.play",
            "audio.pause",
            "audio.stop",
            "audio.seek",
            "audio.volume.set",
            "audio.state.get"
        ),
        "mediaSession" to listOf(
            "mediaSession.update",
            "mediaSession.clear"
        )
    )

    val commandPermissions: Map<String, List<String>> = mapOf(
        "ping" to emptyList(),
        "capabilities.query" to emptyList(),
        "app.getInfo" to emptyList(),
        "device.getInfo" to listOf(
            "device.info"
        ),
        "fs.readText" to listOf(
            "fs.read"
        ),
        "fs.writeText" to listOf(
            "fs.write"
        ),
        "fs.exists" to listOf(
            "fs.read"
        ),
        "fs.remove" to listOf(
            "fs.remove"
        ),
        "fs.readDir" to listOf(
            "fs.read"
        ),
        "fs.mkdir" to listOf(
            "fs.write"
        ),
        "fs.stat" to listOf(
            "fs.read"
        ),
        "fs.appendText" to listOf(
            "fs.write"
        ),
        "fs.copy" to listOf(
            "fs.read",
            "fs.write"
        ),
        "fs.move" to listOf(
            "fs.write"
        ),
        "fs.readBytes" to listOf(
            "fs.read"
        ),
        "fs.writeBytes" to listOf(
            "fs.write"
        ),
        "dialog.message" to listOf(
            "dialog"
        ),
        "dialog.confirm" to listOf(
            "dialog"
        ),
        "dialog.openFile" to listOf(
            "dialog"
        ),
        "dialog.saveFile" to listOf(
            "dialog"
        ),
        "clipboard.readText" to listOf(
            "clipboard.read"
        ),
        "clipboard.writeText" to listOf(
            "clipboard.write"
        ),
        "notification.show" to listOf(
            "notification.show"
        ),
        "permission.request" to emptyList(),
        "event.subscribe" to emptyList(),
        "event.unsubscribe" to emptyList(),
        "window.setTitle" to listOf(
            "window.control"
        ),
        "window.minimize" to listOf(
            "window.control"
        ),
        "window.maximize" to listOf(
            "window.control"
        ),
        "window.setSize" to listOf(
            "window.control"
        ),
        "window.close" to listOf(
            "window.control"
        ),
        "shell.openExternal" to listOf(
            "shell.open"
        ),
        "shell.openPath" to listOf(
            "shell.open"
        ),
        "shell.showInFolder" to listOf(
            "shell.open"
        ),
        "app.singleInstance.state" to emptyList(),
        "app.singleInstance.focus" to emptyList(),
        "tray.show" to listOf(
            "tray"
        ),
        "tray.hide" to listOf(
            "tray"
        ),
        "globalShortcut.register" to listOf(
            "globalShortcut"
        ),
        "globalShortcut.unregister" to listOf(
            "globalShortcut"
        ),
        "updater.check" to listOf(
            "updater"
        ),
        "updater.download" to listOf(
            "updater"
        ),
        "http.download" to listOf(
            "http.download"
        ),
        "sql.drivers" to listOf(
            "sql"
        ),
        "sql.open" to listOf(
            "sql"
        ),
        "sql.query" to listOf(
            "sql"
        ),
        "sql.execute" to listOf(
            "sql"
        ),
        "sql.close" to listOf(
            "sql"
        ),
        "path.dirs" to emptyList(),
        "path.join" to emptyList(),
        "path.normalize" to emptyList(),
        "path.dirname" to emptyList(),
        "path.basename" to emptyList(),
        "path.extname" to emptyList(),
        "path.isAbsolute" to emptyList(),
        "store.get" to listOf(
            "store"
        ),
        "store.set" to listOf(
            "store"
        ),
        "store.delete" to listOf(
            "store"
        ),
        "store.clear" to listOf(
            "store"
        ),
        "store.keys" to listOf(
            "store"
        ),
        "os.getInfo" to listOf(
            "os.info"
        ),
        "log.write" to listOf(
            "log.write"
        ),
        "app.exit" to listOf(
            "window.control"
        ),
        "http.request" to listOf(
            "http.request"
        ),
        "http.transfer.start" to listOf(
            "http.request"
        ),
        "http.transfer.cancel" to listOf(
            "http.request"
        ),
        "http.cookies.get" to listOf(
            "http.cookies"
        ),
        "http.cookies.set" to listOf(
            "http.cookies"
        ),
        "http.cookies.clear" to listOf(
            "http.cookies"
        ),
        "dialog.openFolder" to listOf(
            "dialog"
        ),
        "dialog.openFiles" to listOf(
            "dialog"
        ),
        "fs.walk" to listOf(
            "fs.read"
        ),
        "fs.scope.release" to listOf(
            "fs.read"
        ),
        "media.probe" to listOf(
            "media.read"
        ),
        "media.ffmpeg.detect" to listOf(
            "media.read"
        ),
        "media.ffmpeg.validate" to listOf(
            "media.read"
        ),
        "media.ffmpeg.setPath" to listOf(
            "media.read"
        ),
        "media.ffmpeg.pickExecutable" to listOf(
            "media.read"
        ),
        "audio.devices.list" to listOf(
            "media.control"
        ),
        "audio.devices.set" to listOf(
            "media.control"
        ),
        "audio.output.getMode" to listOf(
            "media.control"
        ),
        "audio.output.setExclusive" to listOf(
            "media.control"
        ),
        "audio.eq.set" to listOf(
            "media.control"
        ),
        "audio.speed.set" to listOf(
            "media.control"
        ),
        "audio.probe" to listOf(
            "media.read"
        ),
        "audio.load" to listOf(
            "media.control"
        ),
        "audio.play" to listOf(
            "media.control"
        ),
        "audio.pause" to listOf(
            "media.control"
        ),
        "audio.stop" to listOf(
            "media.control"
        ),
        "audio.seek" to listOf(
            "media.control"
        ),
        "audio.volume.set" to listOf(
            "media.control"
        ),
        "audio.state.get" to listOf(
            "media.read"
        ),
        "mediaSession.update" to listOf(
            "mediaSession"
        ),
        "mediaSession.clear" to listOf(
            "mediaSession"
        ),
        "window.create" to listOf(
            "window.manage"
        ),
        "window.show" to listOf(
            "window.manage"
        ),
        "window.hide" to listOf(
            "window.manage"
        ),
        "window.setPosition" to listOf(
            "window.manage"
        ),
        "window.setAlwaysOnTop" to listOf(
            "window.manage"
        ),
        "window.setTransparent" to listOf(
            "window.manage"
        ),
        "window.getState" to listOf(
            "window.manage"
        ),
        "event.emit" to listOf(
            "window.manage"
        ),
        "tray.configure" to listOf(
            "tray.configure"
        ),
        "updater.install" to listOf(
            "updater.install"
        ),
        "app.openDevtools" to listOf(
            "app.devtools"
        ),
        "app.deepLink.getPending" to listOf(
            "app.deepLink"
        )
    )

    private const val ARG_SCHEMAS_JSON = """{
  "ping": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "capabilities.query": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "app.getInfo": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "device.getInfo": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "fs.readText": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.writeText": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      },
      "content": {
        "type": "string",
        "maxBytes": 16777216
      }
    },
    "required": [
      "path",
      "content"
    ],
    "additionalProperties": false
  },
  "fs.exists": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.remove": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.readDir": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.mkdir": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      },
      "recursive": {
        "type": "boolean"
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.stat": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.appendText": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      },
      "content": {
        "type": "string",
        "maxBytes": 16777216
      }
    },
    "required": [
      "path",
      "content"
    ],
    "additionalProperties": false
  },
  "fs.copy": {
    "type": "object",
    "properties": {
      "from": {
        "type": "string",
        "minLength": 1
      },
      "to": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "from",
      "to"
    ],
    "additionalProperties": false
  },
  "fs.move": {
    "type": "object",
    "properties": {
      "from": {
        "type": "string",
        "minLength": 1
      },
      "to": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "from",
      "to"
    ],
    "additionalProperties": false
  },
  "fs.readBytes": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.writeBytes": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      },
      "base64": {
        "type": "string",
        "maxBytes": 33554432
      }
    },
    "required": [
      "path",
      "base64"
    ],
    "additionalProperties": false
  },
  "dialog.message": {
    "type": "object",
    "properties": {
      "title": {
        "type": "string"
      },
      "message": {
        "type": "string"
      }
    },
    "required": [
      "title",
      "message"
    ],
    "additionalProperties": false
  },
  "dialog.confirm": {
    "type": "object",
    "properties": {
      "title": {
        "type": "string"
      },
      "message": {
        "type": "string"
      }
    },
    "required": [
      "title",
      "message"
    ],
    "additionalProperties": false
  },
  "dialog.openFile": {
    "type": "object",
    "properties": {
      "filters": {
        "type": "array",
        "items": {
          "type": "string"
        }
      }
    },
    "required": [],
    "additionalProperties": false
  },
  "dialog.saveFile": {
    "type": "object",
    "properties": {
      "suggestedName": {
        "type": "string"
      }
    },
    "required": [],
    "additionalProperties": false
  },
  "clipboard.readText": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "clipboard.writeText": {
    "type": "object",
    "properties": {
      "text": {
        "type": "string"
      }
    },
    "required": [
      "text"
    ],
    "additionalProperties": false
  },
  "notification.show": {
    "type": "object",
    "properties": {
      "title": {
        "type": "string"
      },
      "body": {
        "type": "string"
      }
    },
    "required": [
      "title",
      "body"
    ],
    "additionalProperties": false
  },
  "permission.request": {
    "type": "object",
    "properties": {
      "permission": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "permission"
    ],
    "additionalProperties": false
  },
  "event.subscribe": {
    "type": "object",
    "properties": {
      "event": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "event"
    ],
    "additionalProperties": false
  },
  "event.unsubscribe": {
    "type": "object",
    "properties": {
      "event": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "event"
    ],
    "additionalProperties": false
  },
  "window.setTitle": {
    "type": "object",
    "properties": {
      "title": {
        "type": "string"
      }
    },
    "required": [
      "title"
    ],
    "additionalProperties": false
  },
  "window.minimize": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "window.maximize": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "window.setSize": {
    "type": "object",
    "properties": {
      "width": {
        "type": "integer"
      },
      "height": {
        "type": "integer"
      }
    },
    "required": [
      "width",
      "height"
    ],
    "additionalProperties": false
  },
  "window.close": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "shell.openExternal": {
    "type": "object",
    "properties": {
      "url": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "url"
    ],
    "additionalProperties": false
  },
  "shell.openPath": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "shell.showInFolder": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "app.singleInstance.state": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "app.singleInstance.focus": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "tray.show": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "tray.hide": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "globalShortcut.register": {
    "type": "object",
    "properties": {
      "shortcut": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 256
      }
    },
    "required": [
      "shortcut"
    ],
    "additionalProperties": false
  },
  "globalShortcut.unregister": {
    "type": "object",
    "properties": {
      "shortcut": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 256
      }
    },
    "required": [
      "shortcut"
    ],
    "additionalProperties": false
  },
  "updater.check": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "updater.download": {
    "type": "object",
    "properties": {
      "version": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 128
      }
    },
    "required": [
      "version"
    ],
    "additionalProperties": false
  },
  "http.download": {
    "type": "object",
    "properties": {
      "url": {
        "type": "string",
        "minLength": 1
      },
      "path": {
        "type": "string",
        "minLength": 1
      },
      "overwrite": {
        "type": "boolean"
      }
    },
    "required": [
      "url",
      "path"
    ],
    "additionalProperties": false
  },
  "sql.drivers": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "sql.open": {
    "type": "object",
    "properties": {
      "driver": {
        "type": "string",
        "enum": [
          "sqlite",
          "mysql",
          "postgres",
          "mongodb"
        ]
      },
      "url": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 4096
      }
    },
    "required": [
      "driver",
      "url"
    ],
    "additionalProperties": false
  },
  "sql.query": {
    "type": "object",
    "properties": {
      "connectionId": {
        "type": "string",
        "minLength": 1
      },
      "statement": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 1048576
      },
      "params": {
        "type": "array",
        "items": {
          "type": [
            "string",
            "number",
            "boolean",
            "null"
          ]
        }
      }
    },
    "required": [
      "connectionId",
      "statement"
    ],
    "additionalProperties": false
  },
  "sql.execute": {
    "type": "object",
    "properties": {
      "connectionId": {
        "type": "string",
        "minLength": 1
      },
      "statement": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 1048576
      },
      "params": {
        "type": "array",
        "items": {
          "type": [
            "string",
            "number",
            "boolean",
            "null"
          ]
        }
      }
    },
    "required": [
      "connectionId",
      "statement"
    ],
    "additionalProperties": false
  },
  "sql.close": {
    "type": "object",
    "properties": {
      "connectionId": {
        "type": "string",
        "minLength": 1
      }
    },
    "required": [
      "connectionId"
    ],
    "additionalProperties": false
  },
  "path.dirs": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "path.join": {
    "type": "object",
    "properties": {
      "base": {
        "type": "string",
        "minLength": 1
      },
      "segments": {
        "type": "array",
        "items": {
          "type": "string",
          "minLength": 1,
          "maxBytes": 256
        }
      }
    },
    "required": [
      "base",
      "segments"
    ],
    "additionalProperties": false
  },
  "path.normalize": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 8192
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "path.dirname": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 8192
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "path.basename": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 8192
      },
      "suffix": {
        "type": "string",
        "maxBytes": 256
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "path.extname": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 8192
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "path.isAbsolute": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 8192
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "store.get": {
    "type": "object",
    "properties": {
      "key": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 256
      }
    },
    "required": [
      "key"
    ],
    "additionalProperties": false
  },
  "store.set": {
    "type": "object",
    "properties": {
      "key": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 256
      },
      "value": {
        "type": [
          "object",
          "array",
          "string",
          "number",
          "boolean",
          "null"
        ]
      }
    },
    "required": [
      "key",
      "value"
    ],
    "additionalProperties": false
  },
  "store.delete": {
    "type": "object",
    "properties": {
      "key": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 256
      }
    },
    "required": [
      "key"
    ],
    "additionalProperties": false
  },
  "store.clear": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "store.keys": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "os.getInfo": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "log.write": {
    "type": "object",
    "properties": {
      "level": {
        "type": "string",
        "enum": [
          "trace",
          "debug",
          "info",
          "warn",
          "error"
        ]
      },
      "message": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 65536
      },
      "target": {
        "type": "string",
        "minLength": 1,
        "maxBytes": 128
      }
    },
    "required": [
      "level",
      "message"
    ],
    "additionalProperties": false
  },
  "app.exit": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "http.request": {
    "type": "object",
    "properties": {
      "url": {
        "type": "string",
        "minLength": 1
      },
      "method": {
        "type": "string",
        "enum": [
          "GET",
          "POST"
        ]
      },
      "headers": {
        "type": "object"
      },
      "body": {
        "type": "string"
      },
      "timeoutMs": {
        "type": "integer"
      }
    },
    "required": [
      "url"
    ],
    "additionalProperties": false
  },
  "http.transfer.start": {
    "type": "object",
    "properties": {
      "url": {
        "type": "string",
        "minLength": 1
      },
      "method": {
        "type": "string",
        "enum": [
          "GET",
          "POST"
        ]
      },
      "headers": {
        "type": "object"
      },
      "body": {
        "type": "string"
      },
      "path": {
        "type": "string"
      },
      "overwrite": {
        "type": "boolean"
      },
      "jarId": {
        "type": "string"
      },
      "userAgent": {
        "type": "string"
      },
      "referer": {
        "type": "string"
      }
    },
    "required": [
      "url",
      "path"
    ],
    "additionalProperties": false
  },
  "http.transfer.cancel": {
    "type": "object",
    "properties": {
      "transferId": {
        "type": "string"
      }
    },
    "required": [
      "transferId"
    ],
    "additionalProperties": false
  },
  "http.cookies.get": {
    "type": "object",
    "properties": {
      "url": {
        "type": "string"
      },
      "jarId": {
        "type": "string"
      }
    },
    "required": [
      "url",
      "jarId"
    ],
    "additionalProperties": false
  },
  "http.cookies.set": {
    "type": "object",
    "properties": {
      "url": {
        "type": "string"
      },
      "jarId": {
        "type": "string"
      },
      "cookies": {
        "type": "array",
        "items": {
          "type": "object"
        }
      }
    },
    "required": [
      "url",
      "jarId",
      "cookies"
    ],
    "additionalProperties": false
  },
  "http.cookies.clear": {
    "type": "object",
    "properties": {
      "url": {
        "type": "string"
      },
      "jarId": {
        "type": "string"
      }
    },
    "required": [
      "url",
      "jarId"
    ],
    "additionalProperties": false
  },
  "dialog.openFolder": {
    "type": "object",
    "properties": {
      "persist": {
        "type": "boolean"
      },
      "filters": {
        "type": "object"
      }
    },
    "required": [],
    "additionalProperties": false
  },
  "dialog.openFiles": {
    "type": "object",
    "properties": {
      "filters": {
        "type": "array",
        "items": {
          "type": "string"
        }
      }
    },
    "required": [],
    "additionalProperties": false
  },
  "fs.walk": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string"
      },
      "maxDepth": {
        "type": "integer"
      },
      "filter": {
        "type": "object"
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "fs.scope.release": {
    "type": "object",
    "properties": {
      "scopeId": {
        "type": "string"
      }
    },
    "required": [
      "scopeId"
    ],
    "additionalProperties": false
  },
  "media.probe": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string"
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "media.ffmpeg.detect": {
    "type": "object",
    "properties": {
      "name": {
        "type": "string"
      }
    },
    "required": [],
    "additionalProperties": false
  },
  "media.ffmpeg.validate": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string"
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "media.ffmpeg.setPath": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string"
      },
      "name": {
        "type": "string"
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "media.ffmpeg.pickExecutable": {
    "type": "object",
    "properties": {
      "name": {
        "type": "string"
      }
    },
    "required": [],
    "additionalProperties": false
  },
  "audio.devices.list": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "audio.devices.set": {
    "type": "object",
    "properties": {
      "deviceId": {
        "type": "string"
      }
    },
    "required": [
      "deviceId"
    ],
    "additionalProperties": false
  },
  "audio.output.getMode": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "audio.output.setExclusive": {
    "type": "object",
    "properties": {
      "exclusive": {
        "type": "boolean"
      }
    },
    "required": [
      "exclusive"
    ],
    "additionalProperties": false
  },
  "audio.eq.set": {
    "type": "object",
    "properties": {
      "gains": {
        "type": "array",
        "items": {
          "type": "number"
        }
      }
    },
    "required": [
      "gains"
    ],
    "additionalProperties": false
  },
  "audio.speed.set": {
    "type": "object",
    "properties": {
      "speed": {
        "type": "number"
      }
    },
    "required": [
      "speed"
    ],
    "additionalProperties": false
  },
  "audio.probe": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string"
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "audio.load": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string"
      }
    },
    "required": [
      "path"
    ],
    "additionalProperties": false
  },
  "audio.play": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "audio.pause": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "audio.stop": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "audio.seek": {
    "type": "object",
    "properties": {
      "positionMs": {
        "type": "integer"
      }
    },
    "required": [
      "positionMs"
    ],
    "additionalProperties": false
  },
  "audio.volume.set": {
    "type": "object",
    "properties": {
      "volume": {
        "type": "number"
      }
    },
    "required": [
      "volume"
    ],
    "additionalProperties": false
  },
  "audio.state.get": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "mediaSession.update": {
    "type": "object",
    "properties": {
      "title": {
        "type": "string"
      },
      "artist": {
        "type": "string"
      },
      "album": {
        "type": "string"
      },
      "artwork": {
        "type": "string"
      },
      "durationMs": {
        "type": [
          "number",
          "null"
        ]
      },
      "positionMs": {
        "type": [
          "number",
          "null"
        ]
      },
      "state": {
        "type": "string"
      }
    },
    "required": [
      "title"
    ],
    "additionalProperties": false
  },
  "mediaSession.clear": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  },
  "window.create": {
    "type": "object",
    "properties": {
      "route": {
        "type": "string"
      },
      "url": {
        "type": "string"
      },
      "title": {
        "type": "string"
      },
      "width": {
        "type": "integer"
      },
      "height": {
        "type": "integer"
      },
      "x": {
        "type": "integer"
      },
      "y": {
        "type": "integer"
      },
      "visible": {
        "type": "boolean"
      },
      "alwaysOnTop": {
        "type": "boolean"
      },
      "transparent": {
        "type": "boolean"
      },
      "resizable": {
        "type": "boolean"
      },
      "decorations": {
        "type": "boolean"
      }
    },
    "required": [],
    "additionalProperties": false
  },
  "window.show": {
    "type": "object",
    "properties": {
      "windowId": {
        "type": "string"
      }
    },
    "required": [
      "windowId"
    ],
    "additionalProperties": false
  },
  "window.hide": {
    "type": "object",
    "properties": {
      "windowId": {
        "type": "string"
      }
    },
    "required": [
      "windowId"
    ],
    "additionalProperties": false
  },
  "window.setPosition": {
    "type": "object",
    "properties": {
      "windowId": {
        "type": "string"
      },
      "x": {
        "type": "integer"
      },
      "y": {
        "type": "integer"
      }
    },
    "required": [
      "windowId",
      "x",
      "y"
    ],
    "additionalProperties": false
  },
  "window.setAlwaysOnTop": {
    "type": "object",
    "properties": {
      "windowId": {
        "type": "string"
      },
      "enabled": {
        "type": "boolean"
      }
    },
    "required": [
      "windowId",
      "enabled"
    ],
    "additionalProperties": false
  },
  "window.setTransparent": {
    "type": "object",
    "properties": {
      "windowId": {
        "type": "string"
      },
      "enabled": {
        "type": "boolean"
      }
    },
    "required": [
      "windowId",
      "enabled"
    ],
    "additionalProperties": false
  },
  "window.getState": {
    "type": "object",
    "properties": {
      "windowId": {
        "type": "string"
      }
    },
    "required": [
      "windowId"
    ],
    "additionalProperties": false
  },
  "event.emit": {
    "type": "object",
    "properties": {
      "event": {
        "type": "string"
      },
      "payload": {
        "type": "object"
      },
      "windowId": {
        "type": "string"
      }
    },
    "required": [
      "event"
    ],
    "additionalProperties": false
  },
  "tray.configure": {
    "type": "object",
    "properties": {
      "icon": {
        "type": "string"
      },
      "tooltip": {
        "type": "string"
      },
      "items": {
        "type": "array",
        "items": {
          "type": "object"
        }
      }
    },
    "required": [],
    "additionalProperties": false
  },
  "updater.install": {
    "type": "object",
    "properties": {
      "path": {
        "type": "string"
      },
      "version": {
        "type": "string"
      },
      "hash": {
        "type": "string"
      },
      "signature": {
        "type": "string"
      }
    },
    "required": [
      "path",
      "version"
    ],
    "additionalProperties": false
  },
  "app.openDevtools": {
    "type": "object",
    "properties": {
      "enabled": {
        "type": "boolean"
      }
    },
    "required": [],
    "additionalProperties": false
  },
  "app.deepLink.getPending": {
    "type": "object",
    "properties": {},
    "required": [],
    "additionalProperties": false
  }
}"""
    private val argSchemas = JSONObject(ARG_SCHEMAS_JSON)

    fun isKnownCommand(command: String): Boolean = argSchemas.has(command)

    fun validateArgs(command: String, args: JSONObject): Boolean {
        val schema = argSchemas.optJSONObject(command) ?: return false
        return validateValue(args, schema)
    }

    private fun validateValue(value: Any?, schema: JSONObject): Boolean {
        val rawType = schema.opt("type")
        val types: List<String> = when (rawType) {
            is String -> listOf(rawType)
            is JSONArray -> (0 until rawType.length()).mapNotNull { rawType.optString(it, "") }
            else -> emptyList()
        }
        if (types.isEmpty() || types.none { matchesType(value, it) }) {
            return false
        }

        if (schema.has("enum")) {
            val candidates = schema.optJSONArray("enum") ?: return false
            var found = false
            for (index in 0 until candidates.length()) {
                if (sameValue(candidates.opt(index), value)) {
                    found = true
                    break
                }
            }
            if (!found) return false
        }
        if (schema.has("const") && !sameValue(schema.opt("const"), value)) {
            return false
        }

        if (value is JSONObject) {
            val properties = schema.optJSONObject("properties") ?: JSONObject()
            val required = schema.optJSONArray("required")
            if (required != null) {
                for (index in 0 until required.length()) {
                    if (!value.has(required.optString(index))) return false
                }
            }
            if (schema.optBoolean("additionalProperties", true).not()) {
                val keys = value.keys()
                while (keys.hasNext()) {
                    if (!properties.has(keys.next())) return false
                }
            }
            val propertyKeys = properties.keys()
            while (propertyKeys.hasNext()) {
                val field = propertyKeys.next()
                val propertySchema = properties.optJSONObject(field) ?: continue
                if (value.has(field) && !validateValue(value.opt(field), propertySchema)) {
                    return false
                }
            }
        }

        if (value is JSONArray && schema.has("items")) {
            val itemSchema = schema.optJSONObject("items") ?: return false
            for (index in 0 until value.length()) {
                if (!validateValue(value.opt(index), itemSchema)) return false
            }
        }

        if (value is String) {
            if (schema.has("minLength") && value.length < schema.optInt("minLength")) {
                return false
            }
            if (
                schema.has("maxBytes") &&
                value.toByteArray(Charsets.UTF_8).size > schema.optInt("maxBytes")
            ) {
                return false
            }
        }
        return true
    }

    private fun matchesType(value: Any?, type: String): Boolean {
        return when (type) {
            "object" -> value is JSONObject
            "array" -> value is JSONArray
            "string" -> value is String
            "number" -> value is Number
            "integer" -> value is Number && value.toDouble() % 1.0 == 0.0
            "boolean" -> value is Boolean
            "null" -> value == null || value === JSONObject.NULL
            else -> false
        }
    }

    private fun sameValue(left: Any?, right: Any?): Boolean {
        return left == right || left?.toString() == right?.toString()
    }
}
