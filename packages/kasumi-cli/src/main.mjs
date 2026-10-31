import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { parseArgs } from './args.mjs';
import { readConfig } from './config.mjs';
import {
  buildAndroid,
  runAndroidDevelopment
} from './android.mjs';
import {
  buildFrontend,
  startFrontendServer
} from './frontend.mjs';
import {
  buildWindows,
  runWindowsDevelopment
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
    await buildFrontend(projectRoot);
    const server = await startFrontendServer(projectRoot);
    try {
      if (options.platforms.has('windows')) {
        await buildWindows(projectRoot, config.windows, options);
        await runWindowsDevelopment(projectRoot, config.windows, server);
      } else {
        await runAndroidDevelopment(projectRoot, config.android, server);
      }
    } finally {
      server.stop();
    }
    return;
  }

  await buildFrontend(projectRoot);
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

async function doctor(projectRoot, config) {
  const checks = [];
  checks.push(check('Node.js', process.versions.node, true));
  checks.push(
    check(
      'frontend entry',
      path.join(projectRoot, 'frontend', 'index.html'),
      await exists(path.join(projectRoot, 'frontend', 'index.html'))
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
  checks.push(check('CMake', findCommand('cmake') ?? 'not found', Boolean(findCommand('cmake'))));
  checks.push(check('Gradle', findCommand('gradle') ?? 'not found', Boolean(findCommand('gradle'))));
  checks.push(check('adb', findCommand('adb') ?? 'not found', Boolean(findCommand('adb'))));
  const toolchain =
    process.env.CMAKE_TOOLCHAIN_FILE ??
    (process.env.VCPKG_ROOT
      ? path.join(process.env.VCPKG_ROOT, 'scripts', 'buildsystems', 'vcpkg.cmake')
      : undefined);
  checks.push(check('vcpkg toolchain', toolchain ?? 'not configured', Boolean(toolchain)));
  checks.push(check('Windows package', config.windows.packageName, true));
  checks.push(check('Android package', config.android.packageName, true));

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
  dev       Build and run one platform with the Vite development server
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
  -h, --help                 Show this help
  -v, --version              Show the kasumi-cli version`);
}
