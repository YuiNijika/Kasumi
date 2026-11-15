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
const ERROR_PAGE_COLOR_PATTERN = /^#(?:[0-9A-Fa-f]{6}|[0-9A-Fa-f]{8})$/;
const ERROR_PAGE_TEXT_PATTERN = /^[^"\\;\u0000-\u001F\u007F]+$/;
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
 new Set([
   '$schema',
   'appId',
   'windows',
   'android',
   'appPlugins',
   'extensions',
   'icons',
   'resources',
   'signing',
   'bundle'
 ]),
 'configuration'
 );
  if (!('windows' in payload) || !('android' in payload)) {
    throw new ConfigError('configuration.windows and configuration.android are required.');
  }
 if (payload.appPlugins !== undefined && payload.extensions !== undefined) {
   throw new ConfigError(
     'configuration.appPlugins and configuration.extensions cannot be used together.'
   );
 }
 const appPlugins = await parseAppPlugins(
 payload.appPlugins ?? payload.extensions,
 projectRoot,
 payload.appPlugins !== undefined
   ? 'configuration.appPlugins'
   : 'configuration.extensions',
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
  const windows = parsePlatform(
    payload.windows,
    'configuration.windows',
    WINDOWS_PACKAGE_PATTERN,
    [...commandData.windows, ...appPlugins.windowsCommands],
    [...commandData.permissions, ...appPlugins.permissions],
    true,
    undefined,
    {
      platform: 'windows',
      frameworkPlugins: commandData.plugins ?? {},
      appPlugins: appPlugins.appPlugins
    }
  );
  const android = parsePlatform(
    payload.android,
    'configuration.android',
    ANDROID_PACKAGE_PATTERN,
    [...commandData.android, ...appPlugins.androidCommands],
    [...commandData.permissions, ...appPlugins.permissions],
    false,
    androidWebView,
    {
      platform: 'android',
      frameworkPlugins: commandData.plugins ?? {},
      appPlugins: appPlugins.appPlugins
    }
  );
  windows.icons = { ...root.icons, ...windows.icons };
  android.icons = { ...root.icons, ...android.icons };
  return {
    ...root,
    appPlugins: appPlugins.appPlugins,
    extensions: appPlugins.appPlugins,
    windows,
    android
  };
}

 const APP_PLUGIN_ID = /^[a-z0-9][a-z0-9-]*$/;
 const APP_PLUGIN_VERSION = /^\d+\.\d+\.\d+$/;
 const APP_PLUGIN_COMMAND = /^app\.[a-z][A-Za-z0-9]*(?:\.[A-Za-z][A-Za-z0-9]*)+$/;
 const APP_PLUGIN_EVENT = /^app\.[a-z][A-Za-z0-9]*(?:\.[A-Za-z][A-Za-z0-9]*)+$/;
 const APP_PLUGIN_PERMISSION = /^[a-z][A-Za-z0-9]*(?:\.[A-Za-z][A-Za-z0-9]*)*$/;
 const APP_PLUGIN_PLATFORMS = new Set(['windows', 'android']);

 async function parseAppPlugins(value, projectRoot, location, commandData) {
  if (value === undefined) {
    return {
      appPlugins: [],
      windowsCommands: [],
      androidCommands: [],
      permissions: []
    };
  }
  if (!Array.isArray(value)) {
    throw new ConfigError(`${location} must be an array.`);
  }
  const frameworkPlugins = commandData.plugins ?? {};
  const ids = new Set();
  const usedCommands = new Set([
    ...commandData.windows,
    ...commandData.android
  ]);
  const usedEvents = new Set(Object.keys(commandData.events ?? {}));
  const knownPermissions = new Set(commandData.permissions);
  const windowsCommands = [];
  const androidCommands = [];
  const permissions = [];
  const appPlugins = [];
  for (const [index, entry] of value.entries()) {
    const itemLocation = `${location}[${index}]`;
    if (!isObject(entry)) {
      throw new ConfigError(`${itemLocation} must be an object.`);
    }
    rejectUnknown(
      entry,
      new Set(['id', 'manifest', 'platforms', 'enabled']),
      itemLocation
    );
    if (typeof entry.id !== 'string' || !APP_PLUGIN_ID.test(entry.id)) {
      throw new ConfigError(`${itemLocation}.id has an invalid format.`);
    }
    if (ids.has(entry.id)) {
      throw new ConfigError(`${itemLocation}.id is duplicated: ${entry.id}.`);
    }
    if (Object.hasOwn(frameworkPlugins, entry.id)) {
      throw new ConfigError(
        `${itemLocation}.id conflicts with a framework plugin: ${entry.id}.`
      );
    }
    if (entry.enabled !== undefined && typeof entry.enabled !== 'boolean') {
      throw new ConfigError(`${itemLocation}.enabled must be a boolean.`);
    }
    const enabled = entry.enabled ?? true;
    ids.add(entry.id);
    const platforms = parseAppPluginPlatforms(
      entry.platforms,
      `${itemLocation}.platforms`
    );
    if (typeof entry.manifest !== 'string' || entry.manifest.length === 0) {
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
    const parsed = parseAppPluginManifest(
      manifest,
      entry.manifest,
      entry.id,
      platforms,
      usedCommands,
      usedEvents,
      knownPermissions,
      commandData
    );
    if (enabled) {
      for (const command of parsed.commands) {
        if (command.platforms.includes('windows')) {
          windowsCommands.push(command.name);
        }
        if (command.platforms.includes('android')) {
          androidCommands.push(command.name);
        }
        for (const permission of command.permissions) {
          if (
            !knownPermissions.has(permission) &&
            !permissions.includes(permission)
          ) {
            permissions.push(permission);
          }
        }
      }
    }
    appPlugins.push({
      id: entry.id,
      manifest: entry.manifest,
      platforms,
      enabled,
      ...parsed
    });
  }
  return {
    appPlugins,
    windowsCommands,
    androidCommands,
    permissions
  };
}

function parseAppPluginPlatforms(value, location) {
  if (!Array.isArray(value) || value.length === 0) {
    throw new ConfigError(`${location} must be a non-empty array.`);
  }
  const result = [];
  for (const platform of value) {
    if (typeof platform !== 'string' || !APP_PLUGIN_PLATFORMS.has(platform)) {
      throw new ConfigError(`${location} contains an invalid platform.`);
    }
    if (result.includes(platform)) {
      throw new ConfigError(`${location} contains a duplicate platform.`);
    }
    result.push(platform);
  }
  return result;
}

function parseAppPluginManifest(
  value,
  location,
  id,
  platforms,
  usedCommands,
  usedEvents,
  knownPermissions,
  commandData
) {
  if (!isObject(value)) {
    throw new ConfigError(`${location} must be an object.`);
  }
  rejectUnknown(
    value,
    new Set([
      'id',
      'name',
      'version',
      'description',
      'commands',
      'events',
      'defaultPermissions',
      'frameworkEvents'
    ]),
    location
  );
  if (value.id !== id) {
    throw new ConfigError(`${location}.id must match configuration app plugin id.`);
  }
  const name = value.name ?? id;
  const version = value.version ?? '1.0.0';
  const description = value.description ?? id;
  if (typeof name !== 'string' || name.length === 0) {
    throw new ConfigError(`${location}.name must be a non-empty string.`);
  }
  if (typeof version !== 'string' || !APP_PLUGIN_VERSION.test(version)) {
    throw new ConfigError(`${location}.version must use semantic versioning.`);
  }
  if (typeof description !== 'string' || description.length === 0) {
    throw new ConfigError(`${location}.description must be a non-empty string.`);
  }
  const commands = parseAppPluginCommands(
    value.commands,
    `${location}.commands`,
    id,
    platforms,
    usedCommands,
    knownPermissions
  );
  const events = parseAppPluginEvents(
    value.events,
    `${location}.events`,
    id,
    platforms,
    usedEvents
  );
  const commandPermissions = new Set(
    commands.flatMap((command) => command.permissions)
  );
  const defaultPermissions = parseAppPluginPermissions(
    value.defaultPermissions,
    `${location}.defaultPermissions`,
    knownPermissions,
    commandPermissions,
    id
  );
  const frameworkEvents = parseFrameworkEvents(
    value.frameworkEvents,
    `${location}.frameworkEvents`,
    platforms,
    commandData
  );
  return {
    name,
    version,
    description,
    commands,
    events,
    defaultPermissions,
    frameworkEvents
  };
}

function parseAppPluginCommands(
  value,
  location,
  id,
  platforms,
  usedCommands,
  knownPermissions
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
      !APP_PLUGIN_COMMAND.test(entry.name) ||
      !entry.name.startsWith(`app.${id}.`)
    ) {
      throw new ConfigError(
        `${itemLocation}.name must use the app.${id}.* namespace.`
      );
    }
    if (seen.has(entry.name) || usedCommands.has(entry.name)) {
      throw new ConfigError(`${itemLocation}.name conflicts: ${entry.name}.`);
    }
    seen.add(entry.name);
    usedCommands.add(entry.name);
    const declared = parseAppPluginPlatforms(
      entry.platforms,
      `${itemLocation}.platforms`
    );
    if (declared.some((platform) => !platforms.includes(platform))) {
      throw new ConfigError(
        `${itemLocation}.platforms exceeds the app plugin platform list.`
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
          !APP_PLUGIN_PERMISSION.test(permission) ||
          (!knownPermissions.has(permission) &&
            !isAppPluginPermission(permission, entry.name))
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

function parseAppPluginEvents(value, location, id, platforms, usedEvents) {
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
      !APP_PLUGIN_EVENT.test(entry.name) ||
      !entry.name.startsWith(`app.${id}.`)
    ) {
      throw new ConfigError(
        `${itemLocation}.name must use the app.${id}.* namespace.`
      );
    }
    if (seen.has(entry.name) || usedEvents.has(entry.name)) {
      throw new ConfigError(`${itemLocation}.name conflicts: ${entry.name}.`);
    }
    seen.add(entry.name);
    usedEvents.add(entry.name);
    const declared = parseAppPluginPlatforms(
      entry.platforms,
      `${itemLocation}.platforms`
    );
    if (declared.some((platform) => !platforms.includes(platform))) {
      throw new ConfigError(
        `${itemLocation}.platforms exceeds the app plugin platform list.`
      );
    }
    return {
      name: entry.name,
      platforms: declared,
      payload: entry.payload ?? {}
    };
  });
}

