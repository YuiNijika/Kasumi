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
const DEVELOPMENT_ORIGIN = 'http://localhost:5173';
const DEFAULT_WEBVIEW_CONFIG = Object.freeze({
  productionEntry: 'https://app.local/index.html',
  developmentEntries: [
    `${DEVELOPMENT_ORIGIN}/index.html`,
    `${DEVELOPMENT_ORIGIN}/`,
    DEVELOPMENT_ORIGIN
  ],
  supportedHtmlFiles: ['index.html']
});

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
 rejectUnknown(
 payload,
 new Set(['$schema', 'windows', 'android', 'extensions']),
 'configuration'
 );
  if (!('windows' in payload) || !('android' in payload)) {
    throw new ConfigError('configuration.windows and configuration.android are required.');
  }
 const extensions = await parseExtensions(
 payload.extensions,
 projectRoot,
 'configuration.extensions',
 commandData
 );
 const root = {
    appId: parseOptionalString(payload.appId, 'configuration.appId')
  };
  const androidWebView = isObject(payload.android)
    ? parseWebView(
        payload.android.webview,
        'configuration.android.webview'
      )
    : {
        productionEntry: DEFAULT_WEBVIEW_CONFIG.productionEntry,
        developmentEntries: [...DEFAULT_WEBVIEW_CONFIG.developmentEntries],
        supportedHtmlFiles: [...DEFAULT_WEBVIEW_CONFIG.supportedHtmlFiles]
      };
  for (const [key, parser] of [
    ['icons', parseIcons],
    ['resources', parseStringArray],
    ['signing', parseSigning],
    ['bundle', parseBundle]
  ]) {
    root[key] = parser(payload[key], `configuration.${key}`);
  }
  return {
    ...root,
 extensions: extensions.extensions,
 windows: parsePlatform(
      payload.windows,
      'configuration.windows',
      WINDOWS_PACKAGE_PATTERN,
 [...commandData.windows, ...extensions.windowsCommands],
 [...commandData.permissions, ...extensions.permissions],
      true
    ),
    android: parsePlatform(
      payload.android,
      'configuration.android',
      ANDROID_PACKAGE_PATTERN,
 [...commandData.android, ...extensions.androidCommands],
 [...commandData.permissions, ...extensions.permissions],
      false,
      androidWebView
    )
  };
}

 const EXTENSION_ID = /^[a-z0-9][a-z0-9-]*$/;
 const EXTENSION_COMMAND = /^app\.[a-z][A-Za-z0-9]*(?:\.[A-Za-z][A-Za-z0-9]*)+$/;
 const EXTENSION_EVENT = /^app\.[a-z][A-Za-z0-9]*(?:\.[A-Za-z][A-Za-z0-9]*)+$/;
 const EXTENSION_PERMISSION = /^[a-z][A-Za-z0-9]*(?:\.[A-Za-z][A-Za-z0-9]*)*$/;
 const EXTENSION_PLATFORMS = new Set(['windows', 'android']);
 const RESERVED_EXTENSION_PREFIXES = ['core', 'ksm'];

 async function parseExtensions(value, projectRoot, location, commandData) {
  if (value === undefined) {
  return {
  extensions: [],
  windowsCommands: [],
  androidCommands: [],
  permissions: []
  };
  }
  if (!Array.isArray(value)) {
  throw new ConfigError(`${location} must be an array.`);
  }
  const ids = new Set();
  const commands = new Set([
  ...commandData.windows,
  ...commandData.android
  ]);
  const knownPermissions = new Set(commandData.permissions);
  const windowsCommands = [];
  const androidCommands = [];
  const permissions = [];
  const extensions = [];
  for (const [index, entry] of value.entries()) {
  const itemLocation = `${location}[${index}]`;
  if (!isObject(entry)) {
  throw new ConfigError(`${itemLocation} must be an object.`);
  }
  rejectUnknown(
  entry,
  new Set(['id', 'manifest', 'platforms']),
  itemLocation
  );
  if (typeof entry.id !== 'string' || !EXTENSION_ID.test(entry.id)) {
  throw new ConfigError(`${itemLocation}.id has an invalid format.`);
  }
  if (ids.has(entry.id)) {
  throw new ConfigError(`${itemLocation}.id is duplicated: ${entry.id}.`);
  }
  ids.add(entry.id);
  const platforms = parseExtensionPlatforms(
  entry.platforms,
  `${itemLocation}.platforms`
  );
  if (typeof entry.manifest !== 'string' || entry.manifest.length ===0) {
  throw new ConfigError(`${itemLocation}.manifest must be a non-empty string.`);
  }
  const manifestPath = path.resolve(projectRoot, entry.manifest);
  const relativeManifest = path.relative(projectRoot, manifestPath);
  if (
  relativeManifest.startsWith('..') ||
  path.isAbsolute(relativeManifest)
  ) {
  throw new ConfigError(`${itemLocation}.manifest must stay inside the project.`);
  }
  let manifest;
  try {
  manifest = JSON.parse(await readFile(manifestPath, 'utf8'));
  } catch (error) {
  if (error?.code === 'ENOENT') {
  throw new ConfigError(`${itemLocation}.manifest was not found.`);
  }
  if (error instanceof SyntaxError) {
  throw new ConfigError(`${itemLocation}.manifest is not valid JSON.`);
  }
  throw error;
  }
  const parsed = parseExtensionManifest(
  manifest,
  entry.manifest,
  entry.id,
  platforms,
  commands,
  knownPermissions
  );
  for (const command of parsed.commands) {
  commands.add(command.name);
  if (command.platforms.includes('windows')) {
  windowsCommands.push(command.name);
  }
  if (command.platforms.includes('android')) {
  androidCommands.push(command.name);
  }
  for (const permission of command.permissions) {
  if (!permissions.includes(permission)) {
  permissions.push(permission);
  }
  }
  }
  for (const event of parsed.events) {
  if (
  commandData.windows.includes(event.name) ||
  commandData.android.includes(event.name)
  ) {
  throw new ConfigError(
  `${itemLocation}.manifest event conflicts with a built-in name: ${event.name}.`
  );
  }
  }
  extensions.push({
  id: entry.id,
  manifest: entry.manifest,
  platforms,
  commands: parsed.commands,
  events: parsed.events
  });
  }
 return {
 extensions,
  windowsCommands,
  androidCommands,
  permissions
  };
 }

 function parseExtensionPlatforms(value, location) {
  if (!Array.isArray(value) || value.length ===0) {
  throw new ConfigError(`${location} must be a non-empty array.`);
  }
  const result = [];
  for (const platform of value) {
  if (typeof platform !== 'string' || !EXTENSION_PLATFORMS.has(platform)) {
  throw new ConfigError(`${location} contains an invalid platform.`);
  }
  if (result.includes(platform)) {
  throw new ConfigError(`${location} contains a duplicate platform.`);
  }
  result.push(platform);
  }
  return result;
 }

 function parseExtensionManifest(
  value,
  location,
  id,
  platforms,
  usedCommands,
  knownPermissions
 ) {
  if (!isObject(value)) {
  throw new ConfigError(`${location} must be an object.`);
  }
  rejectUnknown(value, new Set(['id', 'commands', 'events']), location);
  if (value.id !== id) {
  throw new ConfigError(`${location}.id must match configuration extension id.`);
  }
  const commands = parseExtensionEntries(
  value.commands,
  `${location}.commands`,
  platforms,
  usedCommands,
  knownPermissions,
  'command'
  );
  const events = parseExtensionEvents(
  value.events,
  `${location}.events`,
  platforms
  );
  return { commands, events };
 }

 function parseExtensionEntries(
  value,
  location,
  platforms,
  usedCommands,
  knownPermissions,
  kind
 ) {
  if (value === undefined) {
  return [];
  }
  if (!Array.isArray(value)) {
  throw new ConfigError(`${location} must be an array.`);
  }
  const seen = new Set();
  return value.map((entry, index) => {
  const itemLocation = `${location}[${index}]`;
  if (!isObject(entry)) {
  throw new ConfigError(`${itemLocation} must be an object.`);
  }
  rejectUnknown(
  entry,
  new Set(['name', 'platforms', 'permissions', 'args', 'result']),
  itemLocation
  );
  if (
  typeof entry.name !== 'string' ||
  !EXTENSION_COMMAND.test(entry.name) ||
  RESERVED_EXTENSION_PREFIXES.some((prefix) =>
  entry.name.startsWith(`${prefix}.`)
  )
  ) {
  throw new ConfigError(`${itemLocation}.name has an invalid format.`);
  }
  if (seen.has(entry.name) || usedCommands.has(entry.name)) {
  throw new ConfigError(`${itemLocation}.name conflicts: ${entry.name}.`);
  }
  seen.add(entry.name);
  const declared = parseExtensionPlatforms(
  entry.platforms,
  `${itemLocation}.platforms`
  );
  if (declared.some((platform) => !platforms.includes(platform))) {
  throw new ConfigError(
  `${itemLocation}.platforms exceeds the extension platform list.`
  );
  }
  const permissions = [];
  if (entry.permissions !== undefined) {
  if (!Array.isArray(entry.permissions)) {
  throw new ConfigError(`${itemLocation}.permissions must be an array.`);
  }
  for (const permission of entry.permissions) {
  if (
  typeof permission !== 'string' ||
  !EXTENSION_PERMISSION.test(permission) ||
  !knownPermissions.has(permission)
  ) {
  throw new ConfigError(
  `${itemLocation}.permissions contains an unknown permission.`
  );
  }
  if (!permissions.includes(permission)) {
  permissions.push(permission);
  }
  }
  }
  return {
  name: entry.name,
  platforms: declared,
  permissions,
  args: entry.args ?? {},
  result: entry.result ?? {}
  };
  });
 }

 function parseExtensionEvents(value, location, platforms) {
  if (value === undefined) {
  return [];
  }
  if (!Array.isArray(value)) {
  throw new ConfigError(`${location} must be an array.`);
  }
  const seen = new Set();
  return value.map((entry, index) => {
  const itemLocation = `${location}[${index}]`;
  if (!isObject(entry)) {
  throw new ConfigError(`${itemLocation} must be an object.`);
  }
  rejectUnknown(
  entry,
  new Set(['name', 'platforms', 'payload']),
  itemLocation
  );
  if (
  typeof entry.name !== 'string' ||
  !EXTENSION_EVENT.test(entry.name) ||
  RESERVED_EXTENSION_PREFIXES.some((prefix) =>
  entry.name.startsWith(`${prefix}.`)
  )
  ) {
  throw new ConfigError(`${itemLocation}.name has an invalid format.`);
  }
  if (seen.has(entry.name)) {
  throw new ConfigError(`${itemLocation}.name is duplicated: ${entry.name}.`);
  }
  seen.add(entry.name);
  const declared = parseExtensionPlatforms(
  entry.platforms,
  `${itemLocation}.platforms`
  );
  if (declared.some((platform) => !platforms.includes(platform))) {
  throw new ConfigError(
  `${itemLocation}.platforms exceeds the extension platform list.`
  );
  }
  return {
  name: entry.name,
  platforms: declared,
  payload: entry.payload ?? {}
  };
  });
 }

