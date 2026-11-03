import { createHash } from 'node:crypto';
import { createReadStream, createWriteStream } from 'node:fs';
import {
  lstat,
  mkdir,
  mkdtemp,
  open,
  readFile,
  readdir,
  rename,
  rm,
  rmdir,
  stat
} from 'node:fs/promises';
import path from 'node:path';
import { Readable, Transform } from 'node:stream';
import { pipeline } from 'node:stream/promises';
import { createInflateRaw } from 'node:zlib';

const DEFAULT_REPOSITORY = 'YuiNijika/Kasumi';
const MANIFEST_NAME = 'kasumi-release.json';
const MAX_MANIFEST_BYTES = 1024 * 1024;
const MAX_ASSET_BYTES = 512 * 1024 * 1024;
const MANIFEST_TIMEOUT_MS = 30_000;
const ASSET_TIMEOUT_MS = 10 * 60 * 1000;
const MAX_REDIRECTS = 5;
const MAX_ARCHIVE_ENTRIES = 10_000;
const MAX_EXTRACTED_BYTES = 1024 * 1024 * 1024;
const MAX_COMPRESSION_RATIO = 500;

export async function installSdk({
  projectDirectory,
  platforms,
  version,
  releaseBaseUrl
}) {
  const baseUrl = normalizeBaseUrl(
    releaseBaseUrl ??
      `https://github.com/${DEFAULT_REPOSITORY}/releases/download/v${version}`
  );
  const temporaryDirectory = await createProjectTemporaryDirectory(
    projectDirectory
  );
  try {
    console.log(`Fetching Kasumi release metadata from ${baseUrl}`);
    const manifest = await fetchManifest(baseUrl, version);
    const installations = [];
    if (platforms.has('windows')) {
      installations.push(
        await stageAsset({
          baseUrl,
          asset: manifest.assets.windowsSdk,
          destination: path.join(
            projectDirectory,
            'vendor',
            'windows',
            'kasumi'
          ),
          temporaryDirectory,
          stagingName: 'windows-sdk',
          verify: isWindowsSdk,
          flattenSingleRoot: true
        })
      );
    }
    if (platforms.has('android')) {
      installations.push(
        await stageAsset({
          baseUrl,
          asset: manifest.assets.androidMaven,
          destination: path.join(projectDirectory, 'vendor', 'android', 'repo'),
          temporaryDirectory,
          stagingName: 'android-maven',
          verify: isAndroidMavenRepository
        })
      );
    }
    await commitInstallations(installations);
  } finally {
    await rm(temporaryDirectory, { recursive: true, force: true });
    await rmdir(path.dirname(temporaryDirectory)).catch(() => {});
  }
}

async function fetchManifest(baseUrl, expectedVersion) {
  const content = await fetchBuffer(
    releaseUrl(baseUrl, MANIFEST_NAME),
    MAX_MANIFEST_BYTES,
    MANIFEST_TIMEOUT_MS,
    MANIFEST_NAME
  );
  let manifest;
  try {
    manifest = JSON.parse(content.toString('utf8'));
  } catch {
    throw new Error(`${MANIFEST_NAME} is not valid JSON.`);
  }
  if (
    typeof manifest.version !== 'string' ||
    !manifest.assets?.windowsSdk?.file ||
    !manifest.assets?.androidMaven?.file
  ) {
    throw new Error(`${MANIFEST_NAME} is incomplete.`);
  }
  for (const asset of [
    manifest.assets.windowsSdk,
    manifest.assets.androidMaven
  ]) {
    if (
      typeof asset.sha256 !== 'string' ||
      !/^[a-f0-9]{64}$/i.test(asset.sha256)
    ) {
      throw new Error(`${MANIFEST_NAME} has an invalid SHA-256 checksum.`);
    }
    if (
      asset.size !== undefined &&
      (!Number.isSafeInteger(asset.size) || asset.size < 0)
    ) {
      throw new Error(`${MANIFEST_NAME} has an invalid asset size.`);
    }
  }
  if (manifest.version !== expectedVersion) {
    throw new Error(
      `${MANIFEST_NAME} version ${manifest.version} does not match ${expectedVersion}.`
    );
  }
  return manifest;
}

