import {
  copyFile,
  cp,
  mkdir,
  readFile,
  rm,
  writeFile
} from 'node:fs/promises';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { createZipArchive } from './archive.mjs';
import {
  applicationOutputDirectory,
  packageOutputDirectory
} from './output.mjs';
import {
  exists,
  findCommand,
  run,
  spawnLongRunning,
  terminateProcessTree,
  waitForProcessExit,
  waitForProcessSpawn
} from './process.mjs';
import { resolveProjectIcon } from './icons.mjs';

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
  const manifestMode = await exists(
    path.join(projectRoot, 'windows', 'vcpkg.json')
  );
  await resetManifestModeMismatch(buildDirectory, manifestMode);
  const windowIcon = await resolveProjectIcon(
    projectRoot,
    config.icons?.window,
    '.ico',
    'configuration.icons.window'
  );
  const trayIcon = await resolveProjectIcon(
    projectRoot,
    config.icons?.tray,
    '.ico',
    'configuration.icons.tray'
  );
  const args = [
    '-S',
    path.join(projectRoot, 'windows'),
    '-B',
    buildDirectory,
    '-DCMAKE_BUILD_TYPE=Release',
    '-DCMAKE_DEPENDS_USE_COMPILER=OFF',
    `-DCMAKE_PREFIX_PATH=${sdkDirectory}`,
    `-DKASUMI_APP_PACKAGE_NAME=${config.packageName}`,
    `-DKASUMI_APP_VERSION=${config.version}`,
    `-DKASUMI_APP_COMMANDS=${config.commands.join(',')}`,
    `-DKASUMI_APP_PERMISSIONS=${config.permissions.join(',')}`,
    `-DKASUMI_SHELL_ALLOWED_ORIGINS=${config.shell.allowedOrigins.join(',')}`,
    `-DKASUMI_HTTP_ALLOWED_ORIGINS=${config.http.allowedOrigins.join(',')}`,
    `-DKASUMI_WINDOW_DECORATIONS=${config.decorations === false ? 'OFF' : 'ON'}`,
    `-DKASUMI_WINDOW_BACKGROUND_COLOR=${(config.window.backgroundColor ?? '').replace(/^#/, '')}`,
    `-DKASUMI_WINDOW_WIDTH=${config.window.width ?? 1080}`,
    `-DKASUMI_WINDOW_HEIGHT=${config.window.height ?? 720}`,
    `-DKASUMI_WINDOW_MIN_WIDTH=${config.window.minWidth ?? 320}`,
    `-DKASUMI_WINDOW_MIN_HEIGHT=${config.window.minHeight ?? 240}`,
    `-DKASUMI_WINDOW_MAX_WIDTH=${config.window.maxWidth ?? 0}`,
    `-DKASUMI_WINDOW_MAX_HEIGHT=${config.window.maxHeight ?? 0}`,
    `-DKASUMI_WINDOW_RESIZABLE=${config.window.resizable === false ? 'OFF' : 'ON'}`,
    `-DKASUMI_ERROR_PAGE_BACKGROUND_COLOR=${(config.errorPage?.backgroundColor ?? '').replace(/^#/, '')}`,
    `-DKASUMI_ERROR_PAGE_CARD_COLOR=${(config.errorPage?.cardColor ?? '').replace(/^#/, '')}`,
    `-DKASUMI_ERROR_PAGE_BORDER_COLOR=${(config.errorPage?.borderColor ?? '').replace(/^#/, '')}`,
    `-DKASUMI_ERROR_PAGE_TEXT_COLOR=${(config.errorPage?.textColor ?? '').replace(/^#/, '')}`,
    `-DKASUMI_ERROR_PAGE_MUTED_TEXT_COLOR=${(config.errorPage?.mutedTextColor ?? '').replace(/^#/, '')}`,
    `-DKASUMI_ERROR_PAGE_ACCENT_COLOR=${(config.errorPage?.accentColor ?? '').replace(/^#/, '')}`,
    `-DKASUMI_ERROR_PAGE_ERROR_COLOR=${(config.errorPage?.errorColor ?? '').replace(/^#/, '')}`,
    `-DKASUMI_ERROR_PAGE_TITLE=${config.errorPage?.title ?? ''}`,
    `-DKASUMI_ERROR_PAGE_HINT=${config.errorPage?.hint ?? ''}`,
    `-DKASUMI_EXTERNAL_WEB=${options.externalWeb ? 'ON' : 'OFF'}`,
    `-DKASUMI_BUNDLE_WEBVIEW2_LOADER=${options.bundleWebview2Loader ? 'ON' : 'OFF'}`
  ];
  if (supportsDevMode) {
    args.push(`-DKASUMI_DEV_MODE=${options.devMode ? 'ON' : 'OFF'}`);
  }
  args.push(`-DKASUMI_APP_ICON=${cmakePath(windowIcon)}`);
  args.push(`-DKASUMI_TRAY_ICON=${cmakePath(trayIcon)}`);
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
  args.push(`-DVCPKG_MANIFEST_MODE=${manifestMode ? 'ON' : 'OFF'}`);
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
  const outputs = [await createWindowsPackage(projectRoot, config, executable, options)];
  const installer = options.installer ?? config.bundle?.windowsInstaller;
  if (installer) {
    outputs.push(
      await createWindowsInstaller(
        projectRoot,
        config,
        executable,
        installer,
        options
      )
    );
  }
  return outputs;
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

