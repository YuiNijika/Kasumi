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
      true
    ),
    android: parsePlatform(
      payload.android,
      'configuration.android',
      ANDROID_PACKAGE_PATTERN,
      commandData.android
    )
  };
}

function parsePlatform(
  value,
  location,
  packagePattern,
  availableCommands,
  allowDecorations = false
) {
  if (!isObject(value)) {
    throw new ConfigError(`${location} must be an object.`);
  }
  const allowedFields = new Set(['packageName', 'version', 'commands']);
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
  const seen = new Set(commands);
  for (const required of ['ping', 'capabilities.query']) {
    if (!seen.has(required)) {
      throw new ConfigError(`${location}.commands must include core command: ${required}.`);
    }
  }
  const result = {
    packageName: value.packageName,
    version: value.version,
    commands
  };
  if (allowDecorations) {
    result.decorations = value.decorations ?? true;
  }
  return result;
}

function parseCommands(value, location, availableCommands) {
  if (value === 'all') {
    return [...availableCommands];
  }
  if (!Array.isArray(value) || value.length === 0) {
    throw new ConfigError(
      `${location}.commands must be "all" or a non-empty array.`
    );
  }
  const seen = new Set();
  for (const [index, command] of value.entries()) {
    if (typeof command !== 'string' || !COMMAND_PATTERN.test(command)) {
      throw new ConfigError(`${location}.commands[${index}] has an invalid format.`);
    }
    if (seen.has(command)) {
      throw new ConfigError(`${location}.commands contains a duplicate: ${command}.`);
    }
    if (!availableCommands.includes(command)) {
      throw new ConfigError(
        `${location}.commands contains a command that is not available on this platform: ${command}.`
      );
    }
    seen.add(command);
  }
  return [...value];
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