function parsePlatform(
  value,
  location,
  packagePattern,
  availableCommands,
  availablePermissions,
  allowDecorations = false,
  webviewConfig = undefined
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
    allowedFields.add('window');
    allowedFields.add('tray');
  }
  if (webviewConfig !== undefined) {
    allowedFields.add('webview');
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
    },
    window: parseWindow(value.window, `${location}.window`, allowDecorations),
    tray: parseTray(value.tray, `${location}.tray`),
    icons: parseIcons(value.icons, `${location}.icons`),
    resources: parseStringArray(value.resources, `${location}.resources`),
    signing: parseSigning(value.signing, `${location}.signing`),
    bundle: parseBundle(value.bundle, `${location}.bundle`)
  };
  if (allowDecorations) {
    result.decorations = value.decorations ?? true;
  }
  if (webviewConfig !== undefined) {
    result.webview = webviewConfig;
  }
  return result;
}

function parseWebView(value, location) {
  if (value === undefined) {
    return {
      productionEntry: DEFAULT_WEBVIEW_CONFIG.productionEntry,
      developmentEntries: [...DEFAULT_WEBVIEW_CONFIG.developmentEntries],
      supportedHtmlFiles: [...DEFAULT_WEBVIEW_CONFIG.supportedHtmlFiles]
    };
  }
  if (!isObject(value)) {
    throw new ConfigError(`${location} must be an object.`);
  }
  rejectUnknown(
    value,
    new Set(['productionEntry', 'developmentEntries', 'supportedHtmlFiles']),
    location
  );
  const productionEntry = parseProductionEntry(
    value.productionEntry ?? DEFAULT_WEBVIEW_CONFIG.productionEntry,
    `${location}.productionEntry`
  );
  const supportedHtmlFiles = parseHtmlFiles(
    value.supportedHtmlFiles ?? DEFAULT_WEBVIEW_CONFIG.supportedHtmlFiles,
    `${location}.supportedHtmlFiles`
  );
  if (!supportedHtmlFiles.includes('index.html')) {
    throw new ConfigError(
      `${location}.supportedHtmlFiles must include index.html for frontend builds.`
    );
  }
  const developmentEntries = parseDevelopmentEntries(
    value.developmentEntries ?? DEFAULT_WEBVIEW_CONFIG.developmentEntries,
    `${location}.developmentEntries`,
    supportedHtmlFiles,
    productionEntry.slice(productionEntry.indexOf('/', 'https://'.length) + 1)
  );
  return { productionEntry, developmentEntries, supportedHtmlFiles };
}

