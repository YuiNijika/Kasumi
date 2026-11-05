import { KSM_COMMANDS, KSM_EVENTS } from '../generated/contract.js';
import {
  assertBoolean,
  assertBoundedString,
  assertJsonValue,
  assertPlainObject,
  assertString,
  assertStringArray
} from '../api/guards.js';
import type {
  KSMExtensionCommand,
  KSMExtensionContract,
  KSMExtensionEvent,
  KSMExtensionSchema,
  KSMExtensionSchemaType,
  KSMJsonValue,
  KSMPlatform
} from './types.js';
import { KSM_PLANNED_COMMANDS, KSM_PLANNED_EVENTS } from './planned.js';

const EXTENSION_ID = /^[A-Za-z0-9_-]+$/;
const EXTENSION_NAME = /^app\.[A-Za-z0-9_-]+(?:\.[A-Za-z0-9_-]+)+$/;
const PERMISSION_NAME = /^[A-Za-z0-9][A-Za-z0-9._-]*$/;
const PLATFORMS = new Set<KSMPlatform>(['windows', 'android']);
const SCHEMA_TYPES = new Set<KSMExtensionSchemaType>([
  'object',
  'array',
  'string',
  'number',
  'integer',
  'boolean',
  'null'
]);
const RESERVED_NAME_PREFIXES = new Set([
  'core',
  'ksm',
  'event',
  'app.deepLink',
  'app.secondInstance',
  'app.singleInstance'
]);

const KNOWN_COMMANDS = new Set<string>([
  ...KSM_COMMANDS,
  ...KSM_PLANNED_COMMANDS
]);
const KNOWN_EVENTS = new Set<string>([
  ...KSM_EVENTS,
  ...KSM_PLANNED_EVENTS
]);

export interface KSMExtensionConflictSet {
  ids: ReadonlySet<string>;
  commands: ReadonlySet<string>;
  events: ReadonlySet<string>;
  excludeId?: string;
}

function fail(message: string): never {
  throw new TypeError(message);
}

function conflict(message: string): never {
  const error = new Error(message);
  error.name = 'EXTENSION_CONFLICT';
  throw error;
}

function assertAllowedKeys(
  value: Record<string, unknown>,
  field: string,
  allowed: readonly string[]
): void {
  const allowedKeys = new Set(allowed);
  for (const key of Object.keys(value)) {
    if (!allowedKeys.has(key)) {
      fail(`${field}.${key} is not allowed.`);
    }
  }
}

function assertName(value: unknown, field: string): asserts value is string {
  assertBoundedString(value, field, 256);
  if (!EXTENSION_NAME.test(value)) {
    fail(`${field} must use app.<domain>.<action>.`);
  }
  const firstSegment = value.split('.')[0] ?? '';
  if (RESERVED_NAME_PREFIXES.has(firstSegment)) {
    fail(`${field} uses a reserved prefix.`);
  }
  for (const prefix of RESERVED_NAME_PREFIXES) {
    if (value === prefix || value.startsWith(`${prefix}.`)) {
      fail(`${field} uses a reserved prefix.`);
    }
  }
  for (const known of [...KNOWN_COMMANDS, ...KNOWN_EVENTS]) {
    const knownPrefix = `${known.split('.')[0]}.`;
    if (knownPrefix !== 'app.' && value.startsWith(knownPrefix)) {
      fail(`${field} uses a reserved protocol namespace.`);
    }
  }
}

function assertPlatforms(value: unknown, field: string): asserts value is KSMPlatform[] {
  if (!Array.isArray(value) || value.length === 0) {
    fail(`${field} must contain at least one platform.`);
  }
  const seen = new Set<string>();
  for (const [index, platform] of value.entries()) {
    assertString(platform, `${field}[${index}]`);
    if (!PLATFORMS.has(platform as KSMPlatform)) {
      fail(`${field}[${index}] must be windows or android.`);
    }
    if (seen.has(platform)) {
      fail(`${field} contains a duplicate platform.`);
    }
    seen.add(platform);
  }
}

