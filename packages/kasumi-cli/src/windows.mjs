import { copyFile, cp, mkdir, rm } from 'node:fs/promises';
import { spawnSync } from 'node:child_process';
import path from 'node:path';
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
  const buildDirectory = path.join(
    projectRoot,
    '.ksm',
    'build',
    options.bundleWebview2Loader ? 'windows-loader' : 'windows'
  );
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
    `-DKASUMI_EXTERNAL_WEB=${options.externalWeb ? 'ON' : 'OFF'}`,
    `-DKASUMI_BUNDLE_WEBVIEW2_LOADER=${options.bundleWebview2Loader ? 'ON' : 'OFF'}`
  ];
  if (
    visualStudio &&
    !process.env.CMAKE_GENERATOR &&
    !(await hasCMakeCache(buildDirectory))
  ) {
    args.push('-G', 'Ninja');
    args.push(`-DCMAKE_MAKE_PROGRAM=${visualStudio.ninja}`);
  }
  const toolchain =
    process.env.CMAKE_TOOLCHAIN_FILE ??
    (process.env.VCPKG_ROOT
      ? path.join(process.env.VCPKG_ROOT, 'scripts', 'buildsystems', 'vcpkg.cmake')
      : undefined);
  if (toolchain) {
    args.push(`-DCMAKE_TOOLCHAIN_FILE=${toolchain}`);
    args.push(
      `-DVCPKG_TARGET_TRIPLET=${
        options.bundleWebview2Loader ? 'x64-windows' : 'x64-windows-static-md'
      }`
    );
  }
  await runCMake(cmake, args, projectRoot, visualStudio);
  await runCMake(
    cmake,
    ['--build', buildDirectory, '--config', 'Release'],
    projectRoot,
    visualStudio
  );

  const executable = await findWindowsExecutable(buildDirectory);
  const outputDirectory = path.join(projectRoot, 'dist', 'windows');
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

export async function findCMake() {
  return findCommand('cmake') ?? (await visualStudioEnvironment())?.cmake;
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
  if (
    !(await exists(vcvars)) ||
    !(await exists(ninja)) ||
    !(await exists(cmake))
  ) {
    return undefined;
  }
  return { vcvars, ninja, cmake };
}

async function hasCMakeCache(buildDirectory) {
  return exists(path.join(buildDirectory, 'CMakeCache.txt'));
}

async function runCMake(command, args, cwd, visualStudio) {
  if (!visualStudio) {
    await run(command, args, { cwd });
    return;
  }
  const commandLine = [
    `call "${visualStudio.vcvars}" >nul`,
    `"${command}" ${args.map(quoteWindowsArgument).join(' ')}`
  ].join(' && ');
  await run(
    process.env.ComSpec ?? 'cmd.exe',
    ['/d', '/s', '/c', `"${commandLine}"`],
    { cwd, windowsVerbatimArguments: true }
  );
}

function quoteWindowsArgument(value) {
  if (value.length === 0) {
    return '""';
  }
  if (!/[\s"&|<>^()]/.test(value)) {
    return value;
  }
  const escaped = value
    .replace(/(\\*)"/g, '$1$1\\"')
    .replace(/(\\+)$/, '$1$1');
  return `"${escaped}"`;
}

export async function runWindowsDevelopment(
  projectRoot,
  config,
  server
) {
  const buildDirectory = path.join(projectRoot, '.ksm', 'build', 'windows');
  const executable = await findWindowsExecutable(buildDirectory);
  await run(
    executable,
    ['--url', server.url, '--dev'],
    { cwd: path.dirname(executable) }
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
