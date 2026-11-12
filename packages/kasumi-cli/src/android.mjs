import { copyFile, mkdir, readdir, rm } from 'node:fs/promises';
import path from 'node:path';
import { spawn } from 'node:child_process';
import {
  applicationOutputDirectory,
  packageOutputDirectory
} from './output.mjs';
import { exists, findCommand, run } from './process.mjs';
import { resolveProjectIcon } from './icons.mjs';

export async function buildAndroid(projectRoot, config) {
  const repository = path.join(projectRoot, 'vendor', 'android', 'repo');
  if (!(await exists(repository))) {
    throw new Error(
      'Android Maven repository is missing. Re-run create-kasumi with SDK download enabled.'
    );
  }
  const gradle = await findGradle(projectRoot);
  const unixDomainDirectory = path.join(projectRoot, '.ksm', 'afunix');
  await mkdir(unixDomainDirectory, { recursive: true });
  const launcherIcon = await prepareAndroidIcon(projectRoot, config);
  const args = [
    '-p',
    path.join(projectRoot, 'android'),
    `-PksmPackageName=${config.packageName}`,
    `-PksmVersion=${config.version}`,
    `-PksmCommands=${config.commands.join(',')}`,
    `-PksmPermissions=${config.permissions.join(',')}`,
    `-PksmShellAllowedOrigins=${config.shell.allowedOrigins.join(',')}`,
    `-PksmHttpAllowedOrigins=${config.http.allowedOrigins.join(',')}`,
    `-PksmWebViewProductionEntry=${config.webview.productionEntry}`,
    `-PksmWebViewDevelopmentEntries=${config.webview.developmentEntries.join(',')}`,
    `-PksmWebViewSupportedHtmlFiles=${config.webview.supportedHtmlFiles.join(',')}`,
    `-PksmLauncherIcon=${launcherIcon ?? '@android:drawable/sym_def_app_icon'}`,
    'assembleDebug'
  ];
  await run(gradle.command, [...gradle.prefix, ...args], {
    cwd: projectRoot,
    env: gradleEnvironment(unixDomainDirectory)
  });
  const source = path.join(
    projectRoot,
    'android',
    'app',
    'build',
    'outputs',
    'apk',
    'debug',
    'app-debug.apk'
  );
  if (!(await exists(source))) {
    throw new Error('Android APK was not produced.');
  }
  const outputDirectory = applicationOutputDirectory(projectRoot, 'android');
  await mkdir(outputDirectory, { recursive: true });
  const output = path.join(
    outputDirectory,
    `${config.packageName}-${config.version}.apk`
  );
  await copyFile(source, output);
  return output;
}

export async function packageAndroid(projectRoot, config) {
  requireReleaseSigning();
  const gradle = await findGradle(projectRoot);
  const unixDomainDirectory = path.join(projectRoot, '.ksm', 'afunix');
  await mkdir(unixDomainDirectory, { recursive: true });
  const launcherIcon = await prepareAndroidIcon(projectRoot, config);
  await run(
    gradle.command,
    [
      ...gradle.prefix,
      '-p',
      path.join(projectRoot, 'android'),
      `-PksmPackageName=${config.packageName}`,
      `-PksmVersion=${config.version}`,
      `-PksmCommands=${config.commands.join(',')}`,
      `-PksmPermissions=${config.permissions.join(',')}`,
      `-PksmShellAllowedOrigins=${config.shell.allowedOrigins.join(',')}`,
      `-PksmHttpAllowedOrigins=${config.http.allowedOrigins.join(',')}`,
      `-PksmWebViewProductionEntry=${config.webview.productionEntry}`,
      `-PksmWebViewDevelopmentEntries=${config.webview.developmentEntries.join(',')}`,
      `-PksmWebViewSupportedHtmlFiles=${config.webview.supportedHtmlFiles.join(',')}`,
      `-PksmLauncherIcon=${launcherIcon ?? '@android:drawable/sym_def_app_icon'}`,
      'assembleRelease'
    ],
    {
      cwd: projectRoot,
      env: gradleEnvironment(unixDomainDirectory)
    }
  );

  const source = path.join(
    projectRoot,
    'android',
    'app',
    'build',
    'outputs',
    'apk',
    'release',
    'app-release.apk'
  );
  if (!(await exists(source))) {
    throw new Error('Android release APK was not produced. Check the release signing configuration.');
  }
  const outputDirectory = packageOutputDirectory(projectRoot);
  await mkdir(outputDirectory, { recursive: true });
  const output = path.join(
    outputDirectory,
    `${config.packageName}-${config.version}-android.apk`
  );

  const apksigner = await findApkSigner();
  if (!apksigner) {
    throw new Error(
      'apksigner was not found. Set APKSIGNER or install the Android SDK build-tools.'
    );
  }
  await run(apksigner, ['verify', '--print-certs', source], {
    cwd: projectRoot
  });
  await copyFile(source, output);
  return output;
}