function assertPermissions(value: unknown, field: string): asserts value is string[] {
  assertStringArray(value, field, 64);
  if (value.length === 0) {
    fail(`${field} must contain at least one permission.`);
  }
  const seen = new Set<string>();
  for (const permission of value) {
    if (!PERMISSION_NAME.test(permission)) {
      fail(`${field} contains an invalid permission name.`);
    }
    if (seen.has(permission)) {
      fail(`${field} contains a duplicate permission.`);
    }
    seen.add(permission);
  }
}

function assertRequired(
  value: unknown,
  field: string,
  properties: Record<string, KSMExtensionSchema>
): asserts value is string[] {
  assertStringArray(value, field, 64);
  for (const name of value) {
    if (!Object.prototype.hasOwnProperty.call(properties, name)) {
      fail(`${field}.${name} is not declared in properties.`);
    }
  }
}

function assertSchemaProperties(
  value: unknown,
  field: string
): asserts value is Record<string, KSMExtensionSchema> {
  assertPlainObject(value, field);
  if (Object.keys(value).length > 64) {
    fail(`${field} must contain at most 64 properties.`);
  }
  for (const [key, schema] of Object.entries(value)) {
    if (
      key === '__proto__' ||
      key === 'constructor' ||
      key === 'prototype'
    ) {
      fail(`${field}.${key} is forbidden.`);
    }
    assertBoundedString(key, `${field}.${key}`, 128);
    assertSchema(schema, `${field}.${key}`);
  }
}

function assertSchema(value: unknown, field: string): asserts value is KSMExtensionSchema {
  assertPlainObject(value, field);
  assertAllowedKeys(value, field, [
    'type',
    'properties',
    'required',
    'items',
    'enum',
    'additionalProperties'
  ]);
  const type = value.type;
  assertString(type, `${field}.type`);
  if (!SCHEMA_TYPES.has(type as KSMExtensionSchemaType)) {
    fail(`${field}.type is not a supported schema type.`);
  }
  const schemaType = type as KSMExtensionSchemaType;
  if (value.properties !== undefined) {
    if (schemaType !== 'object') {
      fail(`${field}.properties requires an object schema.`);
    }
    assertSchemaProperties(value.properties, `${field}.properties`);
  }
  if (value.required !== undefined) {
    if (schemaType !== 'object' || value.properties === undefined) {
      fail(`${field}.required requires object properties.`);
    }
    assertRequired(
      value.required,
      `${field}.required`,
      value.properties as Record<string, KSMExtensionSchema>
    );
  }
  if (value.items !== undefined) {
    if (schemaType !== 'array') {
      fail(`${field}.items requires an array schema.`);
    }
    assertSchema(value.items, `${field}.items`);
  }
  if (value.enum !== undefined) {
    if (!Array.isArray(value.enum) || value.enum.length === 0) {
      fail(`${field}.enum must contain at least one value.`);
    }
    for (const [index, item] of (value.enum as unknown[]).entries()) {
      assertJsonValue(item, `${field}.enum[${index}]`);
    }
  }
  if (value.additionalProperties !== undefined) {
    if (schemaType !== 'object') {
      fail(`${field}.additionalProperties requires an object schema.`);
    }
    assertBoolean(value.additionalProperties, `${field}.additionalProperties`);
  }
}

function assertCommand(
  value: unknown,
  field: string
): asserts value is KSMExtensionCommand {
  assertPlainObject(value, field);
  assertAllowedKeys(value, field, [
    'name',
    'platforms',
    'permissions',
    'args',
    'result'
  ]);
  assertName(value.name, `${field}.name`);
  assertPlatforms(value.platforms, `${field}.platforms`);
  assertPermissions(value.permissions, `${field}.permissions`);
  assertSchema(value.args, `${field}.args`);
  assertSchema(value.result, `${field}.result`);
}

