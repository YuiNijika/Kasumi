import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import {
  mkdtemp,
  readFile,
  readdir,
  rm,
  stat
} from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { parseArgs } from '../src/args.mjs';
import { run } from '../src/cli.mjs';
import { platformCommands } from '../src/commands.mjs';
import { commandInvocation } from '../src/install.mjs';
import {
  androidPackageName,
  normalizeProjectName
} from '../src/naming.mjs';

const testDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(testDirectory, '..', '..', '..');

test('parseArgs selects both platforms by default', () => {
  const options = parseArgs(['my-app']);
  assert.equal(options.directory, 'my-app');
  assert.deepEqual([...options.platforms].sort(), ['android', 'windows']);
  assert.equal(options.install, true);
  assert.equal(options.sdk, true);
  assert.equal(options.kasumiVersion, undefined);
});

test('parseArgs supports short help and version flags', () => {
  assert.equal(parseArgs(['-h']).help, true);
  assert.equal(parseArgs(['-v']).version, true);
});

test('parseArgs supports source-only and single-platform initialization', () => {
  const options = parseArgs([
    'my-app',
    '--android',
    '--no-install',
    '--no-sdk',
    '--package-manager',
    'npm'
  ]);
  assert.deepEqual([...options.platforms], ['android']);
  assert.equal(options.install, false);
  assert.equal(options.sdk, false);
  assert.equal(options.packageManager, 'npm');
});

test('naming derives valid npm and Android names', () => {
  assert.equal(normalizeProjectName('My App'), 'my-app');
  assert.equal(androidPackageName('my-app'), 'dev.kasumi.my_app');
  assert.equal(androidPackageName('2fa-tool'), 'dev.kasumi.app_2fa_tool');
  assert.equal(androidPackageName('app'), 'dev.kasumi.apps.app');
});

test('wraps Windows package manager scripts through the system shell', () => {
  const invocation = commandInvocation('pnpm.cmd', ['install']);
  if (process.platform === 'win32') {
    assert.equal(invocation.command, process.env.ComSpec ?? 'cmd.exe');
    assert.deepEqual(invocation.args.slice(0, 3), ['/d', '/s', '/c']);
    assert.match(invocation.args[3], /pnpm\.cmd install/);
    return;
  }
  assert.equal(invocation.command, 'pnpm.cmd');
  assert.deepEqual(invocation.args, ['install']);
});

test('template command allowlists match the protocol', async () => {
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
    assert.deepEqual([...platformCommands[platform]].sort(), expected);
  }
});

test('initializes a source-only project without framework source', async () => {
  const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'create-kasumi-test-'));
  const destination = path.join(temporaryRoot, 'my-app');
  try {
    await run([
      destination,
      '--no-sdk',
      '--no-install',
      '--yes'
    ]);
    const config = JSON.parse(
      await readFile(path.join(destination, 'kasumi.config.json'), 'utf8')
    );
    assert.equal(config.windows.packageName, 'my-app');
    assert.equal(config.windows.version, '0.1.0');
    assert.equal(config.android.packageName, 'dev.kasumi.my_app');
    assert.equal(config.android.version, '0.1.0');
    assert.equal(config.windows.commands, 'all');
    assert.equal(config.windows.decorations, true);
    assert.equal(config.android.commands, 'all');
    assert.equal(
      await exists(
        path.join(
          destination,
          'android',
          'app',
          'src',
          'main',
          'java',
          'dev',
          'kasumi',
          'my_app',
          'MainActivity.kt'
        )
      ),
      true
    );
    assert.equal(
      await exists(path.join(destination, 'windows', 'src')),
      false
    );
    assert.equal(await exists(path.join(destination, '.gitignore')), true);
    assert.equal(
      await exists(
        path.join(
          destination,
          'android',
          'gradle',
          'wrapper',
          'gradle-wrapper.jar'
        )
      ),
      true
    );
    assert.equal(
      await exists(path.join(destination, 'android', 'gradlew.bat')),
      true
    );
    const templateWrapper = path.join(
      testDirectory,
      '..',
      'templates',
      'basic',
      'android',
      'gradle',
      'wrapper',
      'gradle-wrapper.jar'
    );
    const generatedWrapper = path.join(
      destination,
      'android',
      'gradle',
      'wrapper',
      'gradle-wrapper.jar'
    );
    assert.equal(
      sha256(await readFile(generatedWrapper)),
      sha256(await readFile(templateWrapper))
    );
    assert.equal(
      await readFile(
        path.join(destination, 'pnpm-workspace.yaml'),
        'utf8'
      ),
      'allowBuilds:\n  esbuild: true\n'
    );
    const vcpkg = JSON.parse(
      await readFile(
        path.join(destination, 'windows', 'vcpkg.json'),
        'utf8'
      )
    );
    assert.equal(vcpkg['version-string'], '0.1.0');
    assert.deepEqual(vcpkg.dependencies, ['nlohmann-json', 'webview2']);
    const files = await collectFiles(destination);
    for (const file of files) {
      if (path.extname(file).toLowerCase() === '.jar') {
        continue;
      }
      const content = await readFile(file, 'utf8');
      assert.equal(/__[A-Z_]+__/.test(content), false, file);
    }
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('refuses to initialize a non-empty directory without force', async () => {
  const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'create-kasumi-test-'));
  const destination = path.join(temporaryRoot, 'occupied');
  try {
    await run([destination, '--no-sdk', '--no-install', '--yes']);
    await assert.rejects(
      run([destination, '--no-sdk', '--no-install', '--yes']),
      /not empty/
    );
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

async function collectFiles(directory) {
  const result = [];
  const entries = await readdir(directory, { withFileTypes: true });
  for (const entry of entries) {
    const file = path.join(directory, entry.name);
    if (entry.isDirectory()) {
      result.push(...(await collectFiles(file)));
    } else {
      result.push(file);
    }
  }
  return result;
}

async function exists(file) {
  try {
    await stat(file);
    return true;
  } catch {
    return false;
  }
}

function sha256(content) {
  return createHash('sha256').update(content).digest('hex');
}
