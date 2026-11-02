export const KSM_COMMANDS = [
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
] as const;
export const KSM_WINDOWS_COMMANDS = [
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
] as const;
export const KSM_ANDROID_COMMANDS = [
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
] as const;
export const KSM_EVENTS = [
  "webview.reloaded",
  "window.resized"
] as const;
export const KSM_PERMISSIONS = [
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
] as const;
export const KSM_ERROR_CODES = [
  "INVALID_ARGS",
  "UNKNOWN_COMMAND",
  "PERMISSION_DENIED",
  "NOT_SUPPORTED",
  "NOT_FOUND",
  "IO_ERROR",
  "CANCELLED",
  "TIMEOUT",
  "BUSY",
  "RESOURCE_LIMIT",
  "INTERNAL_ERROR"
] as const;
export const KSM_PLUGINS = [
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
] as const;
export interface KSMPluginMetadata {
  name: string;
  version: string;
  description: string;
  frontendPackage: string;
  platforms: readonly ("windows" | "android")[];
  permissions: readonly string[];
  defaultPermissions: readonly string[];
  commands: readonly string[];
}
export const KSM_PLUGIN_METADATA: Readonly<Record<string, KSMPluginMetadata>> = {
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
};
export const KSM_PLUGIN_COMMANDS: Readonly<Record<string, readonly string[]>> = {
  "core": [
    "ping",
    "capabilities.query",
    "app.getInfo",
    "app.exit",
    "permission.request",
    "event.subscribe",
    "event.unsubscribe"
  ],
  "fs": [
    "fs.readText",
    "fs.writeText",
    "fs.exists",
    "fs.remove",
    "fs.readDir",
    "fs.mkdir",
    "fs.stat"
  ],
  "dialog": [
    "dialog.message",
    "dialog.confirm",
    "dialog.openFile",
    "dialog.saveFile"
  ],
  "clipboard": [
    "clipboard.readText",
    "clipboard.writeText"
  ],
  "notification": [
    "notification.show"
  ],
  "window": [
    "window.setTitle",
    "window.minimize",
    "window.maximize",
    "window.setSize",
    "window.close"
  ],
  "http": [
    "http.download"
  ],
  "shell": [
    "shell.openExternal"
  ],
  "device": [
    "device.getInfo"
  ],
  "sql": [
    "sql.drivers",
    "sql.open",
    "sql.query",
    "sql.execute",
    "sql.close"
  ],
  "path": [
    "path.dirs",
    "path.join",
    "path.normalize",
    "path.dirname",
    "path.basename",
    "path.extname",
    "path.isAbsolute"
  ],
  "store": [
    "store.get",
    "store.set",
    "store.delete",
    "store.clear",
    "store.keys"
  ],
  "os": [
    "os.getInfo"
  ],
  "log": [
    "log.write"
  ]
};
export const KSM_COMMAND_PLUGINS: Readonly<Record<string, string>> = {
  "ping": "core",
  "capabilities.query": "core",
  "app.getInfo": "core",
  "app.exit": "core",
  "permission.request": "core",
  "event.subscribe": "core",
  "event.unsubscribe": "core",
  "fs.readText": "fs",
  "fs.writeText": "fs",
  "fs.exists": "fs",
  "fs.remove": "fs",
  "fs.readDir": "fs",
  "fs.mkdir": "fs",
  "fs.stat": "fs",
  "dialog.message": "dialog",
  "dialog.confirm": "dialog",
  "dialog.openFile": "dialog",
  "dialog.saveFile": "dialog",
  "clipboard.readText": "clipboard",
  "clipboard.writeText": "clipboard",
  "notification.show": "notification",
  "window.setTitle": "window",
  "window.minimize": "window",
  "window.maximize": "window",
  "window.setSize": "window",
  "window.close": "window",
  "http.download": "http",
  "shell.openExternal": "shell",
  "device.getInfo": "device",
  "sql.drivers": "sql",
  "sql.open": "sql",
  "sql.query": "sql",
  "sql.execute": "sql",
  "sql.close": "sql",
  "path.dirs": "path",
  "path.join": "path",
  "path.normalize": "path",
  "path.dirname": "path",
  "path.basename": "path",
  "path.extname": "path",
  "path.isAbsolute": "path",
  "store.get": "store",
  "store.set": "store",
  "store.delete": "store",
  "store.clear": "store",
  "store.keys": "store",
  "os.getInfo": "os",
  "log.write": "log"
};
export const KSM_COMMAND_PERMISSIONS: Readonly<Record<string, readonly string[]>> = {
  "ping": [],
  "capabilities.query": [],
  "app.getInfo": [],
  "device.getInfo": [
    "device.info"
  ],
  "fs.readText": [
    "fs.read"
  ],
  "fs.writeText": [
    "fs.write"
  ],
  "fs.exists": [
    "fs.read"
  ],
  "fs.remove": [
    "fs.remove"
  ],
  "fs.readDir": [
    "fs.read"
  ],
  "fs.mkdir": [
    "fs.write"
  ],
  "fs.stat": [
    "fs.read"
  ],
  "dialog.message": [
    "dialog"
  ],
  "dialog.confirm": [
    "dialog"
  ],
  "dialog.openFile": [
    "dialog"
  ],
  "dialog.saveFile": [
    "dialog"
  ],
  "clipboard.readText": [
    "clipboard.read"
  ],
  "clipboard.writeText": [
    "clipboard.write"
  ],
  "notification.show": [
    "notification.show"
  ],
  "permission.request": [],
  "event.subscribe": [],
  "event.unsubscribe": [],
  "window.setTitle": [
    "window.control"
  ],
  "window.minimize": [
    "window.control"
  ],
  "window.maximize": [
    "window.control"
  ],
  "window.setSize": [
    "window.control"
  ],
  "window.close": [
    "window.control"
  ],
  "shell.openExternal": [
    "shell.open"
  ],
  "http.download": [
    "http.download"
  ],
  "sql.drivers": [
    "sql"
  ],
  "sql.open": [
    "sql"
  ],
  "sql.query": [
    "sql"
  ],
  "sql.execute": [
    "sql"
  ],
  "sql.close": [
    "sql"
  ],
  "path.dirs": [],
  "path.join": [],
  "path.normalize": [],
  "path.dirname": [],
  "path.basename": [],
  "path.extname": [],
  "path.isAbsolute": [],
  "store.get": [
    "store"
  ],
  "store.set": [
    "store"
  ],
  "store.delete": [
    "store"
  ],
  "store.clear": [
    "store"
  ],
  "store.keys": [
    "store"
  ],
  "os.getInfo": [
    "os.info"
  ],
  "log.write": [
    "log.write"
  ],
  "app.exit": [
    "window.control"
  ]
};