export async function runAndroidDevelopment(projectRoot, config, server) {
  const developmentEntries = config.webview?.developmentEntries ?? [];
  if (!developmentEntries.includes(server.url)) {
    throw new Error(
      `Development URL ${server.url} is not listed in android.webview.developmentEntries.`
    );
  }
  const configuredAdb = process.env.ADB;
  const adb = configuredAdb && await exists(configuredAdb)
    ? configuredAdb
    : findCommand('adb');
  if (!adb) {
    throw new Error('adb is required for Android development.');
  }
  const apk = await buildAndroid(projectRoot, config);
  await run(adb, ['reverse', 'tcp:5173', 'tcp:5173'], {
    cwd: projectRoot,
    env: adbEnvironment()
  });
  const install = await runCapture(adb, ['install', '-r', apk], adbEnvironment());
  if (install.code !== 0 && install.output.includes('INSTALL_FAILED_UPDATE_INCOMPATIBLE')) {
    await run(adb, ['uninstall', config.packageName], {
      cwd: projectRoot,
      env: adbEnvironment()
    });
    await run(adb, ['install', apk], { cwd: projectRoot, env: adbEnvironment() });
  } else if (install.code !== 0) {
    throw new Error(install.output || 'adb install failed.');
  }
  await run(
    adb,
    ['shell', 'am', 'force-stop', config.packageName],
    { cwd: projectRoot, env: adbEnvironment() }
  );
  await run(
    adb,
    [
      'shell',
      'am',
      'start',
      '-n',
      `${config.packageName}/.MainActivity`,
      '--es',
      'ksmDevUrl',
      server.url
    ],
    { cwd: projectRoot, env: adbEnvironment() }
  );
  console.log('Android development app is running. Press Ctrl+C to stop.');
  await new Promise((resolve) => {
    const stop = () => resolve();
    process.once('SIGINT', stop);
    process.once('SIGTERM', stop);
  });
  await run(
    adb,
    ['shell', 'am', 'force-stop', config.packageName],
    { cwd: projectRoot, env: adbEnvironment() }
  ).catch(() => {});
  await run(adb, ['reverse', '--remove', 'tcp:5173'], {
    cwd: projectRoot,
    env: adbEnvironment()
  }).catch(() => {});
}

export async function findGradle(projectRoot) {
  const configured = process.env.GRADLE;
  if (configured) {
    return { command: configured, prefix: [] };
  }
  const wrapper = path.join(
    projectRoot,
    'android',
    process.platform === 'win32' ? 'gradlew.bat' : 'gradlew'
  );
  if (await exists(wrapper)) {
    return process.platform === 'win32'
      ? { command: wrapper, prefix: [] }
      : { command: 'sh', prefix: [wrapper] };
  }
  const gradle = findCommand('gradle');
  if (!gradle) {
    throw new Error(
      'Gradle is required for Android builds. Install Gradle or set GRADLE.'
    );
  }
  return { command: gradle, prefix: [] };
}

