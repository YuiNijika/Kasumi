import assert from 'node:assert/strict';
import {
  mkdir,
  mkdtemp,
  readFile,
  rm,
  writeFile
} from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { parseArgs } from '../src/args.mjs';
import { findGradle } from '../src/android.mjs';
import { ConfigError, readConfig } from '../src/config.mjs';
import {
  injectContentSecurityPolicy,
  readFrontendSettings
} from '../src/frontend.mjs';
import { commandInvocation } from '../src/process.mjs';

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
    assert.equal(config.windows.commands.length, 28);
    assert.equal(config.android.commands.length, 25);
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
