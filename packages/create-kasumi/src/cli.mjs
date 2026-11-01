import { readFile, readdir, rm, stat } from 'node:fs/promises';
import path from 'node:path';
import { createInterface } from 'node:readline/promises';
import { fileURLToPath } from 'node:url';

import { parseArgs } from './args.mjs';
import { platformCommands } from './commands.mjs';
import { installDependencies, validatePackageManager } from './install.mjs';
import {
  androidPackageName,
  normalizeProjectName,
  projectTitle
} from './naming.mjs';
import { installSdk } from './sdk.mjs';
import { renderTemplate } from './template.mjs';

const sourceDirectory = path.dirname(fileURLToPath(import.meta.url));
const packageJsonPath = path.join(sourceDirectory, '..', 'package.json');

export async function run(argv) {
  const options = parseArgs(argv);
  if (options.help) {
    printHelp();
    return;
  }
  const packageJson = JSON.parse(await readFile(packageJsonPath, 'utf8'));
  if (options.version) {
    console.log(packageJson.version);
    return;
  }

  const requestedDirectory =
    options.directory ?? (await promptProjectName(options.yes));
  const projectName = normalizeProjectName(
    path.basename(path.resolve(requestedDirectory))
  );
  const destination = path.resolve(process.cwd(), requestedDirectory);
  const destinationExisted = await pathExists(destination);
  await assertWritableDirectory(destination, options.force);

  const packageManager = validatePackageManager(options.packageManager);
  const windowsPackageName = projectName;
  const androidPackage = androidPackageName(projectName);
  const platforms = options.platforms;
  const kasumiVersion = options.kasumiVersion ?? packageJson.version;
  const replacements = {
    __PROJECT_NAME__: projectName,
    __APP_TITLE__: projectTitle(projectName),
    __WINDOWS_PACKAGE_NAME__: windowsPackageName,
    __ANDROID_PACKAGE_NAME__: androidPackage,
    __ANDROID_PACKAGE_PATH__: androidPackage.replaceAll('.', '/'),
    __KASUMI_VERSION__: kasumiVersion,
    __ANDROID_COMMANDS_CSV__: platformCommands.android.join(',')
  };

  console.log(`Creating ${projectName} in ${destination}`);
  try {
    await renderTemplate({
      template: options.template,
      destination,
      replacements,
      force: options.force
    });
    if (options.sdk) {
      await installSdk({
        projectDirectory: destination,
        platforms,
        version: kasumiVersion,
        releaseBaseUrl: options.releaseBaseUrl
      });
    }
    if (options.install) {
      await installDependencies(destination, packageManager);
    }
  } catch (error) {
    if (!destinationExisted) {
      await rm(destination, { recursive: true, force: true });
    }
    throw error;
  }

  printNextSteps(projectName, destination, packageManager, platforms, options);
}

async function promptProjectName(skipPrompt) {
  if (skipPrompt || !process.stdin.isTTY || !process.stdout.isTTY) {
    return 'kasumi-app';
  }
  const prompt = createInterface({
    input: process.stdin,
    output: process.stdout
  });
  try {
    const answer = await prompt.question('Project name (kasumi-app): ');
    return answer.trim() || 'kasumi-app';
  } finally {
    prompt.close();
  }
}

async function assertWritableDirectory(directory, force) {
  try {
    const info = await stat(directory);
    if (!info.isDirectory()) {
      throw new Error(`Target exists and is not a directory: ${directory}`);
    }
    const entries = await readdir(directory);
    if (entries.length > 0 && !force) {
      throw new Error(
        `Target directory is not empty: ${directory}. Use --force to write into it.`
      );
    }
  } catch (error) {
    if (error?.code === 'ENOENT') {
      return;
    }
    throw error;
  }
}

async function pathExists(value) {
  try {
    await stat(value);
    return true;
  } catch {
    return false;
  }
}

function printNextSteps(
  projectName,
  destination,
  packageManager,
  platforms,
  options
) {
  const runPrefix = packageManager === 'npm' ? 'npm run' : packageManager;
  console.log('');
  console.log(`Created ${projectName}.`);
  console.log(`  cd ${path.relative(process.cwd(), destination) || '.'}`);
  if (!options.install) {
    console.log(`  ${packageManager} install`);
  }
  if (!options.sdk) {
    console.log('  Install the platform SDKs before building.');
  }
  if (platforms.has('windows')) {
    console.log(`  ${scriptCommand(runPrefix, 'dev --windows')}`);
  }
  if (platforms.has('android')) {
    console.log(`  ${scriptCommand(runPrefix, 'dev --android')}`);
  }
  console.log(`  ${scriptCommand(runPrefix, 'build --all')}`);
}

function scriptCommand(prefix, argumentsText) {
  return prefix === 'npm run'
    ? `npm run ksm -- ${argumentsText}`
    : `${prefix} ksm ${argumentsText}`;
}

function printHelp() {
  console.log(`Usage: pnpm create kasumi [directory] [options]

Options:
  --template <name>          basic, react, vue, nextjs, nuxtjs; default basic
  --windows                  Enable Windows
  --android                  Enable Android
  --all                      Enable both platforms, default
  --package-manager <name>   pnpm, npm, or yarn
  --no-install               Skip dependency installation
  --no-sdk                   Skip Windows SDK and Android Maven download
  --force                    Allow writing into a non-empty directory
  --kasumi-version <ver>     Kasumi release version, default package version
  --release-base-url <url>   Override the release download base URL
  -h, --help                 Show this help
  -v, --version              Show the create-kasumi version`);
}
