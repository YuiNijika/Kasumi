const VALUE_OPTIONS = new Set([
  'template',
  'package-manager',
  'kasumi-version',
  'release-base-url'
]);

const BOOLEAN_OPTIONS = new Set([
  'windows',
  'android',
  'all',
  'no-install',
  'no-sdk',
  'force',
  'yes',
  'help',
  'version'
]);

export function parseArgs(argv) {
  const options = {
    directory: undefined,
    template: 'basic',
    packageManager: 'pnpm',
    kasumiVersion: undefined,
    releaseBaseUrl: undefined,
    platforms: new Set(),
    install: true,
    sdk: true,
    force: false,
    yes: false,
    help: false,
    version: false
  };

  for (let index = 0; index < argv.length; index += 1) {
    const argument = argv[index];
    if (!argument.startsWith('-')) {
      if (options.directory !== undefined) {
        throw new Error(`Unexpected argument: ${argument}`);
      }
      options.directory = argument;
      continue;
    }

    const separator = argument.indexOf('=');
    const name = shortFlagName(argument) ?? argument.slice(
      argument.startsWith('--') ? 2 : 1,
      separator === -1 ? undefined : separator
    );
    if (VALUE_OPTIONS.has(name)) {
      const value = separator === -1 ? argv[++index] : argument.slice(separator + 1);
      if (value === undefined || value.startsWith('--')) {
        throw new Error(`--${name} requires a value.`);
      }
      applyValueOption(options, name, value);
      continue;
    }
    if (!BOOLEAN_OPTIONS.has(name) || separator !== -1) {
      throw new Error(`Unknown option: ${argument}`);
    }
    applyBooleanOption(options, name);
  }

  if (options.platforms.size === 0) {
    options.platforms.add('windows');
    options.platforms.add('android');
  }
  return options;
}

function shortFlagName(argument) {
  if (argument === '-h') {
    return 'help';
  }
  if (argument === '-v') {
    return 'version';
  }
  return undefined;
}

function applyValueOption(options, name, value) {
  switch (name) {
    case 'template':
      options.template = value;
      return;
    case 'package-manager':
      options.packageManager = value;
      return;
    case 'kasumi-version':
      options.kasumiVersion = value;
      return;
    case 'release-base-url':
      options.releaseBaseUrl = value;
      return;
    default:
      throw new Error(`Unknown option: --${name}`);
  }
}

function applyBooleanOption(options, name) {
  switch (name) {
    case 'windows':
      options.platforms.add('windows');
      return;
    case 'android':
      options.platforms.add('android');
      return;
    case 'all':
      options.platforms.add('windows');
      options.platforms.add('android');
      return;
    case 'no-install':
      options.install = false;
      return;
    case 'no-sdk':
      options.sdk = false;
      return;
    case 'force':
      options.force = true;
      return;
    case 'yes':
      options.yes = true;
      return;
    case 'help':
      options.help = true;
      return;
    case 'version':
      options.version = true;
      return;
    default:
      throw new Error(`Unknown option: --${name}`);
  }
}