async function stageAsset({
  baseUrl,
  asset,
  destination,
  temporaryDirectory,
  stagingName,
  verify,
  flattenSingleRoot = false
}) {
  if (
    typeof asset.file !== 'string' ||
    path.basename(asset.file) !== asset.file ||
    !asset.file.endsWith('.zip')
  ) {
    throw new Error('Release manifest contains an invalid asset file name.');
  }
  const archive = path.join(
    temporaryDirectory,
    `${stagingName}-${asset.file}`
  );
  console.log(`Downloading ${asset.file}`);
  const downloaded = await download(
    releaseUrl(baseUrl, asset.file),
    archive,
    MAX_ASSET_BYTES
  );
  if (
    Number.isSafeInteger(asset.size) &&
    downloaded.bytes !== asset.size
  ) {
    throw new Error(`Downloaded size does not match ${asset.file}.`);
  }
  if (downloaded.sha256.toLowerCase() !== asset.sha256.toLowerCase()) {
    throw new Error(`Checksum mismatch for ${asset.file}.`);
  }

  const staging = await mkdtemp(path.join(temporaryDirectory, 'extract-'));
  try {
    await extractZip(archive, staging);
    await validateExtractedTree(staging);
    if (flattenSingleRoot && !(await verify(staging))) {
      await flattenSingleRootDirectory(staging);
    }
    if (!(await verify(staging))) {
      throw new Error(`Downloaded SDK has an unexpected layout: ${asset.file}`);
    }
    return {
      destination,
      stagingDirectory: staging
    };
  } catch (error) {
    await rm(staging, { recursive: true, force: true });
    throw error;
  }
}

async function commitInstallations(installations) {
  const records = [];
  try {
    for (const installation of installations) {
      const parent = path.dirname(installation.destination);
      await mkdir(parent, { recursive: true });
      const record = {
        ...installation,
        backup: `${installation.destination}.backup-${process.pid}-${Date.now()}-${records.length}`,
        hasDestination: await exists(installation.destination),
        isInstalled: false
      };
      records.push(record);
      if (record.hasDestination) {
        await rename(record.destination, record.backup);
      }
      await rename(record.stagingDirectory, record.destination);
      record.isInstalled = true;
    }
  } catch (error) {
    const rollbackErrors = [];
    for (const record of records.toReversed()) {
      if (record.isInstalled) {
        try {
          await rm(record.destination, { recursive: true, force: true });
        } catch (rollbackError) {
          rollbackErrors.push(rollbackError);
        }
      }
      if (record.hasDestination && await exists(record.backup)) {
        try {
          await rename(record.backup, record.destination);
        } catch (rollbackError) {
          rollbackErrors.push(rollbackError);
        }
      }
    }
    if (rollbackErrors.length > 0) {
      throw new AggregateError(
        [error, ...rollbackErrors],
        'SDK installation failed and rollback was incomplete.'
      );
    }
    throw error;
  }
  for (const record of records) {
    if (record.hasDestination) {
      await rm(record.backup, { recursive: true, force: true });
    }
  }
}

async function download(url, destination, maxBytes) {
  const hash = createHash('sha256');
  let bytes = 0;
  await withResponse(
    url,
    {
      timeoutMs: ASSET_TIMEOUT_MS,
      label: `download ${path.basename(destination)}`,
      maxBytes
    },
    async (response) => {
      if (!response.body) {
        throw new Error(`Download failed: ${response.status} ${url}`);
      }
      const meter = new Transform({
        transform(chunk, _encoding, callback) {
          bytes += chunk.length;
          if (bytes > maxBytes) {
            callback(new Error('Downloaded SDK exceeds the maximum size.'));
            return;
          }
          hash.update(chunk);
          callback(null, chunk);
        }
      });
      await pipeline(
        Readable.fromWeb(response.body),
        meter,
        createWriteStream(destination)
      );
    }
  );
  return { bytes, sha256: hash.digest('hex') };
}

async function fetchBuffer(url, maxBytes, timeoutMs, label) {
  return withResponse(
    url,
    { timeoutMs, label, maxBytes },
    async (response) => {
      if (!response.body) {
        throw new Error(`Download failed: ${response.status} ${url}`);
      }
      const chunks = [];
      let bytes = 0;
      for await (const chunk of response.body) {
        bytes += chunk.length;
        if (bytes > maxBytes) {
          throw new Error(`${label} exceeds the maximum size.`);
        }
        chunks.push(Buffer.from(chunk));
      }
      return Buffer.concat(chunks);
    }
  );
}