function parseProductionEntry(value, location) {
  const parsed = parseEntryUrl(value, location, 'https', 'app.local', '');
  const htmlFile = parsed.path.slice(1);
  if (!htmlFile.endsWith('.html')) {
    throw new ConfigError(`${location} must point to an HTML file.`);
  }
  return parsed.url;
}

function parseDevelopmentEntries(
  value,
  location,
  supportedHtmlFiles,
  productionHtmlFile
) {
  if (!Array.isArray(value) || value.length === 0) {
    throw new ConfigError(`${location} must be a non-empty array.`);
  }
  const entries = [];
  for (const [index, entry] of value.entries()) {
    const itemLocation = `${location}[${index}]`;
    const parsed = parseEntryUrl(
      entry,
      itemLocation,
      'http',
      'localhost',
      ':5173'
    );
    const path = parsed.path.slice(1);
    if (path !== '' && !supportedHtmlFiles.includes(path)) {
      throw new ConfigError(
        `${itemLocation} is not in configuration.android.webview.supportedHtmlFiles.`
      );
    }
    if (entries.includes(parsed.url)) {
      throw new ConfigError(`${location} contains a duplicate: ${parsed.url}.`);
    }
    entries.push(parsed.url);
  }
  if (!entries.some((entry) => entry.endsWith(`/${productionHtmlFile}`))) {
    throw new ConfigError(
      `${location} must include the production HTML file: ${productionHtmlFile}.`
    );
  }
  return entries;
}

