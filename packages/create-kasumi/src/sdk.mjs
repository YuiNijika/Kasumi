import { createHash } from 'node:crypto';
import {
  mkdir,
  mkdtemp,
  readFile,
  readdir,
  rename,
  rm,
  stat,
  writeFile
} from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

const DEFAULT_REPOSITORY = 'YuiNijika/Kasumi';
const MANIFEST_NAME = 'kasumi-release.json';

export async function installSdk({
  projectDirectory,
  platforms,
  version,
  releaseBaseUrl
}) {
  const baseUrl =
    releaseBaseUrl ??
    `https://github.com/${DEFAULT_REPOSITORY}/releases/download/v${version}`;
  const temporaryDirectory = await mkdtemp(
    path.join(tmpdir(), 'create-kasumi-')
  );
  try {
    console.log(`Fetching Kasumi release metadata from ${baseUrl}`);
    const manifest = await fetchManifest(baseUrl, version);
    const tasks = [];
    if (platforms.has('windows')) {
      tasks.push(
        installAsset({
          baseUrl,
          asset: manifest.assets.windowsSdk,
          destination: path.join(
            projectDirectory,
            'vendor',
            'windows',
            'kasumi'
          ),
          temporaryDirectory,
          verify: isWindowsSdk,
          flattenSingleRoot: true
        })
      );
    }
    if (platforms.has('android')) {
      tasks.push(
        installAsset({
          baseUrl,
          asset: manifest.assets.androidMaven,
          destination: path.join(projectDirectory, 'vendor', 'android', 'repo'),
          temporaryDirectory,
          verify: isAndroidMavenRepository
        })
      );
    }
    await Promise.all(tasks);
  } finally {
    await rm(temporaryDirectory, { recursive: true, force: true });
  }
}

async function fetchManifest(baseUrl, expectedVersion) {
  const response = await fetch(`${baseUrl}/${MANIFEST_NAME}`, {
    headers: { 'user-agent': 'create-kasumi' }
  });
  if (!response.ok) {
    throw new Error(
      `Unable to download ${MANIFEST_NAME}: HTTP ${response.status}. ` +
        'Use --no-sdk for a source-only project or pass --release-base-url.'
    );
  }
  const manifest = await response.json();
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
  }
  if (manifest.version !== expectedVersion) {
    throw new Error(
      `${MANIFEST_NAME} version ${manifest.version} does not match ${expectedVersion}.`
    );
  }
  return manifest;
}

async function installAsset({
  baseUrl,
  asset,
  destination,
  temporaryDirectory,
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
  const archive = path.join(temporaryDirectory, asset.file);
  console.log(`Downloading ${asset.file}`);
  await download(`${baseUrl}/${asset.file}`, archive);
  if (
    Number.isSafeInteger(asset.size) &&
    asset.size >= 0 &&
    (await stat(archive)).size !== asset.size
  ) {
    throw new Error(`Downloaded size does not match ${asset.file}.`);
  }
  if (asset.sha256) {
    const actual = await sha256(archive);
    if (actual.toLowerCase() !== asset.sha256.toLowerCase()) {
      throw new Error(`Checksum mismatch for ${asset.file}.`);
    }
  }
  await rm(destination, { recursive: true, force: true });
  await mkdir(destination, { recursive: true });
  extractZip(archive, destination);
  if (flattenSingleRoot && !(await verify(destination))) {
    await flattenSingleRootDirectory(destination);
  }
  if (!(await verify(destination))) {
    throw new Error(`Downloaded SDK has an unexpected layout: ${asset.file}`);
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

async function download(url, destination) {
  const response = await fetch(url, {
    headers: { 'user-agent': 'create-kasumi' }
  });
  if (!response.ok || !response.body) {
    throw new Error(`Download failed: HTTP ${response.status} ${url}`);
  }
  // Release assets are small enough to buffer and avoid Node stream bridging bugs.
  await writeFile(destination, Buffer.from(await response.arrayBuffer()));
}

async function sha256(file) {
  const content = await readFile(file);
  return createHash('sha256').update(content).digest('hex');
}

function extractZip(archive, destination) {
  const commands = process.platform === 'win32'
    ? [
        ['tar', ['-xf', archive, '-C', destination]],
        [
          'powershell.exe',
          [
            '-NoProfile',
            '-NonInteractive',
            '-Command',
            `Expand-Archive -LiteralPath '${escapePowerShell(archive)}' -DestinationPath '${escapePowerShell(destination)}' -Force`
          ]
        ]
      ]
    : [
        ['unzip', ['-q', archive, '-d', destination]],
        ['tar', ['-xf', archive, '-C', destination]]
      ];
  let lastError;
  for (const [command, args] of commands) {
    const result = spawnSync(command, args, { stdio: 'ignore' });
    if (result.status === 0) {
      return;
    }
    lastError =
      result.error ?? new Error(`${command} exited with ${result.status}`);
  }
  throw new Error(`Unable to extract ${path.basename(archive)}: ${lastError}`);
}

function escapePowerShell(value) {
  return value.replaceAll("'", "''");
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