export async function findApkSigner() {
  const configured = process.env.APKSIGNER;
  if (configured && await exists(configured)) {
    return configured;
  }
  const roots = [process.env.ANDROID_HOME, process.env.ANDROID_SDK_ROOT]
    .filter(Boolean);
  const executable = process.platform === 'win32'
    ? 'apksigner.bat'
    : 'apksigner';
  for (const root of roots) {
    const buildTools = path.join(root, 'build-tools');
    if (!(await exists(buildTools))) {
      continue;
    }
    const entries = await readdir(buildTools, { withFileTypes: true });
    const versions = entries
      .filter((entry) => entry.isDirectory())
      .map((entry) => entry.name)
      .sort(compareVersions)
      .reverse();
    for (const version of versions) {
      const candidate = path.join(buildTools, version, executable);
      if (await exists(candidate)) {
        return candidate;
      }
    }
  }
  return findCommand(executable);
}

function compareVersions(left, right) {
  const leftParts = left.split(/[.-]/).map(parseVersionPart);
  const rightParts = right.split(/[.-]/).map(parseVersionPart);
  const length = Math.max(leftParts.length, rightParts.length);
  for (let index = 0; index < length; index += 1) {
    const leftPart = leftParts[index];
    const rightPart = rightParts[index];
    if (leftPart === rightPart) {
      continue;
    }
    if (leftPart === undefined) {
      return -1;
    }
    if (rightPart === undefined) {
      return 1;
    }
    if (typeof leftPart === 'number' && typeof rightPart === 'number') {
      return leftPart - rightPart;
    }
    return String(leftPart).localeCompare(String(rightPart));
  }
  return 0;
}

function parseVersionPart(value) {
  return /^\d+$/.test(value) ? Number(value) : value;
}

function requireReleaseSigning() {
  const required = [
    'KASUMI_ANDROID_KEYSTORE',
    'KASUMI_ANDROID_KEYSTORE_PASSWORD',
    'KASUMI_ANDROID_KEY_ALIAS',
    'KASUMI_ANDROID_KEY_PASSWORD'
  ];
  const missing = required.filter((name) => !process.env[name]);
  if (missing.length > 0) {
    throw new Error(
      `Android release signing requires: ${missing.join(', ')}.`
    );
  }
}

function gradleEnvironment(unixDomainDirectory) {
  const environment = { ...process.env };
  if (process.platform === 'win32' && !environment.JAVA_TOOL_OPTIONS) {
    const normalized = unixDomainDirectory.replaceAll('\\', '/');
    environment.JAVA_TOOL_OPTIONS =
      `-Djdk.net.unixdomain.tmpdir="${normalized}"`;
  }
  return environment;
}

function adbEnvironment() {
  return { ...process.env };
}

async function prepareAndroidIcon(projectRoot, config) {
  const icon = await resolveProjectIcon(
    projectRoot,
    config.icons?.android,
    '.png',
    'configuration.icons.android'
  );
  const resourceDirectory = path.join(projectRoot, '.ksm', 'android-res');
  await rm(resourceDirectory, { recursive: true, force: true });
  if (!icon) {
    return null;
  }
  const drawableDirectory = path.join(resourceDirectory, 'drawable');
  await mkdir(drawableDirectory, { recursive: true });
  await copyFile(icon, path.join(drawableDirectory, 'ksm_launcher.png'));
  return '@drawable/ksm_launcher';
}

function runCapture(command, args, environment) {
  return new Promise((resolve, reject) => {
    const child = spawn(command, args, {
      env: environment,
      stdio: ['ignore', 'pipe', 'pipe']
    });
    let output = '';
    child.stdout.on('data', (chunk) => {
      output += chunk.toString();
    });
    child.stderr.on('data', (chunk) => {
      output += chunk.toString();
    });
    child.once('error', reject);
    child.once('exit', (code) => resolve({ code: code ?? 1, output }));
  });
}