function parseEntryUrl(value, location, scheme, authority, port) {
  if (typeof value !== 'string' || value.trim() !== value || value === '') {
    throw new ConfigError(`${location} is invalid.`);
  }
  const match = new RegExp(
    `^${scheme}://([^/?#]+)([^?#]*)$`
  ).exec(value);
  if (!match || value.includes('?') || value.includes('#')) {
    throw new ConfigError(`${location} is invalid.`);
  }
  const actualAuthority = match[1];
  const path = match[2];
  if (
    actualAuthority !== `${authority}${port}` ||
    !isSafeEntryPath(path)
  ) {
    throw new ConfigError(`${location} is invalid.`);
  }
  return { url: value, path };
}

function isSafeEntryPath(path) {
  if (path === '') {
    return true;
  }
  return /^\/(?:[A-Za-z0-9._-]+\/)*[A-Za-z0-9._-]*$/.test(path) &&
    !path.includes('%') &&
    !path.includes('\\') &&
    !path.includes('\u0000');
}

function parseHtmlFiles(value, location) {
  if (!Array.isArray(value) || value.length === 0) {
    throw new ConfigError(`${location} must be a non-empty array.`);
  }
  const htmlFiles = [];
  for (const [index, entry] of value.entries()) {
    if (
      typeof entry !== 'string' ||
      !/^(?:[A-Za-z0-9._-]+\/)*[A-Za-z0-9._-]+\.html$/.test(entry) ||
      entry.includes('\\') ||
      entry.includes('\u0000')
    ) {
      throw new ConfigError(`${location}[${index}] is invalid.`);
    }
    if (htmlFiles.includes(entry)) {
      throw new ConfigError(`${location} contains a duplicate: ${entry}.`);
    }
    htmlFiles.push(entry);
  }
  return htmlFiles;
}

function parseWindow(value, location, allowWindowControl) {
  if (value === undefined) {
    return {};
  }
  if (!isObject(value)) {
    throw new ConfigError(`${location} must be an object.`);
  }
  rejectUnknown(
    value,
    new Set([
      'title',
      'decorations',
      'transparent',
      'alwaysOnTop',
      'center',
      'width',
      'height',
      'minWidth',
      'minHeight',
      'maxWidth',
      'maxHeight',
      'resizable',
      'visible'
    ]),
    location
  );
  const defaults = allowWindowControl
    ? {
        title: '',
        decorations: true,
        transparent: false,
        alwaysOnTop: false,
        center: true,
        width: 1080,
        height: 720,
        minWidth: 320,
        minHeight: 240,
        maxWidth: 0,
        maxHeight: 0,
        resizable: true,
        visible: true
      }
    : {};
  const result = {};
  for (const key of Object.keys(defaults)) {
    if (key in value) {
      result[key] = value[key];
    } else {
      result[key] = defaults[key];
    }
  }
  for (const key of [
    'title'
  ]) {
    if (typeof result[key] !== 'string') {
      throw new ConfigError(`${location}.${key} must be a string.`);
    }
  }
  for (const key of [
    'decorations',
    'transparent',
    'alwaysOnTop',
    'center',
    'resizable',
    'visible'
  ]) {
    if (typeof result[key] !== 'boolean') {
      throw new ConfigError(`${location}.${key} must be a boolean.`);
    }
  }
  for (const key of [
    'width',
    'height',
    'minWidth',
    'minHeight',
    'maxWidth',
    'maxHeight'
  ]) {
    if (!Number.isInteger(result[key]) || result[key] < 0) {
      throw new ConfigError(`${location}.${key} must be a non-negative integer.`);
    }
  }
  if (result.maxWidth === 0) {
    result.maxWidth = 0;
  }
  if (result.maxHeight === 0) {
    result.maxHeight = 0;
  }
  return result;
}

