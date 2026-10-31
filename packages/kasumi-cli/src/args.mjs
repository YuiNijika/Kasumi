const VALUE_OPTIONS = new Set(['config', 'platform']);
const BOOLEAN_OPTIONS = new Set([
  'windows',
  'android',
  'all',
  'external-web',
  'with-webview2-loader',
  'bundle-webview2',
  'help',
  'version'
]);
const COMMANDS = new Set(['dev', 'build', 'package', 'doctor', 'test']);

export function parseArgs(argv) {
  const command = argv[0] && !argv[0].startsWith('-') ? argv[0] : 'help';
  if (command !== 'help' && !COMMANDS.has(command)) {
    throw new Error(`Unknown command: ${command}`);
  }
  const options = {
    command,
    platforms: new Set(),
    config: 'kasumi.config.json',
    externalWeb: false,
    bundleWebview2Loader: false,
    help: command === 'help',
    version: false
  };

  const start = command === 'help' ? 0 : 1;
  for (let index = start; index < argv.length; index += 1) {
    const argument = argv[index];
    if (!argument.startsWith('-')) {
      throw new Error(`Unexpected argument: ${argument}`);
    }
    const separator = argument.indexOf('=');
    const name = shortFlagName(argument) ?? argument.slice(
      argument.startsWith('--') ? 2 : 1,
      separator === -1 ? undefined : separator
    );
    if (VALUE_OPTIONS.has(name)) {
      const value = separator === -1 ? argv[++index] : argument.slice(separator + 1);
      if (value === undefined) {
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
  if (name === 'config') {
    options.config = value;
    return;
  }
  if (name !== 'platform') {
    throw new Error(`Unknown option: --${name}`);
  }
  if (value === 'all') {
    options.platforms.add('windows');
    options.platforms.add('android');
    return;
  }
  if (value !== 'windows' && value !== 'android') {
    throw new Error(`Invalid platform: ${value}`);
  }
  options.platforms.add(value);
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
    case 'external-web':
      options.externalWeb = true;
      return;
    case 'with-webview2-loader':
    case 'bundle-webview2':
      options.bundleWebview2Loader = true;
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
