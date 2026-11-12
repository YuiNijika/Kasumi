import assert from 'node:assert/strict';
import {
  mkdir,
  mkdtemp,
  rm,
  writeFile
} from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';

import {
  collectDevelopmentSnapshot,
  compareSnapshots,
  watchDevelopmentChanges
} from '../src/dev-watcher.mjs';

test('collects backend inputs and ignores generated directories', async () => {
  const temporaryRoot = await mkdtemp(
    path.join(tmpdir(), 'kasumi-dev-watcher-')
  );
  try {
    const configPath = path.join(temporaryRoot, 'kasumi.config.json');
    const sourcePath = path.join(
      temporaryRoot,
      'windows',
      'src',
      'main.cpp'
    );
    const buildPath = path.join(
      temporaryRoot,
      'windows',
      'build',
      'generated.cpp'
    );
    const manifestPath = path.join(
      temporaryRoot,
      'extensions',
      'example',
      'manifest.json'
    );
    await mkdir(path.dirname(sourcePath), { recursive: true });
    await mkdir(path.dirname(buildPath), { recursive: true });
    await mkdir(path.dirname(manifestPath), { recursive: true });
    await writeFile(configPath, '{}', 'utf8');
    await writeFile(sourcePath, 'int main() { return 0; }\n', 'utf8');
    await writeFile(buildPath, 'generated\n', 'utf8');
    await writeFile(manifestPath, '{"id":"example"}\n', 'utf8');

    const snapshot = await collectDevelopmentSnapshot({
      projectRoot: temporaryRoot,
      configPath,
      extensionPaths: [manifestPath]
    });
    assert.equal(snapshot.has(path.normalize(configPath)), true);
    assert.equal(snapshot.has(path.normalize(sourcePath)), true);
    assert.equal(snapshot.has(path.normalize(manifestPath)), true);
    assert.equal(snapshot.has(path.normalize(buildPath)), false);
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('compares added, modified, and removed files', () => {
  const changes = compareSnapshots(
    new Map([
      ['a.cpp', '1:1'],
      ['b.cpp', '1:1']
    ]),
    new Map([
      ['b.cpp', '2:1'],
      ['c.cpp', '1:1']
    ])
  );
  assert.deepEqual(changes, [
    { path: 'a.cpp', kind: 'removed' },
    { path: 'b.cpp', kind: 'modified' },
    { path: 'c.cpp', kind: 'added' }
  ]);
});

test('reports configuration changes without generated output noise', async () => {
  const temporaryRoot = await mkdtemp(
    path.join(tmpdir(), 'kasumi-dev-watcher-config-')
  );
  const configPath = path.join(temporaryRoot, 'kasumi.config.json');
  let watcher;
  try {
    await mkdir(path.join(temporaryRoot, 'windows', 'build'), {
      recursive: true
    });
    await writeFile(configPath, '{"version":1}\n', 'utf8');
    const changes = [];
    watcher = await watchDevelopmentChanges({
      projectRoot: temporaryRoot,
      configPath,
      intervalMs: 20,
      debounceMs: 20,
      onChange(items) {
        changes.push(...items);
      }
    });

    await writeFile(configPath, '{"version":200}\n', 'utf8');
    await waitFor(() => changes.length > 0);
    assert.equal(changes[0].path, path.normalize(configPath));

    const count = changes.length;
    await writeFile(
      path.join(temporaryRoot, 'windows', 'build', 'generated.cpp'),
      'generated output\n',
      'utf8'
    );
    await waitForDelay(120);
    assert.equal(changes.length, count);
  } finally {
    await watcher?.close();
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('serializes reloads and queues changes made during a rebuild', async () => {
  const temporaryRoot = await mkdtemp(
    path.join(tmpdir(), 'kasumi-dev-watcher-queue-')
  );
  let watcher;
  try {
    const windowsDirectory = path.join(temporaryRoot, 'windows');
    const configPath = path.join(temporaryRoot, 'kasumi.config.json');
    await mkdir(windowsDirectory, { recursive: true });
    await writeFile(configPath, '{"version":1}\n', 'utf8');
    await writeFile(path.join(windowsDirectory, 'main.cpp'), 'v1\n', 'utf8');

    let activeReloads = 0;
    let maxActiveReloads = 0;
    let reloads = 0;
    watcher = await watchDevelopmentChanges({
      projectRoot: temporaryRoot,
      configPath,
      intervalMs: 20,
      debounceMs: 20,
      async onChange() {
        reloads += 1;
        activeReloads += 1;
        maxActiveReloads = Math.max(maxActiveReloads, activeReloads);
        await waitForDelay(100);
        activeReloads -= 1;
      }
    });

    await writeFile(path.join(windowsDirectory, 'main.cpp'), 'v2\n', 'utf8');
    await waitFor(() => reloads === 1);
    await writeFile(path.join(windowsDirectory, 'main.cpp'), 'v3\n', 'utf8');
    await waitFor(() => reloads === 2);
    assert.equal(maxActiveReloads, 1);
  } finally {
    await watcher?.close();
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

async function waitFor(predicate, timeoutMs = 3000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (predicate()) {
      return;
    }
    await waitForDelay(20);
  }
  throw new Error('Timed out waiting for the watcher.');
}

function waitForDelay(delayMs) {
  return new Promise((resolve) => setTimeout(resolve, delayMs));
}
