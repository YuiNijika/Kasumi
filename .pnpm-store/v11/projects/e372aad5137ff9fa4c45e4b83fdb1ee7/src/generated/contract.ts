export const KSM_COMMANDS = [
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
  "window.close",
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
  "window.close",
  "app.exit"
] as const;
export const KSM_ANDROID_COMMANDS = [
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
  "window.close",
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
  "window.control"
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
  "INTERNAL_ERROR"
] as const;

export type KSMCommandName = (typeof KSM_COMMANDS)[number];
export type KSMEventName = (typeof KSM_EVENTS)[number];
export type KSMPermission = (typeof KSM_PERMISSIONS)[number];

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
  "window.close": {
    "type": "object",
    "properties": {},
    "required": [],
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
