import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { parseArgs } from './args.mjs';
import { readConfig } from './config.mjs';
import {
  buildAndroid,
  findGradle,
  packageAndroid,
  runAndroidDevelopment
} from './android.mjs';
import {
  buildFrontend,
  readFrontendSettings,
  startFrontendServer
} from './frontend.mjs';
import {
  beginDevelopmentSession,
  cleanupStaleDevelopment,
  recordDevelopmentProcess
} from './dev-runtime.mjs';
import {
  buildWindows,
  findCMake,
  findVcpkgToolchain,
  packageWindows,
  runWindowsDevelopment,
  supportsWindowsDevMode
} from './windows.mjs';
import {
  exists,
  findCommand,
  run as runCommand
} from './process.mjs';

const sourceDirectory = path.dirname(fileURLToPath(import.meta.url));
const packageJsonPath = path.join(sourceDirectory, '..', 'package.json');

export async function run(argv) {
  const options = parseArgs(argv);
  if (options.version) {
    const packageJson = JSON.parse(await readFile(packageJsonPath, 'utf8'));
    console.log(packageJson.version);
    return;
  }
  if (options.help) {
    printHelp();
    return;
  }

  const projectRoot = await findProjectRoot(process.cwd());
  const config = await readConfig(projectRoot, options.config);
  if (options.command === 'doctor') {
    await doctor(projectRoot, config);
    return;
  }
  if (options.command === 'dev') {
    if (options.platforms.size !== 1) {
      throw new Error('dev requires exactly one platform.');
    }
    await beginDevelopmentSession(projectRoot);
    const server = await startFrontendServer(
      projectRoot,
      config.android.webview
    );
    await recordDevelopmentProcess(
      projectRoot,
      'frontend',
      server.child.pid
    );
    let isCleaned = false;
    const cleanup = async () => {
      if (isCleaned) {
        return;
      }
      isCleaned = true;
      server.stop();
      await cleanupStaleDevelopment(projectRoot);
    };
    const cleanupOnSignal = () => {
      void cleanup();
    };
    process.once('SIGINT', cleanupOnSignal);
    process.once('SIGTERM', cleanupOnSignal);
    try {
      if (options.platforms.has('windows')) {
        const hasWindowsDevMode = await supportsWindowsDevMode(projectRoot);
        const hasFrontendBuild = await exists(
          path.join(projectRoot, 'frontend', 'dist', 'index.html')
        );
        if (!hasWindowsDevMode && !hasFrontendBuild) {
          await buildFrontend(projectRoot);
        }
        await buildWindows(projectRoot, config.windows, {
          ...options,
          devMode: true
        });
        await runWindowsDevelopment(
          projectRoot,
          config.windows,
          server,
          {
            onSpawn(child) {
              void recordDevelopmentProcess(
                projectRoot,
                'app',
                child.pid
              ).catch((error) => {
                console.error(
                  `Could not track the development app: ${error.message}`
                );
              });
            }
          }
        );
      } else {
        await runAndroidDevelopment(projectRoot, config.android, server);
      }
    } finally {
      process.removeListener('SIGINT', cleanupOnSignal);
      process.removeListener('SIGTERM', cleanupOnSignal);
      await cleanup();
    }
    return;
  }

  await buildFrontend(projectRoot);
  if (options.command === 'package') {
    if (options.platforms.has('windows')) {
      const output = await packageWindows(projectRoot, config.windows, options);
      console.log(`Windows package: ${output}`);
    }
    if (options.platforms.has('android')) {
      const output = await packageAndroid(projectRoot, config.android);
      console.log(`Android package: ${output}`);
    }
    return;
  }
  if (options.platforms.has('windows')) {
    const output = await buildWindows(projectRoot, config.windows, options);
    console.log(`Windows application: ${output}`);
  }
  if (options.platforms.has('android')) {
    const output = await buildAndroid(projectRoot, config.android);
    console.log(`Android application: ${output}`);
  }
  if (options.command === 'test' && options.platforms.has('windows')) {
    const ctest = findCommand('ctest');
    if (!ctest) {
      throw new Error('CTest is required for Windows tests.');
    }
    await runCommand(
      ctest,
      [
        '--test-dir',
        path.join(projectRoot, '.ksm', 'build', 'windows'),
        '-C',
        'Release',
        '--output-on-failure'
      ],
      { cwd: projectRoot }
    );
  }
}