async function withResponse(
  initialUrl,
  { timeoutMs, label, maxBytes },
  handler
) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  let current = initialUrl;
  try {
    for (let redirects = 0; ; redirects += 1) {
      assertTrustedReleaseUrl(current);
      let response;
      try {
        response = await fetch(current, {
          headers: { 'user-agent': 'create-kasumi' },
          redirect: 'manual',
          signal: controller.signal
        });
      } catch (error) {
        if (controller.signal.aborted) {
          throw new Error(`${label} timed out.`);
        }
        throw error;
      }
      if (isRedirect(response.status)) {
        if (redirects >= MAX_REDIRECTS) {
          throw new Error(`${label} exceeded the redirect limit.`);
        }
        const location = response.headers.get('location');
        if (!location) {
          throw new Error(`${label} redirect did not include a location.`);
        }
        await response.body?.cancel();
        current = new URL(location, current).toString();
        continue;
      }
      if (!response.ok) {
        throw new Error(`${label} failed with HTTP ${response.status}.`);
      }
      const declaredLength = Number(response.headers.get('content-length'));
      if (
        Number.isFinite(declaredLength) &&
        declaredLength > maxBytes
      ) {
        throw new Error(`${label} exceeds the maximum size.`);
      }
      return await handler(response);
    }
  } finally {
    clearTimeout(timer);
  }
}

function isRedirect(status) {
  return status >= 300 && status <= 399;
}

function normalizeBaseUrl(value) {
  const url = new URL(value);
  assertTrustedReleaseUrl(url.toString());
  if (url.search || url.hash) {
    throw new Error('Release base URL must not contain a query or fragment.');
  }
  return url.toString().replace(/\/+$/, '');
}

function releaseUrl(baseUrl, file) {
  return `${baseUrl}/${encodeURIComponent(file)}`;
}

function assertTrustedReleaseUrl(value) {
  let url;
  try {
    url = new URL(value);
  } catch {
    throw new Error(`Invalid release URL: ${value}`);
  }
  if (url.username || url.password) {
    throw new Error('Release URLs must not contain credentials.');
  }
  if (url.protocol === 'https:') {
    return;
  }
  if (url.protocol === 'http:' && isLoopbackHost(url.hostname)) {
    return;
  }
  throw new Error('Release downloads must use HTTPS.');
}

function isLoopbackHost(hostname) {
  return hostname === 'localhost' ||
    hostname === '127.0.0.1' ||
    hostname === '::1' ||
    hostname === '[::1]';
}

async function extractZip(archive, destination) {
  const handle = await open(archive, 'r');
  try {
    const entries = await inspectZipEntries(handle, archive);
    const root = path.resolve(destination);
    for (const entry of entries) {
      const target = path.resolve(root, entry.name);
      if (target !== root && !target.startsWith(`${root}${path.sep}`)) {
        throw new Error(`Archive contains an unsafe path: ${entry.name}`);
      }
      if (entry.isDirectory) {
        await mkdir(target, { recursive: true });
        continue;
      }
      await mkdir(path.dirname(target), { recursive: true });
      if (entry.compressedSize === 0 && entry.uncompressedSize === 0) {
        const empty = await open(target, 'wx');
        await empty.close();
        continue;
      }
      const source = createReadStream(archive, {
        start: entry.dataOffset,
        end: entry.dataOffset + entry.compressedSize - 1
      });
      const guard = archiveEntryGuard(entry);
      const output = createWriteStream(target, { flags: 'wx' });
      if (entry.method === 0) {
        await pipeline(source, guard, output);
      } else if (entry.method === 8) {
        await pipeline(source, createInflateRaw(), guard, output);
      }
    }
  } finally {
    await handle.close();
  }
}

