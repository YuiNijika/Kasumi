import assert from 'node:assert/strict';
import {
  mkdir,
  mkdtemp,
  readFile,
  rm,
  stat,
  writeFile
} from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { parseArgs } from '../src/args.mjs';
import {
  findApkSigner,
  findGradle,
  packageAndroid
} from '../src/android.mjs';
import { ConfigError, readConfig } from '../src/config.mjs';
import {
  injectContentSecurityPolicy,
  readFrontendSettings
} from '../src/frontend.mjs';
import { createZipArchive } from '../src/archive.mjs';
import { collectDoctorChecks } from '../src/main.mjs';
import {
  applicationOutputDirectory,
  packageOutputDirectory
} from '../src/output.mjs';
import { commandInvocation } from '../src/process.mjs';
import {
  createVcpkgEnvironment,
  createWindowsPackage,
  findVcpkgToolchain
} from '../src/windows.mjs';

const testDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(testDirectory, '..', '..', '..');

test('parseArgs defaults build to Windows and supports all platforms', () => {
  assert.deepEqual([...parseArgs(['build']).platforms], ['windows']);
  assert.deepEqual(
    [...parseArgs(['build', '--all']).platforms].sort(),
    ['android', 'windows']
  );
  assert.equal(parseArgs(['build', '--with-webview2-loader']).bundleWebview2Loader, true);
});

test('parseArgs supports short help and version flags', () => {
  assert.equal(parseArgs(['-h']).help, true);
  assert.equal(parseArgs(['-v']).version, true);
});

test('wraps Windows batch commands through the system shell', () => {
  const invocation = commandInvocation('gradlew.bat', ['--version']);
  if (process.platform === 'win32') {
    assert.equal(invocation.command, process.env.ComSpec ?? 'cmd.exe');
    assert.deepEqual(invocation.args.slice(0, 3), ['/d', '/s', '/c']);
    assert.match(invocation.args[3], /gradlew\.bat --version/);
    return;
  }
  assert.equal(invocation.command, 'gradlew.bat');
  assert.deepEqual(invocation.args, ['--version']);
});

test('keeps application outputs under .ksm', () => {
  const projectRoot = path.join('C:\\project');
  assert.equal(
    applicationOutputDirectory(projectRoot, 'windows'),
    path.join(projectRoot, '.ksm', 'dist', 'windows')
  );
  assert.equal(
    applicationOutputDirectory(projectRoot, 'android'),
    path.join(projectRoot, '.ksm', 'dist', 'android')
  );
  assert.equal(
    packageOutputDirectory(projectRoot),
    path.join(projectRoot, '.ksm', 'dist', 'packages')
  );
});

test('discovers the bundled Visual Studio vcpkg toolchain', async () => {
  const environment = saveEnvironment([
    'CMAKE_TOOLCHAIN_FILE',
    'VCPKG_ROOT'
  ]);
  try {
    delete process.env.CMAKE_TOOLCHAIN_FILE;
    delete process.env.VCPKG_ROOT;
    const toolchain = path.join(
      'C:\\VisualStudio',
      'VC',
      'vcpkg',
      'scripts',
      'buildsystems',
      'vcpkg.cmake'
    );
    assert.equal(
      await findVcpkgToolchain({ vcpkgToolchain: toolchain }),
      toolchain
    );
  } finally {
    restoreEnvironment(environment);
  }
});

test('prefers configured vcpkg toolchains over Visual Studio', async () => {
  const environment = saveEnvironment([
    'CMAKE_TOOLCHAIN_FILE',
    'VCPKG_ROOT'
  ]);
  try {
    const explicit = path.join('C:\\tools', 'custom', 'vcpkg.cmake');
    process.env.CMAKE_TOOLCHAIN_FILE = explicit;
    process.env.VCPKG_ROOT = 'C:\\ignored';
    assert.equal(await findVcpkgToolchain(), explicit);

    delete process.env.CMAKE_TOOLCHAIN_FILE;
    process.env.VCPKG_ROOT = 'C:\\vcpkg';
    assert.equal(
      await findVcpkgToolchain(),
      path.join(
        'C:\\vcpkg',
        'scripts',
        'buildsystems',
        'vcpkg.cmake'
      )
    );
  } finally {
    restoreEnvironment(environment);
  }
});

