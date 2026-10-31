import { copyFile, mkdir } from 'node:fs/promises';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { exists, findCommand, run } from './process.mjs';

export async function buildAndroid(projectRoot, config) {
  const repository = path.join(projectRoot, 'vendor', 'android', 'repo');
  if (!(await exists(repository))) {
    throw new Error(
      'Android Maven repository is missing. Re-run create-kasumi with SDK download enabled.'
    );
  }
  const gradle = await findGradle(projectRoot);
  await mkdir(path.join(projectRoot, '.ksm', 'afunix'), { recursive: true });
  const args = [
    '-p',
    path.join(projectRoot, 'android'),
    `-PksmPackageName=${config.packageName}`,
    `-PksmVersion=${config.version}`,
    `-PksmCommands=${config.commands.join(',')}`,
    'assembleDebug'
  ];
  await run(gradle.command, [...gradle.prefix, ...args], {
    cwd: projectRoot,
    env: gradleEnvironment()
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
  const outputDirectory = path.join(projectRoot, 'dist', 'android');
  await mkdir(outputDirectory, { recursive: true });
  const output = path.join(
    outputDirectory,
    `${config.packageName}-${config.version}.apk`
  );
  await copyFile(source, output);
  return output;
}

export async function runAndroidDevelopment(projectRoot, config, server) {
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

async function findGradle(projectRoot) {
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
    return { command: wrapper, prefix: [] };
  }
  const gradle = findCommand('gradle');
  if (!gradle) {
    throw new Error(
      'Gradle is required for Android builds. Install Gradle or set GRADLE.'
    );
  }
  return { command: gradle, prefix: [] };
}

function gradleEnvironment() {
  const environment = { ...process.env };
  if (process.platform === 'win32' && !environment.JAVA_TOOL_OPTIONS) {
    environment.JAVA_TOOL_OPTIONS =
      '-Djdk.net.unixdomain.tmpdir=.ksm/afunix';
  }
  return environment;
}

function adbEnvironment() {
  return { ...process.env };
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