export async function collectDoctorChecks(projectRoot, config) {
  const checks = [];
  checks.push(check('Node.js', process.versions.node, true));
  const frontendSettings = await readFrontendSettings(
    projectRoot,
    config.android.webview
  ).catch(() => undefined);
  const frontendEntry = frontendSettings?.kind === 'legacy-vite'
    ? path.join(projectRoot, 'frontend', 'index.html')
    : path.join(projectRoot, 'frontend');
  checks.push(
    check(
      'frontend entry',
      frontendEntry,
      await exists(frontendEntry)
    )
  );
  checks.push(
    check(
      'Windows SDK',
      path.join(projectRoot, 'vendor', 'windows', 'kasumi'),
      await exists(
        path.join(
          projectRoot,
          'vendor',
          'windows',
          'kasumi',
          'lib',
          'cmake',
          'Kasumi',
          'KasumiConfig.cmake'
        )
      )
    )
  );
  checks.push(
    check(
      'Android Maven repository',
      path.join(projectRoot, 'vendor', 'android', 'repo'),
      await exists(path.join(projectRoot, 'vendor', 'android', 'repo'))
    )
  );
  const cmake = await findCMake();
  checks.push(check('CMake', cmake ?? 'not found', Boolean(cmake)));
  const gradle = await findGradle(projectRoot).catch(() => undefined);
  checks.push(
    check(
      'Gradle',
      gradle ? [...gradle.prefix, gradle.command].join(' ') : 'not found',
      Boolean(gradle)
    )
  );
  const adb = findCommand('adb') ?? (await findAndroidAdb());
  checks.push(check('adb', adb ?? 'not found', Boolean(adb)));
  const toolchain = await findVcpkgToolchain();
  checks.push(
    check(
      'vcpkg toolchain',
      toolchain ??
        'not found; install the Visual Studio vcpkg component or set VCPKG_ROOT',
      Boolean(toolchain)
    )
  );
  checks.push(check('Windows package', config.windows.packageName, true));
  checks.push(check('Android package', config.android.packageName, true));
  const signingConfigured = [
    'KASUMI_ANDROID_KEYSTORE',
    'KASUMI_ANDROID_KEYSTORE_PASSWORD',
    'KASUMI_ANDROID_KEY_ALIAS',
    'KASUMI_ANDROID_KEY_PASSWORD'
  ].every((name) => Boolean(process.env[name]));
  checks.push(
    check(
      'Android release signing',
      signingConfigured
        ? 'configured'
        : 'not configured (optional for build; required for package --android)',
      true
    )
  );
  return checks;
}

async function doctor(projectRoot, config) {
  const checks = await collectDoctorChecks(projectRoot, config);
  for (const item of checks) {
    console.log(`${item.ok ? 'ok' : 'missing'}  ${item.label}: ${item.value}`);
  }
  const required = checks.filter((item) =>
    ['frontend entry', 'Windows package', 'Android package', 'Node.js'].includes(item.label)
  );
  if (required.some((item) => !item.ok)) {
    throw new Error('Configuration check failed.');
  }
}

function check(label, value, ok) {
  return { label, value, ok };
}

async function findAndroidAdb() {
  const executable = process.platform === 'win32' ? 'adb.exe' : 'adb';
  const roots = [process.env.ANDROID_HOME, process.env.ANDROID_SDK_ROOT].filter(
    Boolean
  );
  for (const root of roots) {
    const candidate = path.join(root, 'platform-tools', executable);
    if (await exists(candidate)) {
      return candidate;
    }
  }
  return undefined;
}

async function findProjectRoot(start) {
  let current = path.resolve(start);
  while (true) {
    if (
      (await exists(path.join(current, 'package.json'))) &&
      (await exists(path.join(current, 'kasumi.config.json')))
    ) {
      return current;
    }
    const parent = path.dirname(current);
    if (parent === current) {
      throw new Error('Unable to find a Kasumi project root.');
    }
    current = parent;
  }
}

function printHelp() {
  console.log(`Usage: pnpm ksm <command> [options]

Commands:
  dev       Build and run one platform with the frontend development server
  build     Build one or both platforms
  package   Build distributable application files
  test      Build and run the Windows smoke test
  doctor    Check project configuration and local prerequisites

Options:
  --windows                  Select Windows, default for build and dev
  --android                  Select Android
  --all                      Select both platforms
  --platform <name>          windows, android, or all
  --config <path>            Configuration file, default kasumi.config.json
  --external-web             Copy Windows web assets beside the executable
  --with-webview2-loader     Bundle WebView2Loader.dll
  --installer <type>         Windows installer type: nsis or msi
  -h, --help                 Show this help
  -v, --version              Show the kasumi-cli version`);
}