export type KSMCommandName = (typeof KSM_COMMANDS)[number];
export type KSMEventName = (typeof KSM_EVENTS)[number];
export type KSMPermission = (typeof KSM_PERMISSIONS)[number];
export type KSMPluginName = (typeof KSM_PLUGINS)[number];

interface KSMValueSchema {
  type?: string | readonly string[];
  properties?: Readonly<Record<string, KSMValueSchema>>;
  required?: readonly string[];
  additionalProperties?: boolean;
  items?: KSMValueSchema;
  enum?: readonly unknown[];
  const?: unknown;
  minLength?: number;
  maxBytes?: number;
}

const KSM_ARG_SCHEMAS: Readonly<Record<string, KSMValueSchema>> = {
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
};

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function matchesType(value: unknown, type: string): boolean {
  switch (type) {
    case 'object':
      return isRecord(value);
    case 'array':
      return Array.isArray(value);
    case 'string':
      return typeof value === 'string';
    case 'number':
      return typeof value === 'number' && Number.isFinite(value);
    case 'integer':
      return typeof value === 'number' && Number.isInteger(value);
    case 'boolean':
      return typeof value === 'boolean';
    case 'null':
      return value === null;
    default:
      return false;
  }
}

function sameValue(left: unknown, right: unknown): boolean {
  return Object.is(left, right) || JSON.stringify(left) === JSON.stringify(right);
}

function validateValue(value: unknown, schema: KSMValueSchema): boolean {
  const rawTypes = schema.type;
  const types = typeof rawTypes === 'string'
    ? [rawTypes]
    : Array.isArray(rawTypes)
      ? rawTypes
      : [];
  if (types.length === 0 || !types.some((type) => matchesType(value, type))) {
    return false;
  }

  if (schema.enum !== undefined && !schema.enum.some((candidate) => sameValue(candidate, value))) {
    return false;
  }
  if (schema.const !== undefined && !sameValue(schema.const, value)) {
    return false;
  }

  if (isRecord(value)) {
    const properties = schema.properties ?? {};
    for (const field of schema.required ?? []) {
      if (!Object.prototype.hasOwnProperty.call(value, field)) {
        return false;
      }
    }
    if (schema.additionalProperties === false) {
      for (const field of Object.keys(value)) {
        if (!Object.prototype.hasOwnProperty.call(properties, field)) {
          return false;
        }
      }
    }
    for (const [field, propertySchema] of Object.entries(properties)) {
      if (
        Object.prototype.hasOwnProperty.call(value, field) &&
        !validateValue(value[field], propertySchema)
      ) {
        return false;
      }
    }
  }

  if (Array.isArray(value) && schema.items) {
    if (!value.every((item) => validateValue(item, schema.items as KSMValueSchema))) {
      return false;
    }
  }

  if (typeof value === 'string') {
    if (schema.minLength !== undefined && value.length < schema.minLength) {
      return false;
    }
    if (
      schema.maxBytes !== undefined &&
      new TextEncoder().encode(value).byteLength > schema.maxBytes
    ) {
      return false;
    }
  }

  return true;
}

export function validateKSMArgs(command: string, args: unknown): boolean {
  const schema = KSM_ARG_SCHEMAS[command];
  return schema !== undefined && validateValue(args, schema);
}