function parseTray(value, location) {
  if (value === undefined) {
    return {};
  }
  if (!isObject(value)) {
    throw new ConfigError(`${location} must be an object.`);
  }
  rejectUnknown(
    value,
    new Set(['icon', 'tooltip', 'menu', 'clickAction']),
    location
  );
  const result = {};
  for (const key of ['icon', 'tooltip', 'clickAction']) {
    if (key in value) {
      if (typeof value[key] !== 'string') {
        throw new ConfigError(`${location}.${key} must be a string.`);
      }
      result[key] = value[key];
    }
  }
  if ('menu' in value) {
    if (!Array.isArray(value.menu)) {
      throw new ConfigError(`${location}.menu must be an array.`);
    }
    const menu = [];
    for (const [index, item] of value.menu.entries()) {
      if (!isObject(item)) {
        throw new ConfigError(`${location}.menu[${index}] must be an object.`);
      }
      rejectUnknown(item, new Set(['label', 'action']), `${location}.menu[${index}]`);
      if (
        !('label' in item) ||
        typeof item.label !== 'string' ||
        !('action' in item) ||
        typeof item.action !== 'string'
      ) {
        throw new ConfigError(
          `${location}.menu[${index}] must contain label and action strings.`
        );
      }
      menu.push({ label: item.label, action: item.action });
    }
    result.menu = menu;
  }
  return result;
}

function parseIcons(value, location) {
  if (value === undefined) {
    return {};
  }
  if (!isObject(value)) {
    throw new ConfigError(`${location} must be an object.`);
  }
  rejectUnknown(value, new Set(['window', 'tray', 'installer']), location);
  const result = {};
  for (const key of ['window', 'tray', 'installer']) {
    if (key in value) {
      if (typeof value[key] !== 'string' || value[key].length === 0) {
        throw new ConfigError(`${location}.${key} must be a non-empty string.`);
      }
      result[key] = value[key];
    }
  }
  return result;
}

function parseSigning(value, location) {
  if (value === undefined) {
    return {};
  }
  if (!isObject(value)) {
    throw new ConfigError(`${location} must be an object.`);
  }
  rejectUnknown(value, new Set(['certificate', 'password']), location);
  const result = {};
  for (const key of ['certificate', 'password']) {
    if (key in value) {
      if (typeof value[key] !== 'string') {
        throw new ConfigError(`${location}.${key} must be a string.`);
      }
      result[key] = value[key];
    }
  }
  return result;
}

function parseBundle(value, location) {
  if (value === undefined) {
    return {};
  }
  if (!isObject(value)) {
    throw new ConfigError(`${location} must be an object.`);
  }
  rejectUnknown(value, new Set(['windowsInstaller']), location);
  const result = {};
  if ('windowsInstaller' in value) {
    if (value.windowsInstaller !== 'nsis' && value.windowsInstaller !== 'msi') {
      throw new ConfigError(
        `${location}.windowsInstaller must be "nsis" or "msi".`
      );
    }
    result.windowsInstaller = value.windowsInstaller;
  }
  return result;
}

function parseStringArray(value, location) {
  if (value === undefined) {
    return [];
  }
  if (!Array.isArray(value)) {
    throw new ConfigError(`${location} must be an array.`);
  }
  const result = [];
  for (const [index, entry] of value.entries()) {
    if (typeof entry !== 'string' || entry.length === 0) {
      throw new ConfigError(`${location}[${index}] must be a non-empty string.`);
    }
    result.push(entry);
  }
  return result;
}

function parseOptionalString(value, location) {
  if (value === undefined) {
    return undefined;
  }
  if (typeof value !== 'string' || value.length === 0) {
    throw new ConfigError(`${location} must be a non-empty string.`);
  }
  return value;
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