async function inspectZipEntries(handle, archive) {
  const archiveSize = (await handle.stat()).size;
  const tailLength = Math.min(archiveSize, 0xffff + 22);
  const tail = Buffer.alloc(tailLength);
  await handle.read(tail, 0, tailLength, archiveSize - tailLength);
  const endOffset = findEndOfCentralDirectory(tail);
  if (endOffset === -1 || tail.readUInt32LE(endOffset) !== 0x06054b50) {
    throw new Error(`Unable to inspect ${path.basename(archive)}.`);
  }
  const entryCount = tail.readUInt16LE(endOffset + 10);
  const centralSize = tail.readUInt32LE(endOffset + 12);
  const centralOffset = tail.readUInt32LE(endOffset + 16);
  if (
    entryCount === 0xffff ||
    centralOffset === 0xffffffff ||
    centralSize === 0xffffffff
  ) {
    throw new Error(`ZIP64 SDK archives are not supported: ${path.basename(archive)}`);
  }
  if (entryCount > MAX_ARCHIVE_ENTRIES) {
    throw new Error(`Archive contains too many entries: ${path.basename(archive)}`);
  }
  if (
    centralOffset > archiveSize ||
    centralSize > archiveSize ||
    centralOffset + centralSize > archiveSize
  ) {
    throw new Error(`Invalid ZIP central directory in ${path.basename(archive)}.`);
  }
  const content = Buffer.alloc(centralSize);
  await handle.read(content, 0, centralSize, centralOffset);
  let offset = centralOffset;
  const entries = [];
  let extractedBytes = 0;
  for (let index = 0; index < entryCount; index += 1) {
    const relativeOffset = offset - centralOffset;
    if (
      relativeOffset + 46 > content.length ||
      content.readUInt32LE(relativeOffset) !== 0x02014b50
    ) {
      throw new Error(`Invalid ZIP central directory in ${path.basename(archive)}.`);
    }
    const flags = content.readUInt16LE(relativeOffset + 8);
    const method = content.readUInt16LE(relativeOffset + 10);
    const crc = content.readUInt32LE(relativeOffset + 16);
    const compressedSize = content.readUInt32LE(relativeOffset + 20);
    const uncompressedSize = content.readUInt32LE(relativeOffset + 24);
    const nameLength = content.readUInt16LE(relativeOffset + 28);
    const extraLength = content.readUInt16LE(relativeOffset + 30);
    const commentLength = content.readUInt16LE(relativeOffset + 32);
    const madeBy = content.readUInt16LE(relativeOffset + 4);
    const externalAttributes = content.readUInt32LE(relativeOffset + 38);
    const localOffset = content.readUInt32LE(relativeOffset + 42);
    const nameStart = relativeOffset + 46;
    const nameEnd = nameStart + nameLength;
    if (nameEnd > content.length) {
      throw new Error(`Invalid ZIP entry name in ${path.basename(archive)}.`);
    }
    const name = content.subarray(nameStart, nameEnd).toString('utf8');
    if (!isSafeArchivePath(name)) {
      throw new Error(`Archive contains an unsafe path: ${name}`);
    }
    const unixMode = externalAttributes >>> 16;
    if (
      (madeBy >>> 8) === 3 &&
      (unixMode & 0xf000) === 0xa000
    ) {
      throw new Error(`Archive contains a symbolic link: ${name}`);
    }
    if ((flags & 0x1) !== 0) {
      throw new Error(`Archive contains an encrypted entry: ${name}`);
    }
    if (method !== 0 && method !== 8) {
      throw new Error(`Archive entry uses unsupported compression: ${name}`);
    }
    if (compressedSize === 0xffffffff || uncompressedSize === 0xffffffff) {
      throw new Error(`ZIP64 entries are not supported: ${name}`);
    }
    if (
      uncompressedSize > MAX_EXTRACTED_BYTES ||
      extractedBytes > MAX_EXTRACTED_BYTES - uncompressedSize
    ) {
      throw new Error(`Archive expands beyond the size limit: ${name}`);
    }
    if (
      uncompressedSize > 0 &&
      (compressedSize === 0 ||
        uncompressedSize / compressedSize > MAX_COMPRESSION_RATIO)
    ) {
      throw new Error(`Archive entry exceeds the compression ratio limit: ${name}`);
    }
    extractedBytes += uncompressedSize;
    const localHeader = Buffer.alloc(30);
    if (
      localOffset > archiveSize ||
      localOffset + localHeader.length > archiveSize
    ) {
      throw new Error(`Invalid ZIP local header: ${name}`);
    }
    await handle.read(localHeader, 0, localHeader.length, localOffset);
    if (localHeader.readUInt32LE(0) !== 0x04034b50) {
      throw new Error(`Invalid ZIP local header: ${name}`);
    }
    const localNameLength = localHeader.readUInt16LE(26);
    const localExtraLength = localHeader.readUInt16LE(28);
    const dataOffset =
      localOffset + localHeader.length + localNameLength + localExtraLength;
    if (
      dataOffset > archiveSize ||
      compressedSize > archiveSize - dataOffset
    ) {
      throw new Error(`Invalid ZIP entry data: ${name}`);
    }
    entries.push({
      name: name.replaceAll('\\', '/'),
      isDirectory: name.endsWith('/'),
      method,
      crc,
      compressedSize,
      uncompressedSize,
      dataOffset
    });
    offset = centralOffset + nameEnd + extraLength + commentLength;
  }
  return entries;
}