test('keeps vcpkg caches inside the project', async () => {
  const temporaryRoot = await mkdtemp(
    path.join(tmpdir(), 'kasumi-vcpkg-environment-test-')
  );
  try {
    const environment = await createVcpkgEnvironment(temporaryRoot);
    const cacheRoot = path.join(temporaryRoot, '.ksm', 'vcpkg');
    assert.equal(
      environment.X_VCPKG_REGISTRIES_CACHE,
      path.join(cacheRoot, 'registries')
    );
    assert.equal(
      environment.VCPKG_DEFAULT_BINARY_CACHE,
      path.join(cacheRoot, 'archives')
    );
    assert.equal(
      environment.VCPKG_DOWNLOADS,
      path.join(cacheRoot, 'downloads')
    );
    for (const directory of [
      environment.X_VCPKG_REGISTRIES_CACHE,
      environment.VCPKG_DEFAULT_BINARY_CACHE,
      environment.VCPKG_DOWNLOADS
    ]) {
      assert.equal((await stat(directory)).isDirectory(), true);
    }
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('prefers the project Gradle wrapper', async () => {
  const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'kasumi-gradle-test-'));
  try {
    await mkdir(path.join(temporaryRoot, 'android'), { recursive: true });
    await writeFile(
      path.join(
        temporaryRoot,
        'android',
        process.platform === 'win32' ? 'gradlew.bat' : 'gradlew'
      ),
      '',
      'utf8'
    );
    const result = await findGradle(temporaryRoot);
    if (process.platform === 'win32') {
      assert.match(result.command, /gradlew\.bat$/);
      assert.deepEqual(result.prefix, []);
      return;
    }
    assert.equal(result.command, 'sh');
    assert.match(result.prefix[0], /gradlew$/);
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('detects frontend framework scripts and build outputs', async () => {
  const frameworks = [
    {
      dependency: 'vite',
      output: path.join('frontend', 'dist'),
      url: 'http://localhost:5173/index.html'
    },
    {
      dependency: 'next',
      output: path.join('frontend', 'out'),
      url: 'http://localhost:5173/'
    },
    {
      dependency: 'nuxt',
      output: path.join('frontend', '.output', 'public'),
      url: 'http://localhost:5173/'
    }
  ];
  for (const framework of frameworks) {
    const temporaryRoot = await mkdtemp(
      path.join(tmpdir(), 'kasumi-frontend-test-')
    );
    try {
      await writeFile(
        path.join(temporaryRoot, 'package.json'),
        JSON.stringify({
          dependencies: {
            [framework.dependency]: '^1.0.0'
          },
          scripts: {
            'frontend:dev': 'framework dev',
            'frontend:build': 'framework build'
          }
        }),
        'utf8'
      );
      const settings = await readFrontendSettings(temporaryRoot);
      assert.equal(settings.outputDirectory, framework.output);
      assert.equal(settings.devUrl, framework.url);
      assert.equal(settings.devScript, 'frontend:dev');
      assert.equal(settings.buildScript, 'frontend:build');
    } finally {
      await rm(temporaryRoot, { recursive: true, force: true });
    }
  }
});

test('injects a single CSP meta tag into frontend output', async () => {
  const temporaryRoot = await mkdtemp(
    path.join(tmpdir(), 'kasumi-csp-test-')
  );
  const outputDirectory = path.join(temporaryRoot, 'frontend', 'dist');
  try {
    await mkdir(outputDirectory, { recursive: true });
    await writeFile(
      path.join(outputDirectory, 'index.html'),
      '<!doctype html><html><head><title>App</title></head><body></body></html>',
      'utf8'
    );
    await injectContentSecurityPolicy(temporaryRoot);
    await injectContentSecurityPolicy(temporaryRoot);
    const html = await readFile(
      path.join(outputDirectory, 'index.html'),
      'utf8'
    );
    assert.equal(
      html.match(/http-equiv="Content-Security-Policy"/g)?.length,
      1
    );
    assert.match(html, /script-src 'self' 'unsafe-inline'/);
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('doctor accepts Next.js and Nuxt projects without index.html', async () => {
  for (const dependency of ['next', 'nuxt']) {
    const temporaryRoot = await mkdtemp(
      path.join(tmpdir(), 'kasumi-doctor-test-')
    );
    try {
      await mkdir(path.join(temporaryRoot, 'frontend'), { recursive: true });
      await writeFile(
        path.join(temporaryRoot, 'package.json'),
        JSON.stringify({
          dependencies: { [dependency]: '^16.0.0' },
          scripts: {
            'frontend:dev': `${dependency} dev frontend`,
            'frontend:build': `${dependency} build frontend`
          }
        }),
        'utf8'
      );
      const checks = await collectDoctorChecks(temporaryRoot, {
        windows: { packageName: 'sample-app' },
        android: { packageName: 'dev.kasumi.sample' }
      });
      const entry = checks.find((item) => item.label === 'frontend entry');
      assert.equal(entry.ok, true);
      assert.match(entry.value, /frontend$/);
    } finally {
      await rm(temporaryRoot, { recursive: true, force: true });
    }
  }
});

test('creates zip archives with normalized relative paths', async () => {
  const temporaryRoot = await mkdtemp(
    path.join(tmpdir(), 'kasumi-archive-test-')
  );
  try {
    const source = path.join(temporaryRoot, 'source');
    const archive = path.join(temporaryRoot, 'package.zip');
    await mkdir(source, { recursive: true });
    await mkdir(path.join(source, 'web'), { recursive: true });
    await writeFile(path.join(source, 'README.txt'), 'Kasumi', 'utf8');
    await writeFile(path.join(source, 'web', 'index.html'), '<html></html>', 'utf8');
    await createZipArchive(source, archive);
    const info = await stat(archive);
    assert.equal(info.isFile(), true);
    assert.ok(info.size > 0);
    assert.deepEqual(
      zipEntryNames(await readFile(archive)),
      ['README.txt', 'web/index.html']
    );
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('Windows packages contain the executable and optional runtime assets', async () => {
  const temporaryRoot = await mkdtemp(
    path.join(tmpdir(), 'kasumi-windows-package-test-')
  );
  try {
    const executable = path.join(
      temporaryRoot,
      'dist',
      'windows',
      'sample-app.exe'
    );
    await mkdir(path.dirname(executable), { recursive: true });
    await mkdir(path.join(path.dirname(executable), 'web'), { recursive: true });
    await writeFile(executable, 'executable', 'utf8');
    await writeFile(
      path.join(path.dirname(executable), 'web', 'index.html'),
      '<html></html>',
      'utf8'
    );
    await writeFile(
      path.join(path.dirname(executable), 'WebView2Loader.dll'),
      'loader',
      'utf8'
    );
    const output = await createWindowsPackage(
      temporaryRoot,
      {
        packageName: 'sample-app',
        version: '0.1.0'
      },
      executable,
      {
        bundleWebview2Loader: true,
        externalWeb: true
      }
    );
    assert.equal(
      output,
      path.join(
        temporaryRoot,
        '.ksm',
        'dist',
        'packages',
        'sample-app-0.1.0-windows.zip'
      )
    );
    assert.deepEqual(
      zipEntryNames(await readFile(output)),
      [
        'README.txt',
        'WebView2Loader.dll',
        'sample-app.exe',
        'web/index.html'
      ]
    );
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('Android packaging rejects missing release signing configuration', async () => {
  const environment = saveEnvironment([
    'KASUMI_ANDROID_KEYSTORE',
    'KASUMI_ANDROID_KEYSTORE_PASSWORD',
    'KASUMI_ANDROID_KEY_ALIAS',
    'KASUMI_ANDROID_KEY_PASSWORD'
  ]);
  try {
    for (const name of environment.keys()) {
      delete process.env[name];
    }
    await assert.rejects(
      packageAndroid('unused', {
        packageName: 'dev.kasumi.sample',
        version: '0.1.0',
        commands: ['ping', 'capabilities.query']
      }),
      /Android release signing requires/
    );
  } finally {
    restoreEnvironment(environment);
  }
});

test('selects the newest Android build-tools by numeric version', async () => {
  const temporaryRoot = await mkdtemp(
    path.join(tmpdir(), 'kasumi-apksigner-test-')
  );
  const environment = saveEnvironment([
    'APKSIGNER',
    'ANDROID_HOME',
    'ANDROID_SDK_ROOT'
  ]);
  const executable = process.platform === 'win32'
    ? 'apksigner.bat'
    : 'apksigner';
  try {
    for (const version of ['9.0.0', '34.0.0']) {
      const directory = path.join(temporaryRoot, 'build-tools', version);
      await mkdir(directory, { recursive: true });
      await writeFile(path.join(directory, executable), '', 'utf8');
    }
    delete process.env.APKSIGNER;
    delete process.env.ANDROID_SDK_ROOT;
    process.env.ANDROID_HOME = temporaryRoot;
    const signer = await findApkSigner();
    assert.match(signer, /34\.0\.0/);
  } finally {
    restoreEnvironment(environment);
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('loads a valid application configuration', async () => {
  const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'kasumi-cli-test-'));
  try {
    const configPath = path.join(temporaryRoot, 'kasumi.config.json');
    await writeFile(
      configPath,
      JSON.stringify({
        windows: {
          packageName: 'sample-app',
          version: '0.1.0',
          commands: ['ping', 'capabilities.query']
        },
        android: {
          packageName: 'dev.kasumi.sample',
          version: '0.1.0',
          commands: ['ping', 'capabilities.query']
        }
      }),
      'utf8'
    );
    const config = await readConfig(temporaryRoot, 'kasumi.config.json');
    assert.equal(config.windows.packageName, 'sample-app');
    assert.equal(config.android.packageName, 'dev.kasumi.sample');
    assert.equal(config.windows.decorations, true);
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('accepts hidden Windows decorations', async () => {
  const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'kasumi-cli-test-'));
  try {
    const configPath = path.join(temporaryRoot, 'kasumi.config.json');
    await writeFile(
      configPath,
      JSON.stringify({
        windows: {
          packageName: 'sample-app',
          version: '0.1.0',
          commands: ['ping', 'capabilities.query'],
          decorations: false
        },
        android: {
          packageName: 'dev.kasumi.sample',
          version: '0.1.0',
          commands: ['ping', 'capabilities.query']
        }
      }),
      'utf8'
    );
    const config = await readConfig(temporaryRoot, 'kasumi.config.json');
    assert.equal(config.windows.decorations, false);
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('rejects invalid Windows decorations values', async () => {
  const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'kasumi-cli-test-'));
  try {
    const configPath = path.join(temporaryRoot, 'kasumi.config.json');
    await writeFile(
      configPath,
      JSON.stringify({
        windows: {
          packageName: 'sample-app',
          version: '0.1.0',
          commands: ['ping', 'capabilities.query'],
          decorations: 'false'
        },
        android: {
          packageName: 'dev.kasumi.sample',
          version: '0.1.0',
          commands: ['ping', 'capabilities.query']
        }
      }),
      'utf8'
    );
    await assert.rejects(
      readConfig(temporaryRoot, 'kasumi.config.json'),
      /decorations must be a boolean/
    );
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('rejects decorations on Android', async () => {
  const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'kasumi-cli-test-'));
  try {
    const configPath = path.join(temporaryRoot, 'kasumi.config.json');
    await writeFile(
      configPath,
      JSON.stringify({
        windows: {
          packageName: 'sample-app',
          version: '0.1.0',
          commands: ['ping', 'capabilities.query']
        },
        android: {
          packageName: 'dev.kasumi.sample',
          version: '0.1.0',
          commands: ['ping', 'capabilities.query'],
          decorations: false
        }
      }),
      'utf8'
    );
    await assert.rejects(
      readConfig(temporaryRoot, 'kasumi.config.json'),
      /unknown fields: decorations/
    );
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('expands all to the platform command matrix', async () => {
  const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'kasumi-cli-test-'));
  try {
    const configPath = path.join(temporaryRoot, 'kasumi.config.json');
    await writeFile(
      configPath,
      JSON.stringify({
        windows: {
          packageName: 'sample-app',
          version: '0.1.0',
          commands: 'all'
        },
        android: {
          packageName: 'dev.kasumi.sample',
          version: '0.1.0',
          commands: 'all'
        }
      }),
      'utf8'
    );
    const config = await readConfig(temporaryRoot, 'kasumi.config.json');
    assert.equal(config.windows.commands.length, 33);
    assert.equal(config.android.commands.length, 30);
    assert.equal(config.windows.commands.includes('device.getInfo'), false);
    assert.equal(config.android.commands.includes('device.getInfo'), true);
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('rejects cross-platform commands', async () => {
  const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'kasumi-cli-test-'));
  try {
    const configPath = path.join(temporaryRoot, 'kasumi.config.json');
    await writeFile(
      configPath,
      JSON.stringify({
        windows: {
          packageName: 'sample-app',
          version: '0.1.0',
          commands: ['ping', 'capabilities.query', 'device.getInfo']
        },
        android: {
          packageName: 'dev.kasumi.sample',
          version: '0.1.0',
          commands: ['ping', 'capabilities.query']
        }
      }),
      'utf8'
    );
    await assert.rejects(
      readConfig(temporaryRoot, 'kasumi.config.json'),
      ConfigError
    );
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('CLI command data matches the protocol platform matrix', async () => {
  const data = JSON.parse(
    await readFile(
      path.join(repositoryRoot, 'packages', 'kasumi-cli', 'data', 'commands.json'),
      'utf8'
    )
  );
  const protocol = JSON.parse(
    await readFile(
      path.join(repositoryRoot, 'protocol', 'commands.json'),
      'utf8'
    )
  );
  for (const platform of ['windows', 'android']) {
    const expected = Object.entries(protocol.commands)
      .filter(([, command]) => command.platforms.includes(platform))
      .map(([name]) => name)
      .sort();
    assert.deepEqual([...data[platform]].sort(), expected);
  }
});

function zipEntryNames(buffer) {
  const endOffset = buffer.lastIndexOf(
    Buffer.from([0x50, 0x4b, 0x05, 0x06])
  );
  assert.notEqual(endOffset, -1);
  const entryCount = buffer.readUInt16LE(endOffset + 10);
  let offset = buffer.readUInt32LE(endOffset + 16);
  const names = [];
  for (let index = 0; index < entryCount; index += 1) {
    assert.equal(buffer.readUInt32LE(offset), 0x02014b50);
    const nameLength = buffer.readUInt16LE(offset + 28);
    const extraLength = buffer.readUInt16LE(offset + 30);
    const commentLength = buffer.readUInt16LE(offset + 32);
    names.push(buffer.subarray(offset + 46, offset + 46 + nameLength).toString('utf8'));
    offset += 46 + nameLength + extraLength + commentLength;
  }
  return names;
}

function saveEnvironment(names) {
  return new Map(names.map((name) => [name, process.env[name]]));
}

function restoreEnvironment(environment) {
  for (const [name, value] of environment) {
    if (value === undefined) {
      delete process.env[name];
    } else {
      process.env[name] = value;
    }
  }
}