function isAppPluginPermission(permission, command) {
  const segments = command.split('.');
  if (segments.length < 2) {
    return false;
  }
  const namespace = `${segments[0]}.${segments[1]}`;
  return permission === namespace || permission.startsWith(`${namespace}.`);
}

function parseAppPluginPermissions(
  value,
  location,
  knownPermissions,
  commandPermissions,
  id
) {
  if (value === undefined) {
    return [];
  }
  if (!Array.isArray(value)) {
    throw new ConfigError(`${location} must be an array.`);
  }
  const permissions = [];
  for (const permission of value) {
    if (
      typeof permission !== 'string' ||
      !APP_PLUGIN_PERMISSION.test(permission) ||
      (!knownPermissions.has(permission) &&
        !permission.startsWith(`app.${id}`))
    ) {
      throw new ConfigError(`${location} contains an unknown permission.`);
    }
    if (!commandPermissions.has(permission)) {
      throw new ConfigError(
        `${location} must belong to the plugin command permissions.`
      );
    }
    if (!permissions.includes(permission)) {
      permissions.push(permission);
    }
  }
  return permissions;
}

function parseFrameworkEvents(value, location, platforms, commandData) {
  if (value === undefined) {
    return [];
  }
  if (!Array.isArray(value)) {
    throw new ConfigError(`${location} must be an array.`);
  }
  const definitions = commandData.events ?? {};
  const events = [];
  for (const event of value) {
    if (typeof event !== 'string' || !Object.hasOwn(definitions, event)) {
      throw new ConfigError(`${location} contains an unknown framework event.`);
    }
    const eventPlatforms = definitions[event].platforms ?? [];
    if (!eventPlatforms.some((platform) => platforms.includes(platform))) {
      throw new ConfigError(
        `${location} event has no platform overlap with the app plugin.`
      );
    }
    if (!events.includes(event)) {
      events.push(event);
    }
  }
  return events;
}