function findEndOfCentralDirectory(content) {
  const minimumOffset = Math.max(0, content.length - 0xffff - 22);
  for (let offset = content.length - 22; offset >= minimumOffset; offset -= 1) {
    if (content.readUInt32LE(offset) === 0x06054b50) {
      return offset;
    }
  }
  return -1;
}

function isSafeArchivePath(entry) {
  const normalized = entry.replaceAll('\\', '/');
  const segments = normalized.split('/');
  return !(
    normalized.startsWith('/') ||
    /^[A-Za-z]:/.test(normalized) ||
    segments.includes('..') ||
    normalized.includes('\0')
  );
}

function archiveEntryGuard(entry) {
  let written = 0;
  let crc = 0xffffffff;
  return new Transform({
    transform(chunk, _encoding, callback) {
      written += chunk.length;
      if (written > entry.uncompressedSize) {
        callback(new Error(`Archive entry exceeds its declared size: ${entry.name}`));
        return;
      }
      crc = updateCrc32(crc, chunk);
      callback(null, chunk);
    },
    flush(callback) {
      if (
        written !== entry.uncompressedSize ||
        (crc ^ 0xffffffff) >>> 0 !== entry.crc
      ) {
        callback(new Error(`Archive entry checksum mismatch: ${entry.name}`));
        return;
      }
      callback();
    }
  });
}

function updateCrc32(crc, chunk) {
  let value = crc;
  for (const byte of chunk) {
    value ^= byte;
    for (let bit = 0; bit < 8; bit += 1) {
      value = (value >>> 1) ^ (0xedb88320 & -(value & 1));
    }
  }
  return value >>> 0;
}

async function validateExtractedTree(root) {
  const absoluteRoot = path.resolve(root);
  const pending = [absoluteRoot];
  while (pending.length > 0) {
    const current = pending.pop();
    const entries = await readdir(current, { withFileTypes: true });
    for (const entry of entries) {
      const target = path.resolve(current, entry.name);
      if (
        target !== absoluteRoot &&
        !target.startsWith(`${absoluteRoot}${path.sep}`)
      ) {
        throw new Error(`Extracted SDK escaped its staging directory: ${target}`);
      }
      const metadata = await lstat(target);
      if (metadata.isSymbolicLink()) {
        throw new Error(`Extracted SDK contains a symbolic link: ${target}`);
      }
      if (metadata.isDirectory()) {
        pending.push(target);
      }
    }
  }
}

async function flattenSingleRootDirectory(directory) {
  const entries = await readdir(directory, { withFileTypes: true });
  if (entries.length !== 1 || !entries[0].isDirectory()) {
    return;
  }
  const root = path.join(directory, entries[0].name);
  for (const entry of await readdir(root, { withFileTypes: true })) {
    await rename(path.join(root, entry.name), path.join(directory, entry.name));
  }
  await rm(root, { recursive: true, force: true });
}

async function isWindowsSdk(directory) {
  return (
    (await exists(path.join(directory, 'include', 'kasumi'))) &&
    (await exists(
      path.join(directory, 'lib', 'cmake', 'Kasumi', 'KasumiConfig.cmake')
    ))
  );
}

async function isAndroidMavenRepository(directory) {
  const versionDirectories = await findVersionDirectories(directory);
  return versionDirectories.some((entry) =>
    entry.startsWith('kasumi-') && entry.endsWith('.pom')
  );
}

async function findVersionDirectories(directory) {
  const result = [];
  const pending = [directory];
  while (pending.length > 0) {
    const current = pending.pop();
    const entries = await readdir(current, { withFileTypes: true }).catch(
      () => []
    );
    for (const entry of entries) {
      if (entry.isDirectory()) {
        pending.push(path.join(current, entry.name));
      } else {
        result.push(entry.name);
      }
    }
  }
  return result;
}

async function exists(value) {
  try {
    await stat(value);
    return true;
  } catch {
    return false;
  }
}

async function createProjectTemporaryDirectory(projectDirectory) {
  const temporaryRoot = path.join(projectDirectory, '.tmp');
  await mkdir(temporaryRoot, { recursive: true });
  return mkdtemp(path.join(temporaryRoot, 'create-kasumi-'));
}
