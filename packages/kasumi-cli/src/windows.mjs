import {
  copyFile,
  cp,
  mkdir,
  readFile,
  rm,
  writeFile
} from 'node:fs/promises';
import { spawnSync } from 'node:child_process';
import path from 'node:path';
import { createZipArchive } from './archive.mjs';
import {
  applicationOutputDirectory,
  packageOutputDirectory
} from './output.mjs';
import { exists, findCommand, run } from './process.mjs';

export async function buildWindows(projectRoot, config, options = {}) {
  const sdkDirectory = path.join(projectRoot, 'vendor', 'windows', 'kasumi');
  if (!(await exists(path.join(sdkDirectory, 'lib', 'cmake', 'Kasumi', 'KasumiConfig.cmake')))) {
    throw new Error(
      'Windows SDK is missing. Re-run create-kasumi with SDK download enabled.'
    );
  }
  const visualStudio = await visualStudioEnvironment();
  const cmake = findCommand('cmake') ?? visualStudio?.cmake;
  if (!cmake) {
    throw new Error('CMake is required for Windows builds.');
  }
  const buildDirectory = windowsBuildDirectory(projectRoot, options);
  const supportsDevMode = await supportsWindowsDevMode(projectRoot);
  const args = [
    '-S',
    path.join(projectRoot, 'windows'),
    '-B',
    buildDirectory,
    '-DCMAKE_BUILD_TYPE=Release',
    `-DCMAKE_PREFIX_PATH=${sdkDirectory}`,
    `-DKASUMI_APP_PACKAGE_NAME=${config.packageName}`,
    `-DKASUMI_APP_VERSION=${config.version}`,
    `-DKASUMI_APP_COMMANDS=${config.commands.join(',')}`,
    `-DKASUMI_APP_PERMISSIONS=${config.permissions.join(',')}`,
    `-DKASUMI_SHELL_ALLOWED_ORIGINS=${config.shell.allowedOrigins.join(',')}`,
    `-DKASUMI_HTTP_ALLOWED_ORIGINS=${config.http.allowedOrigins.join(',')}`,
    `-DKASUMI_WINDOW_DECORATIONS=${config.decorations === false ? 'OFF' : 'ON'}`,
    `-DKASUMI_EXTERNAL_WEB=${options.externalWeb ? 'ON' : 'OFF'}`,
    `-DKASUMI_BUNDLE_WEBVIEW2_LOADER=${options.bundleWebview2Loader ? 'ON' : 'OFF'}`
  ];
  if (supportsDevMode) {
    args.push(`-DKASUMI_DEV_MODE=${options.devMode ? 'ON' : 'OFF'}`);
  }
  if (
    visualStudio &&
    !process.env.CMAKE_GENERATOR &&
    !(await hasCMakeCache(buildDirectory))
  ) {
    args.push('-G', 'Ninja');
    args.push(`-DCMAKE_MAKE_PROGRAM=${visualStudio.ninja}`);
  }
  const toolchain = await findVcpkgToolchain(visualStudio);
  if (!toolchain) {
    throw new Error(
      'vcpkg toolchain was not found. Install the Visual Studio vcpkg component or set VCPKG_ROOT.'
    );
  }
  const vcpkgEnvironment = await createVcpkgEnvironment(projectRoot);
  args.push(`-DCMAKE_TOOLCHAIN_FILE=${toolchain}`);
  args.push(`-DVCPKG_INSTALLED_DIR=${vcpkgEnvironment.installed}`);
  args.push(
    `-DVCPKG_TARGET_TRIPLET=${
      options.bundleWebview2Loader ? 'x64-windows' : 'x64-windows-static-md'
    }`
  );
  await configureWindows(
    projectRoot,
    cmake,
    args,
    buildDirectory,
    visualStudio,
    vcpkgEnvironment
  );
  await runCMake(
    cmake,
    ['--build', buildDirectory, '--config', 'Release'],
    projectRoot,
    visualStudio,
    vcpkgEnvironment
  );

  const executable = await findWindowsExecutable(buildDirectory);
  if (options.devMode) {
    return executable;
  }
  const outputDirectory = applicationOutputDirectory(projectRoot, 'windows');
  await mkdir(outputDirectory, { recursive: true });
  const output = path.join(outputDirectory, `${config.packageName}.exe`);
  await copyFile(executable, output);
  if (options.bundleWebview2Loader) {
    const loader = path.join(path.dirname(executable), 'WebView2Loader.dll');
    if (await exists(loader)) {
      await copyFile(loader, path.join(outputDirectory, 'WebView2Loader.dll'));
    }
  } else {
    await rm(path.join(outputDirectory, 'WebView2Loader.dll'), {
      force: true
    });
  }
  if (options.externalWeb) {
    const web = path.join(path.dirname(executable), 'web');
    if (await exists(web)) {
      await rm(path.join(outputDirectory, 'web'), { recursive: true, force: true });
      await cp(web, path.join(outputDirectory, 'web'), { recursive: true });
    }
  } else {
    await rm(path.join(outputDirectory, 'web'), {
      recursive: true,
      force: true
    });
  }
  return output;
}