function parsePlatform(
  value,
  location,
  packagePattern,
  availableCommands,
  availablePermissions,
  allowDecorations = false,
  webviewConfig = undefined,
  pluginContext = undefined
) {
  if (!isObject(value)) {
    throw new ConfigError(`${location} must be an object.`);
  }
  const allowedFields = new Set([
    'packageName',
    'version',
    'commands',
    'frameworkPlugins',
    'permissions',
    'shell',
    'http',
    'icons',
    'resources',
    'signing',
    'bundle'
  ]);
  if (allowDecorations) {
    allowedFields.add('decorations');
    allowedFields.add('window');
    allowedFields.add('tray');
    allowedFields.add('errorPage');
  }
  if (webviewConfig !== undefined) {
    allowedFields.add('webview');
  }
  rejectUnknown(value, allowedFields, location);
  for (const field of ['packageName', 'version']) {
    if (!(field in value)) {
      throw new ConfigError(`${location}.${field} is required.`);
    }
  }
  if (value.commands === undefined && value.frameworkPlugins === undefined) {
    throw new ConfigError(
      `${location} must define commands or frameworkPlugins.`
    );
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
  const frameworkPluginMode = value.frameworkPlugins !== undefined;
  const enabledFrameworkPlugins = resolveFrameworkPlugins(
    value.frameworkPlugins,
    value.commands,
    `${location}.frameworkPlugins`,
    pluginContext
  );
  const enabledAppPlugins = (pluginContext?.appPlugins ?? [])
    .filter(
      (plugin) =>
        plugin.enabled !== false &&
        plugin.platforms.includes(pluginContext.platform)
    )
    .map((plugin) => plugin.id);
  let commands;
  if (frameworkPluginMode) {
    const baseCommands = pluginCommandsForPlatform(
      enabledFrameworkPlugins,
      enabledAppPlugins,
      pluginContext
    );
    if (value.commands === undefined || value.commands === 'all') {
      commands = baseCommands;
    } else {
      commands = parseCommands(value.commands, location, availableCommands);
      for (const command of commands) {
        if (!baseCommands.includes(command)) {
          throw new ConfigError(
            `${location}.commands contains a command outside enabled plugins: ${command}.`
          );
        }
      }
      for (const required of ['ping', 'capabilities.query']) {
        if (!commands.includes(required)) {
          commands.push(required);
        }
      }
    }
  } else {
    commands = parseCommands(value.commands, location, availableCommands);
  }
  const defaultPermissions = defaultPluginPermissions(
    enabledFrameworkPlugins,
    enabledAppPlugins,
    pluginContext
  );
  const permissions = parseRules(
    value.permissions ??
      (frameworkPluginMode ? defaultPermissions : undefined),
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
  const window = parseWindow(
    value.window,
    `${location}.window`,
    allowDecorations
  );
  const result = {
    packageName: value.packageName,
    version: value.version,
    commands,
    frameworkPlugins: enabledFrameworkPlugins,
    appPlugins: enabledAppPlugins,
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
    window,
    tray: parseTray(value.tray, `${location}.tray`),
    icons: parseIcons(value.icons, `${location}.icons`),
    resources: parseStringArray(value.resources, `${location}.resources`),
    signing: parseSigning(value.signing, `${location}.signing`),
    bundle: parseBundle(value.bundle, `${location}.bundle`)
  };
  if (allowDecorations) {
    result.decorations = value.decorations ?? window.decorations ?? true;
    result.errorPage = parseErrorPage(
      value.errorPage,
      `${location}.errorPage`
    );
  }
  if (webviewConfig !== undefined) {
    result.webview = webviewConfig;
  }
  return result;
}

function resolveFrameworkPlugins(value, commands, location, context) {
  const platform = context?.platform;
  const plugins = context?.frameworkPlugins ?? {};
  const platformIds = Object.keys(plugins).filter((id) =>
    (plugins[id].platforms ?? []).includes(platform)
  );
  if (value === undefined) {
    if (commands === 'all') {
      return platformIds;
    }
    const selected = platformIds.filter(
      (id) =>
        id === 'core' ||
        (plugins[id].commands ?? []).some((command) =>
          commands.includes(command)
        )
    );
    return platformIds.filter((id) => selected.includes(id));
  }
  if (value === 'all') {
    return platformIds;
  }
  if (!Array.isArray(value) || value.length === 0) {
    throw new ConfigError(
      `${location} must be "all" or a non-empty array.`
    );
  }
  const selected = new Set(['core']);
  for (const [index, id] of value.entries()) {
    if (typeof id !== 'string' || !/^[A-Za-z][A-Za-z0-9]*$/.test(id)) {
      throw new ConfigError(`${location}[${index}] has an invalid format.`);
    }
    if (selected.has(id) && id !== 'core') {
      throw new ConfigError(`${location} contains a duplicate: ${id}.`);
    }
    if (!Object.hasOwn(plugins, id)) {
      throw new ConfigError(`${location} contains an unknown plugin: ${id}.`);
    }
    if (!(plugins[id].platforms ?? []).includes(platform)) {
      throw new ConfigError(
        `${location} contains a plugin unsupported on ${platform}: ${id}.`
      );
    }
    selected.add(id);
  }
  return platformIds.filter((id) => selected.has(id));
}

function pluginCommandsForPlatform(
  frameworkPluginIds,
  appPluginIds,
  context
) {
  const platform = context.platform;
  const commands = ['ping', 'capabilities.query'];
  for (const id of frameworkPluginIds) {
    const plugin = context.frameworkPlugins[id];
    for (const command of plugin.commands ?? []) {
      if (!commands.includes(command)) {
        commands.push(command);
      }
    }
  }
  for (const plugin of context.appPlugins ?? []) {
    if (!appPluginIds.includes(plugin.id)) {
      continue;
    }
    for (const command of plugin.commands ?? []) {
      if (
        command.platforms.includes(platform) &&
        !commands.includes(command.name)
      ) {
        commands.push(command.name);
      }
    }
  }
  return commands;
}

function defaultPluginPermissions(
  frameworkPluginIds,
  appPluginIds,
  context
) {
  const permissions = [];
  for (const id of frameworkPluginIds) {
    const plugin = context.frameworkPlugins[id];
    for (const permission of plugin.defaultPermissions ?? []) {
      if (!permissions.includes(permission)) {
        permissions.push(permission);
      }
    }
  }
  for (const plugin of context.appPlugins ?? []) {
    if (!appPluginIds.includes(plugin.id)) {
      continue;
    }
    for (const permission of plugin.defaultPermissions ?? []) {
      if (!permissions.includes(permission)) {
        permissions.push(permission);
      }
    }
  }
  return permissions;
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

function parseErrorPage(value, location) {
  if (value === undefined) {
    return {};
  }
  if (!isObject(value)) {
    throw new ConfigError(`${location} must be an object.`);
  }
  const colorFields = [
    'backgroundColor',
    'cardColor',
    'borderColor',
    'textColor',
    'mutedTextColor',
    'accentColor',
    'errorColor'
  ];
  const textFields = {
    title: 80,
    hint: 240
  };
  rejectUnknown(
    value,
    new Set([...colorFields, ...Object.keys(textFields)]),
    location
  );
  const result = {};
  for (const key of colorFields) {
    if (!(key in value)) {
      continue;
    }
    if (
      typeof value[key] !== 'string' ||
      !ERROR_PAGE_COLOR_PATTERN.test(value[key])
    ) {
      throw new ConfigError(
        `${location}.${key} must be #RRGGBB or #AARRGGBB.`
      );
    }
    result[key] = value[key];
  }
  for (const [key, maxLength] of Object.entries(textFields)) {
    if (!(key in value)) {
      continue;
    }
    if (
      typeof value[key] !== 'string' ||
      value[key].length === 0 ||
      value[key].length > maxLength ||
      !ERROR_PAGE_TEXT_PATTERN.test(value[key])
    ) {
      throw new ConfigError(
        `${location}.${key} must be 1-${maxLength} safe characters.`
      );
    }
    result[key] = value[key];
  }
  return result;
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
      'backgroundColor',
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
        backgroundColor: '',
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
  if (
    typeof result.backgroundColor !== 'string' ||
    (
      result.backgroundColor !== '' &&
      !/^#(?:[0-9A-Fa-f]{6}|[0-9A-Fa-f]{8})$/.test(result.backgroundColor)
    )
  ) {
    throw new ConfigError(
      `${location}.backgroundColor must be #RRGGBB or #AARRGGBB.`
    );
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
  if (result.maxWidth > 0 && result.maxWidth < result.minWidth) {
    throw new ConfigError(
      `${location}.maxWidth must be greater than or equal to minWidth.`
    );
  }
  if (result.maxHeight > 0 && result.maxHeight < result.minHeight) {
    throw new ConfigError(
      `${location}.maxHeight must be greater than or equal to minHeight.`
    );
  }
  if (result.maxWidth > 0 && result.width > result.maxWidth) {
    throw new ConfigError(
      `${location}.width must be less than or equal to maxWidth.`
    );
  }
  if (result.maxHeight > 0 && result.height > result.maxHeight) {
    throw new ConfigError(
      `${location}.height must be less than or equal to maxHeight.`
    );
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
  rejectUnknown(
    value,
    new Set(['window', 'tray', 'installer', 'android']),
    location
  );
  const result = {};
  for (const key of ['window', 'tray', 'installer', 'android']) {
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
