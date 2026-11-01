import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import {
  access,
  mkdir,
  mkdtemp,
  readFile,
  rm,
  writeFile
} from 'node:fs/promises';
import http from 'node:http';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';

import { createZipArchive } from '../../kasumi-cli/src/archive.mjs';
import { installSdk } from '../src/sdk.mjs';

test('installs a verified SDK and replaces it atomically', async () => {
  const root = await mkdtemp(path.join(tmpdir(), 'kasumi-sdk-install-'));
  const source = path.join(root, 'source');
  const archive = path.join(root, 'windows.zip');
  const project = path.join(root, 'project');
  const destination = path.join(project, 'vendor', 'windows', 'kasumi');
  let archiveBytes = Buffer.alloc(0);
  let checksumValid = true;

  try {
    await createWindowsSdkTree(source, 'first');
    await createZipArchive(source, archive);
    archiveBytes = await readFile(archive);
    const server = await startReleaseServer({
      archive: () => archiveBytes,
      checksum: () => (checksumValid ? sha256(archiveBytes) : '0'.repeat(64))
    });
    try {
      await installSdk({
        projectDirectory: project,
        platforms: new Set(['windows']),
        version: '0.1.0',
        releaseBaseUrl: server.baseUrl
      });
      assert.equal(
        await readFile(path.join(destination, 'include', 'kasumi', 'version.txt'), 'utf8'),
        'first'
      );

      await rm(source, { recursive: true, force: true });
      await createWindowsSdkTree(source, 'second');
      await createZipArchive(source, archive);
      archiveBytes = await readFile(archive);
      await installSdk({
        projectDirectory: project,
        platforms: new Set(['windows']),
        version: '0.1.0',
        releaseBaseUrl: server.baseUrl
      });
      assert.equal(
        await readFile(path.join(destination, 'include', 'kasumi', 'version.txt'), 'utf8'),
        'second'
      );

      checksumValid = false;
      await assert.rejects(
        installSdk({
          projectDirectory: project,
          platforms: new Set(['windows']),
          version: '0.1.0',
          releaseBaseUrl: server.baseUrl
        }),
        /Checksum mismatch/
      );
      assert.equal(
        await readFile(path.join(destination, 'include', 'kasumi', 'version.txt'), 'utf8'),
        'second'
      );
    } finally {
      await server.close();
    }
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test('rejects archives that escape the SDK staging directory', async () => {
  const root = await mkdtemp(path.join(tmpdir(), 'kasumi-sdk-slip-'));
  const archive = unsafeZip('../escaped.txt', 'escaped');
  const project = path.join(root, 'project');
  const server = await startReleaseServer({
    archive: () => archive,
    checksum: () => sha256(archive)
  });
  try {
    await assert.rejects(
      installSdk({
        projectDirectory: project,
        platforms: new Set(['windows']),
        version: '0.1.0',
        releaseBaseUrl: server.baseUrl
      }),
      /unsafe path/
    );
    await assert.rejects(
      readFile(path.join(root, 'escaped.txt')),
      /ENOENT/
    );
  } finally {
    await server.close();
    await rm(root, { recursive: true, force: true });
  }
});

test('rejects symbolic links before extracting the SDK archive', async () => {
  const root = await mkdtemp(path.join(tmpdir(), 'kasumi-sdk-link-'));
  const archive = unsafeZip('link', '', { symlink: true });
  const server = await startReleaseServer({
    archive: () => archive,
    checksum: () => sha256(archive)
  });
  try {
    await assert.rejects(
      installSdk({
        projectDirectory: path.join(root, 'project'),
        platforms: new Set(['windows']),
        version: '0.1.0',
        releaseBaseUrl: server.baseUrl
      }),
      /symbolic link/
    );
  } finally {
    await server.close();
    await rm(root, { recursive: true, force: true });
  }
});

test('rejects release downloads after five redirects', async () => {
  const root = await mkdtemp(path.join(tmpdir(), 'kasumi-sdk-redirect-'));
  const server = await startRedirectServer();
  try {
    await assert.rejects(
      installSdk({
        projectDirectory: path.join(root, 'project'),
        platforms: new Set(['windows']),
        version: '0.1.0',
        releaseBaseUrl: server.baseUrl
      }),
      /redirect limit/
    );
  } finally {
    await server.close();
    await rm(root, { recursive: true, force: true });
  }
});

test('installs Windows and Android SDKs as one transaction', async () => {
  const root = await mkdtemp(path.join(tmpdir(), 'kasumi-sdk-dual-'));
  const windowsSource = path.join(root, 'windows-source');
  const androidSource = path.join(root, 'android-source');
  const windowsArchive = path.join(root, 'windows.zip');
  const androidArchive = path.join(root, 'android.zip');
  const project = path.join(root, 'project');
  let windowsBytes = Buffer.alloc(0);
  let androidBytes = Buffer.alloc(0);

  try {
    await createWindowsSdkTree(windowsSource, 'dual');
    await createAndroidMavenTree(androidSource, '0.1.0');
    await createZipArchive(windowsSource, windowsArchive);
    await createZipArchive(androidSource, androidArchive);
    windowsBytes = await readFile(windowsArchive);
    androidBytes = await readFile(androidArchive);
    const server = await startReleaseServer({
      archive: () => windowsBytes,
      checksum: () => sha256(windowsBytes),
      androidArchive: () => androidBytes,
      androidChecksum: () => sha256(androidBytes)
    });
    try {
      await installSdk({
        projectDirectory: project,
        platforms: new Set(['windows', 'android']),
        version: '0.1.0',
        releaseBaseUrl: server.baseUrl
      });
      assert.equal(
        await readFile(
          path.join(
            project,
            'vendor',
            'windows',
            'kasumi',
            'include',
            'kasumi',
            'version.txt'
          ),
          'utf8'
        ),
        'dual'
      );
      assert.equal(
        await readFile(
          path.join(
            project,
            'vendor',
            'android',
            'repo',
            'dev',
            'kasumi',
            'kasumi-android',
            '0.1.0',
            'kasumi-android-0.1.0.pom'
          ),
          'utf8'
        ),
        '<project />'
      );
      await assert.rejects(
        access(path.join(project, '.tmp')),
        /ENOENT/
      );
    } finally {
      await server.close();
    }
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test('rolls back every SDK when one destination cannot be committed', async () => {
  const root = await mkdtemp(path.join(tmpdir(), 'kasumi-sdk-rollback-'));
  const windowsSource = path.join(root, 'windows-source');
  const androidSource = path.join(root, 'android-source');
  const windowsArchive = path.join(root, 'windows.zip');
  const androidArchive = path.join(root, 'android.zip');
  const project = path.join(root, 'project');
  const windowsDestination = path.join(
    project,
    'vendor',
    'windows',
    'kasumi'
  );
  let windowsBytes = Buffer.alloc(0);
  let androidBytes = Buffer.alloc(0);

  try {
    await createWindowsSdkTree(windowsSource, 'replacement');
    await createAndroidMavenTree(androidSource, '0.1.0');
    await createWindowsSdkTree(windowsDestination, 'previous');
    await createZipArchive(windowsSource, windowsArchive);
    await createZipArchive(androidSource, androidArchive);
    windowsBytes = await readFile(windowsArchive);
    androidBytes = await readFile(androidArchive);
    await mkdir(path.join(project, 'vendor'), { recursive: true });
    await writeFile(
      path.join(project, 'vendor', 'android'),
      'not a directory',
      'utf8'
    );
    const server = await startReleaseServer({
      archive: () => windowsBytes,
      checksum: () => sha256(windowsBytes),
      androidArchive: () => androidBytes,
      androidChecksum: () => sha256(androidBytes)
    });
    try {
      await assert.rejects(
        installSdk({
          projectDirectory: project,
          platforms: new Set(['windows', 'android']),
          version: '0.1.0',
          releaseBaseUrl: server.baseUrl
        }),
        /EEXIST|ENOTDIR/
      );
      assert.equal(
        await readFile(
          path.join(
            windowsDestination,
            'include',
            'kasumi',
            'version.txt'
          ),
          'utf8'
        ),
        'previous'
      );
      assert.equal(
        await readFile(path.join(project, 'vendor', 'android'), 'utf8'),
        'not a directory'
      );
      await assert.rejects(
        access(path.join(project, '.tmp')),
        /ENOENT/
      );
    } finally {
      await server.close();
    }
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

async function createWindowsSdkTree(directory, version) {
  await mkdir(path.join(directory, 'include', 'kasumi'), { recursive: true });
  await mkdir(
    path.join(directory, 'lib', 'cmake', 'Kasumi'),
    { recursive: true }
  );
  await writeFile(
    path.join(directory, 'include', 'kasumi', 'version.txt'),
    version,
    'utf8'
  );
  await writeFile(
    path.join(directory, 'lib', 'cmake', 'Kasumi', 'KasumiConfig.cmake'),
    '# Kasumi',
    'utf8'
  );
}

async function createAndroidMavenTree(directory, version) {
  const versionDirectory = path.join(
    directory,
    'dev',
    'kasumi',
    'kasumi-android',
    version
  );
  await mkdir(versionDirectory, { recursive: true });
  await writeFile(
    path.join(
      versionDirectory,
      `kasumi-android-${version}.pom`
    ),
    '<project />',
    'utf8'
  );
}

function sha256(content) {
  return createHash('sha256').update(content).digest('hex');
}

function startReleaseServer({
  archive,
  checksum,
  androidArchive,
  androidChecksum
}) {
  return new Promise((resolve) => {
    const server = http.createServer((request, response) => {
      if (request.url === '/releases/kasumi-release.json') {
        const windowsBytes = archive();
        const androidBytes = androidArchive?.() ?? Buffer.alloc(0);
        response.end(JSON.stringify({
          version: '0.1.0',
          assets: {
            windowsSdk: {
              file: 'windows.zip',
              size: windowsBytes.length,
              sha256: checksum()
            },
            androidMaven: {
              file: 'android.zip',
              size: androidBytes.length,
              sha256: androidChecksum?.() ?? '0'.repeat(64)
            }
          }
        }));
        return;
      }
      if (request.url === '/releases/windows.zip') {
        response.end(archive());
        return;
      }
      if (request.url === '/releases/android.zip' && androidArchive) {
        response.end(androidArchive());
        return;
      }
      response.statusCode = 404;
      response.end();
    });
    server.listen(0, '127.0.0.1', () => {
      const address = server.address();
      resolve({
        baseUrl: `http://127.0.0.1:${address.port}/releases`,
        close: () => new Promise((done) => server.close(done))
      });
    });
  });
}

function startRedirectServer() {
  return new Promise((resolve) => {
    const server = http.createServer((request, response) => {
      if (request.url === '/releases/kasumi-release.json') {
        response.end(JSON.stringify({
          version: '0.1.0',
          assets: {
            windowsSdk: {
              file: 'windows.zip',
              size: 0,
              sha256: '0'.repeat(64)
            },
            androidMaven: {
              file: 'android.zip',
              size: 0,
              sha256: '0'.repeat(64)
            }
          }
        }));
        return;
      }
      if (request.url.startsWith('/releases/windows.zip')) {
        const count = Number(request.url.split('/').pop()) || 0;
        response.statusCode = 302;
        response.setHeader(
          'location',
          `/releases/windows.zip/${count + 1}`
        );
        response.end();
        return;
      }
      response.statusCode = 404;
      response.end();
    });
    server.listen(0, '127.0.0.1', () => {
      const address = server.address();
      resolve({
        baseUrl: `http://127.0.0.1:${address.port}/releases`,
        close: () => new Promise((done) => server.close(done))
      });
    });
  });
}

function unsafeZip(name, content, options = {}) {
  const nameBytes = Buffer.from(name, 'utf8');
  const contentBytes = Buffer.from(content, 'utf8');
  const checksum = crc32(contentBytes);
  const local = Buffer.alloc(30);
  local.writeUInt32LE(0x04034b50, 0);
  local.writeUInt16LE(20, 4);
  local.writeUInt16LE(0, 6);
  local.writeUInt16LE(0, 8);
  local.writeUInt32LE(checksum, 14);
  local.writeUInt32LE(contentBytes.length, 18);
  local.writeUInt32LE(contentBytes.length, 22);
  local.writeUInt16LE(nameBytes.length, 26);

  const central = Buffer.alloc(46);
  central.writeUInt32LE(0x02014b50, 0);
  central.writeUInt16LE(options.symlink ? (3 << 8) | 20 : 20, 4);
  central.writeUInt16LE(20, 6);
  central.writeUInt32LE(checksum, 16);
  central.writeUInt32LE(contentBytes.length, 20);
  central.writeUInt32LE(contentBytes.length, 24);
  central.writeUInt16LE(nameBytes.length, 28);
  if (options.symlink) {
    central.writeUInt32LE((0o120777 << 16) >>> 0, 38);
  }

  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(1, 8);
  end.writeUInt16LE(1, 10);
  end.writeUInt32LE(central.length + nameBytes.length, 12);
  end.writeUInt32LE(local.length + nameBytes.length + contentBytes.length, 16);
  return Buffer.concat([local, nameBytes, contentBytes, central, nameBytes, end]);
}

function crc32(content) {
  let crc = 0xffffffff;
  for (const byte of content) {
    crc = (crc >>> 8) ^ CRC_TABLE[(crc ^ byte) & 0xff];
  }
  return (crc ^ 0xffffffff) >>> 0;
}

const CRC_TABLE = Array.from({ length: 256 }, (_, index) => {
  let value = index;
  for (let bit = 0; bit < 8; bit += 1) {
    value = (value & 1) !== 0
      ? 0xedb88320 ^ (value >>> 1)
      : value >>> 1;
  }
  return value >>> 0;
});
