import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const sourceDirectory = path.dirname(fileURLToPath(import.meta.url));
const commandData = JSON.parse(
  await readFile(path.join(sourceDirectory, '..', 'data', 'commands.json'), 'utf8')
);
const SEMVER_PATTERN =
  /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$/;
const WINDOWS_PACKAGE_PATTERN = /^[A-Za-z0-9][A-Za-z0-9._-]*$/;
const ANDROID_PACKAGE_PATTERN = /^[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)+$/;
const COMMAND_PATTERN = /^[a-z][A-Za-z0-9]*(?:\.[A-Za-z][A-Za-z0-9]*)*$/;
const PERMISSION_PATTERN = COMMAND_PATTERN;

export class ConfigError extends Error {
  constructor(message) {
    super(message);
    this.name = 'ConfigError';
  }
}

export async function readConfig(projectRoot, configPath) {
  const absolutePath = path.resolve(projectRoot, configPath);
  let payload;
  try {
    payload = JSON.parse(await readFile(absolutePath, 'utf8'));
  } catch (error) {
    if (error?.code === 'ENOENT') {
      throw new ConfigError(`Configuration file not found: ${absolutePath}`);
    }
    if (error instanceof SyntaxError) {
      throw new ConfigError(`Configuration file is not valid JSON: ${absolutePath}`);
    }
    throw error;
  }
  if (!isObject(payload)) {
    throw new ConfigError('configuration must be an object.');
  }
  rejectUnknown(payload, new Set(['$schema', 'windows', 'android']), 'configuration');
  if (!('windows' in payload) || !('android' in payload)) {
    throw new ConfigError('configuration.windows and configuration.android are required.');
  }
  return {
    windows: parsePlatform(
      payload.windows,
      'configuration.windows',
      WINDOWS_PACKAGE_PATTERN,
      commandData.windows,
      commandData.permissions,
      true
    ),
    android: parsePlatform(
      payload.android,
      'configuration.android',
      ANDROID_PACKAGE_PATTERN,
      commandData.android,
      commandData.permissions
    )
  };
}

function parsePlatform(
  value,
  location,
  packagePattern,
  availableCommands,
  availablePermissions,
  allowDecorations = false
) {
  if (!isObject(value)) {
    throw new ConfigError(`${location} must be an object.`);
  }
  const allowedFields = new Set([
    'packageName',
    'version',
    'commands',
    'permissions',
    'shell',
    'http'
  ]);
  if (allowDecorations) {
    allowedFields.add('decorations');
  }
  rejectUnknown(value, allowedFields, location);
  for (const field of ['packageName', 'version', 'commands']) {
    if (!(field in value)) {
      throw new ConfigError(`${location}.${field} is required.`);
    }
  }
  if (typeof value.packageName !== 'string' || !packagePattern.test(value.packageName)) {
    throw new ConfigError(`${location}.packageName has an invalid format.`);
  }
  if (typeof value.version !== 'string' || !SEMVER_PATTERN.test(value.version)) {
    throw new ConfigError(`${location}.version must be a semantic version.`);
  }
  if (
    allowDecorations &&
    'decorations' in value &&
    typeof value.decorations !== 'boolean'
  ) {
    throw new ConfigError(`${location}.decorations must be a boolean.`);
  }
  const commands = parseCommands(value.commands, location, availableCommands);
  const permissions = parseRules(
    value.permissions,
    `${location}.permissions`,
    availablePermissions,
    PERMISSION_PATTERN,
    false
  );
  const seen = new Set(commands);
  for (const required of ['ping', 'capabilities.query']) {
    if (!seen.has(required)) {
      throw new ConfigError(`${location}.commands must include core command: ${required}.`);
    }
  }
  const result = {
    packageName: value.packageName,
    version: value.version,
    commands,
    permissions,
    shell: {
      allowedOrigins: parseOrigins(
        value.shell,
        `${location}.shell`,
        new Set(['http:', 'https:'])
      )
    },
    http: {
      allowedOrigins: parseOrigins(
        value.http,
        `${location}.http`,
        new Set(['https:'])
      )
    }
  };
  if (allowDecorations) {
    result.decorations = value.decorations ?? true;
  }
  return result;
}

function parseRules(value, location, available, pattern, requireNonEmpty) {
  if (value === 'all') {
    return [...available];
  }
  if (value === undefined) {
    return [];
  }
  if (!Array.isArray(value) || (requireNonEmpty && value.length === 0)) {
    throw new ConfigError(
      `${location} must be "all" or ${requireNonEmpty ? 'a non-empty array' : 'an array'}.`
    );
  }
  const seen = new Set();
  for (const [index, entry] of value.entries()) {
    if (typeof entry !== 'string' || !pattern.test(entry)) {
      throw new ConfigError(`${location}[${index}] has an invalid format.`);
    }
    if (seen.has(entry)) {
      throw new ConfigError(`${location} contains a duplicate: ${entry}.`);
    }
    if (!available.includes(entry)) {
      throw new ConfigError(`${location} contains an unknown value: ${entry}.`);
    }
    seen.add(entry);
  }
  return [...value];
}

function parseCommands(value, location, availableCommands) {
  return parseRules(
    value,
    `${location}.commands`,
    availableCommands,
    COMMAND_PATTERN,
    true
  );
}

function parseOrigins(value, location, protocols) {
  if (value === undefined) {
    return [];
  }
  if (!isObject(value)) {
    throw new ConfigError(`${location} must be an object.`);
  }
  rejectUnknown(value, new Set(['allowedOrigins']), location);
  const origins = value.allowedOrigins ?? [];
  if (!Array.isArray(origins)) {
    throw new ConfigError(`${location}.allowedOrigins must be an array.`);
  }
  const normalized = [];
  for (const [index, origin] of origins.entries()) {
    if (typeof origin !== 'string' || origin.trim() !== origin || origin === '') {
      throw new ConfigError(`${location}.allowedOrigins[${index}] is invalid.`);
    }
    let parsed;
    try {
      parsed = new URL(origin);
    } catch {
      throw new ConfigError(`${location}.allowedOrigins[${index}] is invalid.`);
    }
    if (
      !protocols.has(parsed.protocol) ||
      parsed.username ||
      parsed.password ||
      parsed.pathname !== '/' ||
      parsed.search ||
      parsed.hash
    ) {
      throw new ConfigError(`${location}.allowedOrigins[${index}] is invalid.`);
    }
    if (normalized.includes(parsed.origin)) {
      throw new ConfigError(
        `${location}.allowedOrigins contains a duplicate origin: ${parsed.origin}.`
      );
    }
    normalized.push(parsed.origin);
  }
  return normalized;
}

function rejectUnknown(value, allowed, location) {
  const unknown = Object.keys(value).filter((key) => !allowed.has(key));
  if (unknown.length > 0) {
    throw new ConfigError(`${location} contains unknown fields: ${unknown.join(', ')}.`);
  }
}

function isObject(value) {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}