export async function packageWindows(projectRoot, config, options = {}) {
  const executable = await buildWindows(projectRoot, config, options);
  return createWindowsPackage(projectRoot, config, executable, options);
}

export async function createWindowsPackage(
  projectRoot,
  config,
  executable,
  options = {}
) {
  const staging = path.join(projectRoot, '.ksm', 'package', 'windows');
  const outputDirectory = packageOutputDirectory(projectRoot);
  const output = path.join(
    outputDirectory,
    `${config.packageName}-${config.version}-windows.zip`
  );
  const sourceDirectory = path.dirname(executable);

  await rm(staging, { recursive: true, force: true });
  await mkdir(staging, { recursive: true });
  await mkdir(outputDirectory, { recursive: true });
  try {
    await copyFile(executable, path.join(staging, path.basename(executable)));
    await writeFile(
      path.join(staging, 'README.txt'),
      [
        `${config.packageName} ${config.version}`,
        '',
        'Run the executable on Windows with the Evergreen WebView2 Runtime installed.',
        options.bundleWebview2Loader
          ? 'WebView2Loader.dll is included because the package was built with the loader option.'
          : 'WebView2Loader.dll is not bundled by default.',
        options.externalWeb
          ? 'The web directory must remain beside the executable.'
          : 'Web assets are embedded in the executable.',
        ''
      ].join('\r\n'),
      'utf8'
    );

    if (options.externalWeb) {
      const web = path.join(sourceDirectory, 'web');
      if (!(await exists(web))) {
        throw new Error(
          'External web assets were not produced beside the executable.'
        );
      }
      await cp(web, path.join(staging, 'web'), { recursive: true });
    }
    if (options.bundleWebview2Loader) {
      const loader = path.join(sourceDirectory, 'WebView2Loader.dll');
      if (await exists(loader)) {
        await copyFile(loader, path.join(staging, 'WebView2Loader.dll'));
      }
    }

    await createZipArchive(staging, output);
  } finally {
    await rm(staging, { recursive: true, force: true });
  }
  return output;
}

export async function findCMake() {
  return findCommand('cmake') ?? (await visualStudioEnvironment())?.cmake;
}

export async function findVcpkgToolchain(visualStudio) {
  if (process.env.CMAKE_TOOLCHAIN_FILE) {
    return process.env.CMAKE_TOOLCHAIN_FILE;
  }
  if (process.env.VCPKG_ROOT) {
    return path.join(
      process.env.VCPKG_ROOT,
      'scripts',
      'buildsystems',
      'vcpkg.cmake'
    );
  }
  const environment =
    visualStudio ?? (await visualStudioEnvironment());
  return environment?.vcpkgToolchain;
}

