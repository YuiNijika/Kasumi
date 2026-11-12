import assert from 'node:assert/strict';
import {
  mkdir,
  mkdtemp,
  readFile,
  realpath,
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
  packageAndroid,
  runAndroidDevelopment
} from '../src/android.mjs';
import { ConfigError, readConfig } from '../src/config.mjs';
import {
  contentSecurityPolicy,
  configuredConnectSources,
  injectContentSecurityPolicy,
  readFrontendSettings,
} from '../src/frontend.mjs';
import { createZipArchive } from '../src/archive.mjs';
import {
  clearDevelopmentProcess,
  hasDevelopmentState,
  recordDevelopmentProcess
} from '../src/dev-runtime.mjs';
import { collectDoctorChecks } from '../src/main.mjs';
import {
  applicationOutputDirectory,
  packageOutputDirectory
} from '../src/output.mjs';
import { commandInvocation } from '../src/process.mjs';
import { resolveProjectIcon } from '../src/icons.mjs';
import {
  cmakeCacheMatches,
  createVcpkgEnvironment,
  createWindowsPackage,
  findVcpkgToolchain,
  buildVcpkgAssetSources,
  supportsWindowsDevMode
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

test('parseArgs accepts a Windows installer type', () => {
  assert.equal(parseArgs(['package', '--installer', 'nsis']).installer, 'nsis');
  assert.equal(parseArgs(['package', '--installer=msi']).installer, 'msi');
  assert.throws(
    () => parseArgs(['package', '--installer', 'deb']),
    /Invalid installer/
  );
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
    assert.equal(
      environment.installed,
      path.join(cacheRoot, 'installed')
    );
    assert.match(
      environment.VCPKG_BINARY_SOURCES,
      /readwrite$/
    );
    assert.match(
      environment.X_VCPKG_ASSET_SOURCES,
      /^clear;x-script,/
    );
    assert.match(
      environment.X_VCPKG_ASSET_SOURCES,
      /\{url\} \{sha512\} \{dst\}$/
    );
    for (const directory of [
      environment.X_VCPKG_REGISTRIES_CACHE,
      environment.VCPKG_DEFAULT_BINARY_CACHE,
      environment.VCPKG_DOWNLOADS,
      environment.installed
    ]) {
      assert.equal((await stat(directory)).isDirectory(), true);
    }
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('builds a quoted vcpkg asset source command', () => {
  const sources = buildVcpkgAssetSources();
  assert.match(sources, /^clear;x-script,"/);
  assert.match(sources, /vcpkg-download\.mjs/);
  assert.match(sources, /\{url\} \{sha512\} \{dst\}$/);
});

test('tracks and clears development process state', async () => {
  const temporaryRoot = await mkdtemp(
    path.join(tmpdir(), 'kasumi-dev-state-test-')
  );
  try {
    await recordDevelopmentProcess(temporaryRoot, 'frontend', 1234);
    assert.equal(await hasDevelopmentState(temporaryRoot), true);
    await clearDevelopmentProcess(temporaryRoot, 'frontend', 9999);
    assert.equal(await hasDevelopmentState(temporaryRoot), true);
    await clearDevelopmentProcess(temporaryRoot, 'frontend', 1234);
    assert.equal(await hasDevelopmentState(temporaryRoot), false);
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('reuses CMake caches across Windows path separators', async () => {
  const temporaryRoot = await mkdtemp(
    path.join(tmpdir(), 'kasumi-cmake-cache-test-')
  );
  try {
    await writeFile(
      path.join(temporaryRoot, 'CMakeCache.txt'),
      [
        'CMAKE_BUILD_TYPE:STRING=Release',
        'VCPKG_INSTALLED_DIR:PATH=D:/project/.ksm/vcpkg/installed',
        ''
      ].join('\n'),
      'utf8'
    );
    assert.equal(
      await cmakeCacheMatches(temporaryRoot, [
        '-DCMAKE_BUILD_TYPE=Release',
        '-DVCPKG_INSTALLED_DIR=D:\\project\\.ksm\\vcpkg\\installed'
      ]),
      true
    );
    assert.equal(
      await cmakeCacheMatches(temporaryRoot, [
        '-DCMAKE_BUILD_TYPE=Debug'
      ]),
      false
    );
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('template development builds do not require production web assets', async () => {
  const cmake = await readFile(
    path.join(
      repositoryRoot,
      'packages',
      'create-kasumi',
      'templates',
      'basic',
      'windows',
      'CMakeLists.txt'
    ),
    'utf8'
  );
  assert.match(cmake, /option\(\s+KASUMI_DEV_MODE/);
  assert.match(cmake, /if\(KASUMI_DEV_MODE\)/);
});

test('detects whether a generated Windows host supports dev mode', async () => {
  const temporaryRoot = await mkdtemp(
    path.join(tmpdir(), 'kasumi-windows-dev-mode-test-')
  );
  try {
    const windowsDirectory = path.join(temporaryRoot, 'windows');
    await mkdir(windowsDirectory, { recursive: true });
    const cmakePath = path.join(windowsDirectory, 'CMakeLists.txt');
    await writeFile(cmakePath, 'project(kasumi_app)\n', 'utf8');
    assert.equal(await supportsWindowsDevMode(temporaryRoot), false);
    await writeFile(
      cmakePath,
      'option(KASUMI_DEV_MODE "development" OFF)\n',
      'utf8'
    );
    assert.equal(await supportsWindowsDevMode(temporaryRoot), true);
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

test('uses configured Android WebView development entries', async () => {
  const temporaryRoot = await mkdtemp(
    path.join(tmpdir(), 'kasumi-frontend-test-')
  );
  try {
    await writeFile(
      path.join(temporaryRoot, 'package.json'),
      JSON.stringify({
        dependencies: { next: '^16.0.0' },
        scripts: {
          'frontend:dev': 'next dev',
          'frontend:build': 'next build'
        }
      }),
      'utf8'
    );
    const settings = await readFrontendSettings(temporaryRoot, {
      productionEntry: 'https://app.local/start.html',
      developmentEntries: [
        'http://localhost:5173/start.html',
        'http://localhost:5173/'
      ],
      supportedHtmlFiles: ['index.html', 'start.html']
    });
    assert.equal(settings.devUrl, 'http://localhost:5173/');
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
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

test('adds configured HTTP origins to the injected CSP', async () => {
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
    const connectSources = configuredConnectSources({
      windows: {
        http: { allowedOrigins: ['https://api.github.com'] }
      },
      android: {
        http: { allowedOrigins: ['https://api.miomoe.cn'] }
      }
    });
    assert.deepEqual(connectSources, [
      'https://api.github.com',
      'https://api.miomoe.cn'
    ]);
    assert.match(
      contentSecurityPolicy(connectSources),
      /connect-src 'self' https:\/\/api\.github\.com https:\/\/api\.miomoe\.cn/
    );
    await injectContentSecurityPolicy(temporaryRoot, connectSources);
    await injectContentSecurityPolicy(temporaryRoot, connectSources);
    const html = await readFile(
      path.join(outputDirectory, 'index.html'),
      'utf8'
    );
    assert.equal(
      html.match(/http-equiv="Content-Security-Policy"/g)?.length,
      1
    );
    assert.match(
      html,
      /connect-src 'self' https:\/\/api\.github\.com https:\/\/api\.miomoe\.cn/
    );
    assert.match(
      html,
      /img-src 'self' data: https:\/\/api\.github\.com https:\/\/api\.miomoe\.cn/
    );
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
        commands: ['ping', 'capabilities.query'],
        webview: {
          productionEntry: 'https://app.local/index.html',
          developmentEntries: ['http://localhost:5173/index.html'],
          supportedHtmlFiles: ['index.html']
        }
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

test('loads an extension manifest into the command allowlist', async () => {
 const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'kasumi-cli-test-'));
 try {
 const manifestPath = path.join(temporaryRoot, 'library.extension.json');
 await writeFile(
 manifestPath,
 JSON.stringify({
 id: 'library',
 commands: [
 {
 name: 'app.library.scan.start',
 platforms: ['windows', 'android'],
 permissions: ['fs.read'],
 args: { type: 'object', properties: {}, required: [] },
 result: { type: 'object', properties: {}, required: [] }
 }
 ],
 events: [
 {
 name: 'app.library.scan.progress',
 platforms: ['windows', 'android'],
 payload: { type: 'object', properties: {}, required: [] }
 }
 ]
 }),
 'utf8'
 );
 await writeFile(
 path.join(temporaryRoot, 'kasumi.config.json'),
 JSON.stringify({
 extensions: [
 { id: 'library', manifest: 'library.extension.json', platforms: ['windows', 'android'] }
 ],
 windows: {
 packageName: 'sample-app',
 version: '0.1.0',
 commands: ['ping', 'capabilities.query', 'app.library.scan.start']
 },
 android: {
 packageName: 'dev.kasumi.sample',
 version: '0.1.0',
 commands: ['ping', 'capabilities.query', 'app.library.scan.start']
 }
 }),
 'utf8'
 );
 const config = await readConfig(temporaryRoot, 'kasumi.config.json');
 assert.deepEqual(config.windows.commands, [
 'ping',
 'capabilities.query',
 'app.library.scan.start'
 ]);
 assert.equal(config.extensions[0].events[0].name, 'app.library.scan.progress');
 } finally {
 await rm(temporaryRoot, { recursive: true, force: true });
 }
});

test('expands all commands with extension commands and permissions', async () => {
  const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'kasumi-cli-test-'));
  try {
    await writeFile(
      path.join(temporaryRoot, 'musicstorm.extension.json'),
      JSON.stringify({
        id: 'musicstorm',
        commands: [
          {
            name: 'app.musicstorm.command',
            platforms: ['windows', 'android'],
            permissions: ['app.musicstorm'],
            args: { type: 'object', properties: {}, required: [] },
            result: { type: 'object', properties: {}, required: [] }
          }
        ],
        events: [
          {
            name: 'app.musicstorm.event',
            platforms: ['windows', 'android'],
            payload: { type: 'object', properties: {}, required: [] }
          }
        ]
      }),
      'utf8'
    );
    await writeFile(
      path.join(temporaryRoot, 'kasumi.config.json'),
      JSON.stringify({
        extensions: [
          {
            id: 'musicstorm',
            manifest: 'musicstorm.extension.json',
            platforms: ['windows', 'android']
          }
        ],
        windows: {
          packageName: 'music-storm',
          version: '0.1.0',
          commands: 'all',
          permissions: 'all'
        },
        android: {
          packageName: 'dev.kasumi.music_storm',
          version: '0.1.0',
          commands: 'all',
          permissions: 'all'
        }
      }),
      'utf8'
    );
    const config = await readConfig(temporaryRoot, 'kasumi.config.json');
    assert.ok(config.windows.commands.includes('app.musicstorm.command'));
    assert.ok(config.android.commands.includes('app.musicstorm.command'));
    assert.ok(config.android.permissions.includes('app.musicstorm'));
    assert.equal(config.extensions[0].events[0].name, 'app.musicstorm.event');
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('rejects an extension command that exceeds its declared platforms', async () => {
 const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'kasumi-cli-test-'));
 try {
 await writeFile(
 path.join(temporaryRoot, 'library.extension.json'),
 JSON.stringify({
 id: 'library',
 commands: [
 {
 name: 'app.library.scan.start',
 platforms: ['windows', 'android'],
 permissions: ['fs.read']
 }
 ],
 events: []
 }),
 'utf8'
 );
 await writeFile(
 path.join(temporaryRoot, 'kasumi.config.json'),
 JSON.stringify({
 extensions: [
 { id: 'library', manifest: 'library.extension.json', platforms: ['windows'] }
 ],
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
 await assert.rejects(
 readConfig(temporaryRoot, 'kasumi.config.json'),
 /platforms exceeds the extension platform list/
 );
 } finally {
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
    assert.deepEqual(config.android.webview, {
      productionEntry: 'https://app.local/index.html',
      developmentEntries: [
        'http://localhost:5173/index.html',
        'http://localhost:5173/',
        'http://localhost:5173'
      ],
      supportedHtmlFiles: ['index.html']
    });
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('accepts Android WebView entry configuration', async () => {
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
          webview: {
            productionEntry: 'https://app.local/start.html',
            developmentEntries: [
              'http://localhost:5173/start.html',
              'http://localhost:5173/'
            ],
            supportedHtmlFiles: ['index.html', 'start.html']
          }
        }
      }),
      'utf8'
    );
    const config = await readConfig(temporaryRoot, 'kasumi.config.json');
    assert.equal(
      config.android.webview.productionEntry,
      'https://app.local/start.html'
    );
    assert.deepEqual(config.android.webview.developmentEntries, [
      'http://localhost:5173/start.html',
      'http://localhost:5173/'
    ]);
    assert.deepEqual(config.android.webview.supportedHtmlFiles, [
      'index.html',
      'start.html'
    ]);
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('rejects unsafe Android WebView entry configuration', async () => {
  const invalidConfigurations = [
    {
      productionEntry: 'https://app.local/index.html?next=1'
    },
    {
      productionEntry: 'https://app.local/../index.html'
    },
    {
      developmentEntries: [
        'http://localhost:5173/index.html',
        'http://localhost:5173/'
      ],
      supportedHtmlFiles: ['other.html']
    },
    {
      developmentEntries: [
        'http://localhost:5173/index.html',
        'http://localhost:5173/secret.html'
      ]
    }
  ];
  for (const webview of invalidConfigurations) {
    const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'kasumi-cli-test-'));
    try {
      await writeFile(
        path.join(temporaryRoot, 'kasumi.config.json'),
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
            webview
          }
        }),
        'utf8'
      );
      await assert.rejects(
        readConfig(temporaryRoot, 'kasumi.config.json'),
        /webview/i
      );
    } finally {
      await rm(temporaryRoot, { recursive: true, force: true });
    }
  }
});

test('rejects Android WebView configuration on Windows', async () => {
  const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'kasumi-cli-test-'));
  try {
    await writeFile(
      path.join(temporaryRoot, 'kasumi.config.json'),
      JSON.stringify({
        windows: {
          packageName: 'sample-app',
          version: '0.1.0',
          commands: ['ping', 'capabilities.query'],
          webview: {}
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
      /unknown fields: webview/
    );
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

test('accepts a WebView2 fallback background color', async () => {
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
          window: { backgroundColor: '#0F1117' }
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
    assert.equal(config.windows.window.backgroundColor, '#0F1117');
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('accepts Windows error page customization', async () => {
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
          errorPage: {
            backgroundColor: '#0B0B0D',
            cardColor: '#15171D',
            borderColor: '#2B2F38',
            textColor: '#F5F6FA',
            mutedTextColor: '#9CA3AF',
            accentColor: '#8B5CF6',
            errorColor: '#FF6B6B',
            title: 'Music Storm 启动失败',
            hint: '请检查网络、WebView2 运行时或应用配置后重试'
          }
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
    assert.deepEqual(config.windows.errorPage, {
      backgroundColor: '#0B0B0D',
      cardColor: '#15171D',
      borderColor: '#2B2F38',
      textColor: '#F5F6FA',
      mutedTextColor: '#9CA3AF',
      accentColor: '#8B5CF6',
      errorColor: '#FF6B6B',
      title: 'Music Storm 启动失败',
      hint: '请检查网络、WebView2 运行时或应用配置后重试'
    });
    assert.deepEqual(config.android.errorPage, undefined);
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('rejects unsafe Windows error page customization', async () => {
  const invalidErrorPages = [
    { backgroundColor: '#12' },
    { title: '失败"提示' },
    { hint: '第一行\n第二行' },
    { unknown: '#FFFFFF' }
  ];
  for (const errorPage of invalidErrorPages) {
    const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'kasumi-cli-test-'));
    try {
      await writeFile(
        path.join(temporaryRoot, 'kasumi.config.json'),
        JSON.stringify({
          windows: {
            packageName: 'sample-app',
            version: '0.1.0',
            commands: ['ping', 'capabilities.query'],
            errorPage
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
        /errorPage|safe characters|RRGGBB|unknown fields/
      );
    } finally {
      await rm(temporaryRoot, { recursive: true, force: true });
    }
  }
});

test('rejects an invalid WebView2 fallback background color', async () => {
  const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'kasumi-cli-test-'));
  try {
    await writeFile(
      path.join(temporaryRoot, 'kasumi.config.json'),
      JSON.stringify({
        windows: {
          packageName: 'sample-app',
          version: '0.1.0',
          commands: ['ping', 'capabilities.query'],
          window: { backgroundColor: '#12' }
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
      /backgroundColor must be #RRGGBB or #AARRGGBB/
    );
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('accepts Windows window size limits', async () => {
  const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'kasumi-cli-test-'));
  try {
    await writeFile(
      path.join(temporaryRoot, 'kasumi.config.json'),
      JSON.stringify({
        windows: {
          packageName: 'sample-app',
          version: '0.1.0',
          commands: ['ping', 'capabilities.query'],
          window: {
            width: 1280,
            height: 800,
            minWidth: 960,
            minHeight: 600,
            maxWidth: 1600,
            maxHeight: 1000
          }
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
    assert.deepEqual(
      {
        width: config.windows.window.width,
        height: config.windows.window.height,
        minWidth: config.windows.window.minWidth,
        minHeight: config.windows.window.minHeight,
        maxWidth: config.windows.window.maxWidth,
        maxHeight: config.windows.window.maxHeight
      },
      {
        width: 1280,
        height: 800,
        minWidth: 960,
        minHeight: 600,
        maxWidth: 1600,
        maxHeight: 1000
      }
    );
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('rejects inconsistent Windows window size limits', async () => {
  const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'kasumi-cli-test-'));
  try {
    await writeFile(
      path.join(temporaryRoot, 'kasumi.config.json'),
      JSON.stringify({
        windows: {
          packageName: 'sample-app',
          version: '0.1.0',
          commands: ['ping', 'capabilities.query'],
          window: {
            width: 900,
            height: 700,
            minWidth: 960,
            minHeight: 600,
            maxWidth: 900,
            maxHeight: 800
          }
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
      /maxWidth must be greater than or equal to minWidth/
    );
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('honors window.decorations with a top-level override', async () => {
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
          window: { decorations: false, width: 1280 }
        },
        android: {
          packageName: 'dev.kasumi.sample',
          version: '0.1.0',
          commands: ['ping', 'capabilities.query']
        }
      }),
      'utf8'
    );
    const hidden = await readConfig(temporaryRoot, 'kasumi.config.json');
    assert.equal(hidden.windows.decorations, false);
    assert.equal(hidden.windows.window.width, 1280);

    const payload = JSON.parse(await readFile(configPath, 'utf8'));
    payload.windows.decorations = true;
    await writeFile(configPath, JSON.stringify(payload), 'utf8');
    const visible = await readConfig(temporaryRoot, 'kasumi.config.json');
    assert.equal(visible.windows.decorations, true);
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('merges root icons into platform configuration', async () => {
  const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'kasumi-cli-test-'));
  try {
    const configPath = path.join(temporaryRoot, 'kasumi.config.json');
    await writeFile(
      configPath,
      JSON.stringify({
        icons: {
          window: 'icons/app.ico',
          tray: 'icons/tray.ico',
          android: 'icons/android.png'
        },
        windows: {
          packageName: 'sample-app',
          version: '0.1.0',
          commands: ['ping', 'capabilities.query'],
          icons: { tray: 'icons/windows-tray.ico' }
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
    assert.deepEqual(config.windows.icons, {
      window: 'icons/app.ico',
      tray: 'icons/windows-tray.ico',
      android: 'icons/android.png'
    });
    assert.deepEqual(config.android.icons, {
      window: 'icons/app.ico',
      tray: 'icons/tray.ico',
      android: 'icons/android.png'
    });
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('resolves project icons and rejects unsafe paths', async () => {
  const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'kasumi-cli-icon-'));
  try {
    const icon = path.join(temporaryRoot, 'icon.ico');
    const png = path.join(temporaryRoot, 'icon.png');
    await writeFile(icon, 'icon', 'utf8');
    await writeFile(png, 'png', 'utf8');
    assert.equal(
      await resolveProjectIcon(
        temporaryRoot,
        'icon.ico',
        '.ico',
        'configuration.icons.window'
      ),
      await realpath(icon)
    );
    await assert.rejects(
      resolveProjectIcon(
        temporaryRoot,
        'icon.png',
        '.ico',
        'configuration.icons.window'
      ),
      /must use the \.ico extension/
    );
    await assert.rejects(
      resolveProjectIcon(
        temporaryRoot,
        'missing.ico',
        '.ico',
        'configuration.icons.window'
      ),
      /does not exist/
    );
    await assert.rejects(
      resolveProjectIcon(
        temporaryRoot,
        '../outside.ico',
        '.ico',
        'configuration.icons.window'
      ),
      /must stay inside the project/
    );
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('permissions fail closed and origins are normalized exactly', async () => {
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
          shell: {
            allowedOrigins: ['HTTPS://Example.COM:443']
          },
          http: {
            allowedOrigins: ['https://api.example.com']
          }
        },
        android: {
          packageName: 'dev.kasumi.sample',
          version: '0.1.0',
          commands: ['ping', 'capabilities.query'],
          permissions: 'all'
        }
      }),
      'utf8'
    );
    const config = await readConfig(temporaryRoot, 'kasumi.config.json');
    assert.deepEqual(config.windows.permissions, []);
    assert.deepEqual(
      config.windows.shell.allowedOrigins,
      ['https://example.com']
    );
    assert.deepEqual(
      config.windows.http.allowedOrigins,
      ['https://api.example.com']
    );
    assert.ok(config.android.permissions.length > 0);
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('rejects unsafe origin allowlist entries', async () => {
  for (const origin of [
    'https://user@example.com',
    'https://example.com/path',
    'https://example.com?token=1',
    'https://example.com#fragment'
  ]) {
    const temporaryRoot = await mkdtemp(
      path.join(tmpdir(), 'kasumi-cli-origin-test-')
    );
    try {
      const configPath = path.join(temporaryRoot, 'kasumi.config.json');
      await writeFile(
        configPath,
        JSON.stringify({
          windows: {
            packageName: 'sample-app',
            version: '0.1.0',
            commands: ['ping', 'capabilities.query'],
            shell: {
              allowedOrigins: [origin]
            }
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
        /allowedOrigins/
      );
    } finally {
      await rm(temporaryRoot, { recursive: true, force: true });
    }
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
 const commandData = JSON.parse(
 await readFile(
 path.join(repositoryRoot, 'packages', 'kasumi-cli', 'data', 'commands.json'),
 'utf8'
 )
 );
 assert.equal(config.windows.commands.length, commandData.windows.length);
 assert.equal(config.android.commands.length, commandData.android.length);
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

test('keeps Android development URL within the configured WebView entries', async () => {
  const originalAdb = process.env.ADB;
  try {
    delete process.env.ADB;
    await assert.rejects(
      runAndroidDevelopment(
        'unused',
        {
          packageName: 'dev.kasumi.sample',
          version: '0.1.0',
          webview: {
            productionEntry: 'https://app.local/start.html',
            developmentEntries: ['http://localhost:5173/start.html'],
            supportedHtmlFiles: ['index.html', 'start.html']
          }
        },
        { url: 'http://localhost:5173/index.html' }
      ),
      /not listed in android\.webview\.developmentEntries/
    );
  } finally {
    if (originalAdb === undefined) {
      delete process.env.ADB;
    } else {
      process.env.ADB = originalAdb;
    }
  }
});

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
