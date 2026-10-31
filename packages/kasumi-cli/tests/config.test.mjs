import assert from 'node:assert/strict';
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { parseArgs } from '../src/args.mjs';
import { ConfigError, readConfig } from '../src/config.mjs';

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