export async function createWindowsInstaller(
  projectRoot,
  config,
  executable,
  installerType,
  options = {}
) {
  const installerIcon = await resolveProjectIcon(
    projectRoot,
    config.icons?.installer,
    '.ico',
    'configuration.icons.installer'
  );
  const staging = path.join(
    projectRoot,
    '.ksm',
    'package',
    'installer'
  );
  const outputDirectory = packageOutputDirectory(projectRoot);
  await mkdir(staging, { recursive: true });
  await mkdir(outputDirectory, { recursive: true });
  const sourceDirectory = path.dirname(executable);

  if (installerType === 'nsis') {
    const makensis = findCommand('makensis');
    if (!makensis) {
      throw new Error(
        'NSIS is not installed. Install NSIS and add makensis to PATH.'
      );
    }
    const appName = config.packageName;
    const version = config.version;
    const output = path.join(
      outputDirectory,
      `${appName}-${version}-setup.exe`
    );
    const appExe = path.basename(executable);
    const nsi = path.join(staging, `install-${Date.now()}.nsi`);
    await writeFile(
      nsi,
      [
        'Unicode true',
        `!define APP_NAME ${JSON.stringify(appName)}`,
        `!define APP_VERSION ${JSON.stringify(version)}`,
        `!define APP_EXE ${JSON.stringify(appExe)}`,
        `OutFile ${JSON.stringify(output)}`,
        ...(installerIcon
          ? [
              `Icon ${JSON.stringify(installerIcon)}`,
              `UninstallIcon ${JSON.stringify(installerIcon)}`
            ]
          : []),
        'InstallDir "$PROGRAMFILES64\\Kasumi\\' + appName + '"',
        'RequestExecutionLevel admin',
        '',
        'Section "Main"',
        '  SetOutPath "$INSTDIR"',
        `  File ${JSON.stringify(executable)}`,
        '  WriteUninstaller "$INSTDIR\\Uninstall.exe"',
        '  CreateShortcut "$DESKTOP\\' + appName + '.lnk" "$INSTDIR\\' + appExe + '"',
        `  WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${appName}" "DisplayName" "${appName}"`,
        `  WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\${appName}" "DisplayVersion" "${version}"`,
        'SectionEnd',
        '',
        'Section "Uninstall"',
        '  RMDir /r "$INSTDIR"',
        `  Delete "$DESKTOP\\${appName}.lnk"`,
        'SectionEnd',
        ''
      ].join('\r\n'),
      'utf8'
    );
    await run(makensis, [nsi], { cwd: projectRoot });
    await rm(nsi, { force: true });
    return output;
  }

  if (installerType === 'msi') {
    const candle = findCommand('candle');
    const light = findCommand('light');
    if (!candle || !light) {
      throw new Error(
        'MSI packaging requires the WiX Toolset (candle and light).'
      );
    }
    const appName = config.packageName;
    const output = path.join(
      outputDirectory,
      `${appName}-${config.version}-setup.msi`
    );
    const wxs = path.join(staging, `install-${Date.now()}.wxs`);
    const wixobj = path.join(staging, `install-${Date.now()}.wixobj`);
    await writeFile(
      wxs,
      [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">',
        '  <Product Id="*" Name="' + appName + '" Language="1033"',
        '           Version="' + config.version + '" Manufacturer="Kasumi"',
        '           UpgradeCode="00000000-0000-0000-0000-000000000000">',
        '    <Package InstallerVersion="200" Compressed="yes" />',
        ...(installerIcon
          ? [
              '    <Icon Id="ProductIcon" SourceFile="' +
                xmlEscape(installerIcon) +
                '" />',
              '    <Property Id="ARPPRODUCTICON" Value="ProductIcon" />'
            ]
          : []),
        '    <Directory Id="TARGETDIR" Name="SourceDir">',
        '      <Directory Id="ProgramFiles64Folder" Name="PFiles">',
        '        <Directory Id="INSTALLFOLDER" Name="Kasumi">',
        '          <Directory Id="APPLICATIONFOLDER" Name="' + appName + '" />',
        '        </Directory>',
        '      </Directory>',
        '    </Directory>',
        '    <ComponentGroup Id="ProductComponents" Directory="APPLICATIONFOLDER">',
        '      <Component Id="MainExecutable" Guid="*">',
        '        <File Id="MainExe" Source="' + executable + '" />',
        '      </Component>',
        '    </ComponentGroup>',
        '    <Feature Id="MainFeature" Title="Main Feature" Level="1">',
        '      <ComponentGroupRef Id="ProductComponents" />',
        '    </Feature>',
        '  </Product>',
        '</Wix>',
        ''
      ].join('\r\n'),
      'utf8'
    );
    await run(candle, [wxs, '-o', wixobj], { cwd: projectRoot });
    await run(light, [wixobj, '-out', output], { cwd: projectRoot });
    await rm(wxs, { force: true });
    await rm(wixobj, { force: true });
    return output;
  }

  throw new Error(`Unsupported Windows installer type: ${installerType}`);
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
    X_VCPKG_ASSET_SOURCES: buildVcpkgAssetSources(),
    installed
  };
}