export async function createVcpkgEnvironment(projectRoot) {
  // Profile caches can be encrypted so keep vcpkg state inside the project
  const root = path.join(projectRoot, '.ksm', 'vcpkg');
  const registries = path.join(root, 'registries');
  const archives = path.join(root, 'archives');
  const downloads = path.join(root, 'downloads');
  const installed = path.join(root, 'installed');
  await Promise.all([
    mkdir(registries, { recursive: true }),
    mkdir(archives, { recursive: true }),
    mkdir(downloads, { recursive: true }),
    mkdir(installed, { recursive: true })
  ]);
  return {
    ...process.env,
    X_VCPKG_REGISTRIES_CACHE: registries,
    VCPKG_DEFAULT_BINARY_CACHE: archives,
    VCPKG_BINARY_SOURCES: `clear;files,${archives},readwrite`,
    VCPKG_DOWNLOADS: downloads,
    installed
  };
}

async function visualStudioEnvironment() {
  if (process.platform !== 'win32') {
    return undefined;
  }
  const vswhere = path.join(
    process.env['ProgramFiles(x86)'] ?? 'C:\\Program Files (x86)',
    'Microsoft Visual Studio',
    'Installer',
    'vswhere.exe'
  );
  if (!(await exists(vswhere))) {
    return undefined;
  }
  const result = spawnSync(
    vswhere,
    [
      '-latest',
      '-products',
      '*',
      '-requires',
      'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
      '-property',
      'installationPath'
    ],
    { encoding: 'utf8' }
  );
  const installation = result.stdout?.trim();
  if (result.status !== 0 || !installation) {
    return undefined;
  }
  const vcvars = path.join(
    installation,
    'VC',
    'Auxiliary',
    'Build',
    'vcvars64.bat'
  );
  const ninja =
    path.join(
      installation,
      'Common7',
      'IDE',
      'CommonExtensions',
      'Microsoft',
      'CMake',
      'Ninja',
      'ninja.exe'
    );
  const cmake = path.join(
    installation,
    'Common7',
    'IDE',
    'CommonExtensions',
    'Microsoft',
    'CMake',
    'CMake',
    'bin',
    'cmake.exe'
  );
  const vcpkgToolchain = path.join(
    installation,
    'VC',
    'vcpkg',
    'scripts',
    'buildsystems',
    'vcpkg.cmake'
  );
  if (
    !(await exists(vcvars)) ||
    !(await exists(ninja)) ||
    !(await exists(cmake))
  ) {
    return undefined;
  }
  return {
    vcvars,
    ninja,
    cmake,
    vcpkgToolchain: await exists(vcpkgToolchain)
      ? vcpkgToolchain
      : undefined
  };
}

async function hasCMakeCache(buildDirectory) {
  return exists(path.join(buildDirectory, 'CMakeCache.txt'));
}

async function runCMake(command, args, cwd, visualStudio, env) {
  if (!visualStudio) {
    await run(command, args, { cwd, env });
    return;
  }
  const buildEnvironment = await visualStudioBuildEnvironment(
    visualStudio.vcvars,
    env,
    cwd
  );
  await run(
    command,
    args,
    { cwd, env: buildEnvironment }
  );
}

const visualStudioEnvironments = new Map();

async function visualStudioBuildEnvironment(vcvars, baseEnvironment, projectRoot) {
  const cacheKey = `${vcvars}\0${baseEnvironment.PATH ?? ''}`;
  const cached = visualStudioEnvironments.get(cacheKey);
  if (cached) {
    return cached;
  }
  const cacheDirectory = path.join(
    projectRoot,
    '.ksm',
    'cache'
  );
  await mkdir(cacheDirectory, { recursive: true });
  const script = path.join(
    cacheDirectory,
    `vcvars-${process.pid}-${Date.now()}.cmd`
  );
  await writeFile(
    script,
    '@echo off\r\ncall "%KASUMI_VCVARS%" >nul || exit /b 1\r\nset\r\n',
    'utf8'
  );
  try {
    const result = spawnSync(
      process.env.ComSpec ?? 'cmd.exe',
      ['/d', '/s', '/c', script],
      {
        env: { ...baseEnvironment, KASUMI_VCVARS: vcvars },
        encoding: 'utf8',
        windowsHide: true
      }
    );
    if (result.status !== 0) {
      throw new Error('The Visual Studio build environment could not be loaded.');
    }
    const environment = { ...baseEnvironment };
    for (const line of result.stdout.split(/\r?\n/)) {
      const separator = line.indexOf('=');
      if (separator <= 0) {
        continue;
      }
      const key = line.slice(0, separator);
      environment[key] = line.slice(separator + 1);
    }
    visualStudioEnvironments.set(cacheKey, environment);
    return environment;
  } finally {
    await rm(script, { force: true });
  }
}

