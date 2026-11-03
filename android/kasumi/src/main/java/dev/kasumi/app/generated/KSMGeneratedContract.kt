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
        "app.exit"
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
        "app.exit"
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
        "app.exit"
    )

    val events: List<String> = listOf(
        "webview.navigated",
        "webview.reloaded",
        "window.resized"
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
        "log.write"
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
        "log"
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
      "window.control"
    ],
    "defaultPermissions": [
      "window.control"
    ],
    "commands": [
      "ping",
      "capabilities.query",
      "app.getInfo",
      "app.exit",
      "permission.request",
      "event.subscribe",
      "event.unsubscribe"
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
      "fs.write",
      "fs.remove"
    ],
    "defaultPermissions": [
      "fs.read",
      "fs.write",
      "fs.remove"
    ],
    "commands": [
      "fs.readText",
      "fs.writeText",
      "fs.exists",
      "fs.remove",
      "fs.readDir",
      "fs.mkdir",
      "fs.stat"
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
      "dialog.saveFile"
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
      "window.control"
    ],
    "defaultPermissions": [
      "window.control"
    ],
    "commands": [
      "window.setTitle",
      "window.minimize",
      "window.maximize",
      "window.setSize",
      "window.close"
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
      "http.download"
    ],
    "defaultPermissions": [
      "http.download"
    ],
    "commands": [
      "http.download"
    ]
  },
  "shell": {
    "name": "Shell",
    "version": "1.0.0",
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
      "shell.openExternal"
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
            "event.unsubscribe"
        ),
        "fs" to listOf(
            "fs.readText",
            "fs.writeText",
            "fs.exists",
            "fs.remove",
            "fs.readDir",
            "fs.mkdir",
            "fs.stat"
        ),
        "dialog" to listOf(
            "dialog.message",
            "dialog.confirm",
            "dialog.openFile",
            "dialog.saveFile"
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
            "window.close"
        ),
        "http" to listOf(
            "http.download"
        ),
        "shell" to listOf(
            "shell.openExternal"
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