export function buildVcpkgAssetSources() {
  const script = path.resolve(
    path.dirname(fileURLToPath(import.meta.url)),
    '..',
    'bin',
    'vcpkg-download.mjs'
  );
  return [
    'clear',
    `x-script,${shellQuote(process.execPath)} ${shellQuote(script)} ` +
      '{url} {sha512} {dst}'
  ].join(';');
}

function shellQuote(value) {
  return `"${String(value).replaceAll('"', '\\"')}"`;
}

function cmakePath(value) {
  return value ? value.replaceAll('\\', '/') : '';
}

function xmlEscape(value) {
  return String(value)
    .replaceAll('&', '&amp;')
    .replaceAll('"', '&quot;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;');
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

async function resetManifestModeMismatch(buildDirectory, manifestMode) {
  const cache = await readText(path.join(buildDirectory, 'CMakeCache.txt'));
  if (!cache) {
    return;
  }
  const match = cache.match(
    /^Z_VCPKG_CHECK_MANIFEST_MODE:INTERNAL=(ON|OFF)$/m
  );
  if (!match || (match[1] === 'ON') === manifestMode) {
    return;
  }
  await rm(buildDirectory, { recursive: true, force: true });
}

async function runCMake(command, args, cwd, visualStudio, env) {
  const environment = { ...env, VSLANG: '1033' };
  if (!visualStudio) {
    await run(command, args, { cwd, env: environment });
    return;
  }
  const buildEnvironment = await visualStudioBuildEnvironment(
    visualStudio.vcvars,
    environment,
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
  const host = await startWindowsDevelopment(
    projectRoot,
    config,
    server,
    options
  );
  await host.waitForExit();
}

export async function startWindowsDevelopment(
  projectRoot,
  config,
  server,
  options = {}
) {
  const buildDirectory = windowsBuildDirectory(projectRoot, {
    devMode: true
  });
  const executable =
    options.executable ?? await findWindowsExecutable(buildDirectory);
  const webviewData = path.join(projectRoot, '.ksm', 'dev', 'webview2');
  await mkdir(webviewData, { recursive: true });
  const child = spawnLongRunning(
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
      env: options.env
    }
  );
  const exit = new Promise((resolve) => {
    child.once('exit', (code, signal) => resolve({ code, signal }));
  });
  await waitForProcessSpawn(child);
  options.onSpawn?.(child);
  return {
    child,
    executable,
    waitForExit() {
      return exit;
    },
    async stop(timeoutMs = 5000) {
      if (child.exitCode !== null || child.signalCode !== null) {
        return;
      }
      terminateProcessTree(child.pid);
      await waitForProcessExit(child, timeoutMs).catch(() => undefined);
    }
  };
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