function assertEvent(
  value: unknown,
  field: string
): asserts value is KSMExtensionEvent {
  assertPlainObject(value, field);
  assertAllowedKeys(value, field, [
    'name',
    'platforms',
    'payload'
  ]);
  assertName(value.name, `${field}.name`);
  assertPlatforms(value.platforms, `${field}.platforms`);
  assertSchema(value.payload, `${field}.payload`);
}

export function validateExtensionContract(
  value: unknown,
  existing?: KSMExtensionConflictSet
): KSMExtensionContract {
  assertPlainObject(value, 'contract');
  assertAllowedKeys(value, 'contract', ['id', 'commands', 'events']);
  const id = value.id;
  assertBoundedString(id, 'contract.id', 128);
  if (!EXTENSION_ID.test(id)) {
    fail('contract.id contains a forbidden character.');
  }
  if (existing?.ids.has(id) && id !== existing.excludeId) {
    conflict(`Extension ${id} is already configured.`);
  }
  if (!Array.isArray(value.commands) || !Array.isArray(value.events)) {
    fail('contract.commands and contract.events must be arrays.');
  }
  const commandNames = new Set<string>();
  const eventNames = new Set<string>();
  const commands: KSMExtensionCommand[] = [];
  const events: KSMExtensionEvent[] = [];
  (value.commands as unknown[]).forEach((command, index) => {
    assertCommand(command, `contract.commands[${index}]`);
    const name = command.name;
    if (
      commandNames.has(name) ||
      KNOWN_COMMANDS.has(name) ||
      KNOWN_EVENTS.has(name) ||
      existing?.commands.has(name)
    ) {
      conflict(`Extension command ${name} conflicts with a registered name.`);
    }
    commandNames.add(name);
    commands.push(command);
  });
  (value.events as unknown[]).forEach((event, index) => {
    assertEvent(event, `contract.events[${index}]`);
    const name = event.name;
    if (
      eventNames.has(name) ||
      KNOWN_COMMANDS.has(name) ||
      KNOWN_EVENTS.has(name) ||
      commandNames.has(name) ||
      existing?.events.has(name)
    ) {
      conflict(`Extension event ${name} conflicts with a registered name.`);
    }
    eventNames.add(name);
    events.push(event);
  });
  return { id, commands, events };
}

export function matchesSchema(
  value: unknown,
  schema: KSMExtensionSchema
): boolean {
  if (schema.enum && !schema.enum.some(
    (item) => JSON.stringify(item) === JSON.stringify(value)
  )) {
    return false;
  }
  switch (schema.type) {
    case 'null':
      return value === null;
    case 'boolean':
      return typeof value === 'boolean';
    case 'string':
      return typeof value === 'string';
    case 'number':
      return typeof value === 'number' && Number.isFinite(value);
    case 'integer':
      return typeof value === 'number' && Number.isSafeInteger(value);
    case 'array':
      if (!Array.isArray(value) || !schema.items) {
        return Array.isArray(value);
      }
      return (value as unknown[]).every(
        (item) => matchesSchema(item, schema.items!)
      );
    case 'object': {
      if (typeof value !== 'object' || value === null || Array.isArray(value)) {
        return false;
      }
      const record = value as Record<string, unknown>;
      const properties = schema.properties ?? {};
      for (const required of schema.required ?? []) {
        if (!Object.prototype.hasOwnProperty.call(record, required)) {
          return false;
        }
      }
      if (schema.additionalProperties === false) {
        for (const key of Object.keys(record)) {
          if (!Object.prototype.hasOwnProperty.call(properties, key)) {
            return false;
          }
        }
      }
      for (const [key, item] of Object.entries(record)) {
        const propertySchema = properties[key];
        if (!propertySchema) {
          if (schema.additionalProperties === false) {
            return false;
          }
          continue;
        }
        if (!matchesSchema(item, propertySchema)) {
          return false;
        }
      }
      return true;
    }
    default:
      return false;
  }
}

export function extensionJsonValue(
  value: unknown,
  field: string
): KSMJsonValue {
  assertJsonValue(value, field);
  return value;
}