export async function runWindowsDevelopment(
  projectRoot,
  config,
  server,
  options = {}
) {
  const buildDirectory = windowsBuildDirectory(projectRoot, {
    devMode: true
  });
  const executable = await findWindowsExecutable(buildDirectory);
  const webviewData = path.join(projectRoot, '.ksm', 'dev', 'webview2');
  await mkdir(webviewData, { recursive: true });
  await run(
    executable,
    [
      '--url',
      server.url,
      '--dev',
      '--user-data-folder',
      webviewData
    ],
    {
      cwd: path.dirname(executable),
      onSpawn: options.onSpawn
    }
  );
}

async function configureWindows(
  projectRoot,
  cmake,
  args,
  buildDirectory,
  visualStudio,
  environment
) {
  const canReuse =
    await cmakeCacheMatches(buildDirectory, args) &&
    await exists(path.join(environment.installed, 'vcpkg', 'status'));
  if (!canReuse) {
    await runCMake(
      cmake,
      args,
      projectRoot,
      visualStudio,
      environment
    );
  }
}

export async function cmakeCacheMatches(buildDirectory, args) {
  const cache = await readText(path.join(buildDirectory, 'CMakeCache.txt'));
  if (!cache) {
    return false;
  }
  const expected = args
    .filter((argument) => argument.startsWith('-D'))
    .map((argument) => argument.slice(2).split(/=(.*)/s))
    .filter(([key, value]) => key && value !== undefined);
  if (expected.length === 0) {
    return true;
  }
  const cached = new Map();
  for (const line of cache.split(/\r?\n/)) {
    const separator = line.indexOf(':');
    const equals = line.indexOf('=', separator + 1);
    if (separator === -1 || equals === -1) {
      continue;
    }
    cached.set(line.slice(0, separator), line.slice(equals + 1));
  }
  return expected.every(
    ([key, value]) =>
      normalizeCMakeValue(cached.get(key)) === normalizeCMakeValue(value)
  );
}

function normalizeCMakeValue(value) {
  return String(value ?? '')
    .replaceAll('\\', '/')
    .replace(/\/+$/, '')
    .toLowerCase();
}

async function readText(file) {
  try {
    return (await readFile(file, 'utf8')).trim();
  } catch {
    return undefined;
  }
}

export async function supportsWindowsDevMode(projectRoot) {
  const cmake = await readText(
    path.join(projectRoot, 'windows', 'CMakeLists.txt')
  );
  return cmake?.includes('KASUMI_DEV_MODE') === true;
}

function windowsBuildDirectory(projectRoot, options) {
  if (options.devMode) {
    return path.join(projectRoot, '.ksm', 'dev', 'windows');
  }
  return path.join(
    projectRoot,
    '.ksm',
    'build',
    options.bundleWebview2Loader ? 'windows-loader' : 'windows'
  );
}

async function findWindowsExecutable(buildDirectory) {
  const candidates = [
    path.join(buildDirectory, 'Release', 'kasumi_app.exe'),
    path.join(buildDirectory, 'kasumi_app.exe')
  ];
  for (const candidate of candidates) {
    if (await exists(candidate)) {
      return candidate;
    }
  }
  throw new Error('Windows application executable was not produced.');
}
