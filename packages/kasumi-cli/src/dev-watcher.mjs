import { readdir, stat } from 'node:fs/promises';
import path from 'node:path';

const DEFAULT_POLL_INTERVAL_MS = 400;
const DEFAULT_DEBOUNCE_MS = 400;
const IGNORED_DIRECTORIES = new Set([
  '.git',
  '.gradle',
  '.idea',
  '.ksm',
  '.vs',
  '.vscode',
  'build',
  'CMakeFiles',
  'Debug',
  'dist',
  'install',
  'node_modules',
  'out',
  'packages',
  'Release',
  'vcpkg_installed',
  'vendor'
]);
const IGNORED_FILE_NAMES = new Set(['.DS_Store', 'Thumbs.db']);
const IGNORED_FILE_SUFFIXES = [
  '.dll',
  '.exe',
  '.ilk',
  '.lastbuildstate',
  '.log',
  '.obj',
  '.pdb',
  '.suo',
  '.swp',
  '.swo',
  '.temp',
  '.tlog',
  '.tmp',
  '.user'
];

export async function collectDevelopmentSnapshot({
  projectRoot,
  configPath,
  extensionPaths = []
}) {
  const files = new Map();
  for (const directory of [
    path.join(projectRoot, 'windows'),
    path.join(projectRoot, 'extensions')
  ]) {
    await collectDirectory(directory, files);
  }
  await collectFile(configPath, files);
  for (const extensionPath of extensionPaths) {
    await collectFile(extensionPath, files);
  }
  return files;
}

export async function watchDevelopmentChanges({
  projectRoot,
  configPath,
  extensionPaths = [],
  onChange,
  intervalMs = DEFAULT_POLL_INTERVAL_MS,
  debounceMs = DEFAULT_DEBOUNCE_MS,
  onError = defaultErrorHandler
}) {
  let snapshot = await collectDevelopmentSnapshot({
    projectRoot,
    configPath,
    extensionPaths
  });
  let isClosed = false;
  let isReloading = false;
  let pollTimer;
  let debounceTimer;
  let activePoll;
  let activeReload;
  let reloadPromise;
  const pendingChanges = new Map();

  const scheduleReload = () => {
    clearTimeout(debounceTimer);
    debounceTimer = setTimeout(() => {
      activeReload = flushChanges();
    }, debounceMs);
  };

  const flushChanges = () => {
    if (isClosed || isReloading || pendingChanges.size === 0) {
      return reloadPromise ?? Promise.resolve();
    }
    const changes = [...pendingChanges.values()];
    pendingChanges.clear();
    isReloading = true;
    reloadPromise = (async () => {
      try {
        await onChange(changes);
      } catch (error) {
        onError(error);
      } finally {
        isReloading = false;
        reloadPromise = undefined;
        if (!isClosed && pendingChanges.size > 0) {
          scheduleReload();
        }
      }
    })();
    return reloadPromise;
  };

  const poll = async () => {
    if (isClosed) {
      return;
    }
    try {
      const next = await collectDevelopmentSnapshot({
        projectRoot,
        configPath,
        extensionPaths
      });
      const changes = compareSnapshots(snapshot, next);
      snapshot = next;
      if (changes.length > 0) {
        for (const change of changes) {
          pendingChanges.set(change.path, change);
        }
        scheduleReload();
      }
    } catch (error) {
      onError(error);
    } finally {
      if (!isClosed) {
        pollTimer = setTimeout(() => {
          activePoll = poll();
        }, intervalMs);
      }
    }
  };

  pollTimer = setTimeout(() => {
    activePoll = poll();
  }, intervalMs);

  return {
    async close() {
      isClosed = true;
      clearTimeout(pollTimer);
      clearTimeout(debounceTimer);
      await Promise.all([activePoll, activeReload]);
    }
  };
}

export function compareSnapshots(previous, next) {
  const changes = [];
  for (const [file, signature] of previous) {
    if (!next.has(file)) {
      changes.push({ path: file, kind: 'removed' });
      continue;
    }
    if (next.get(file) !== signature) {
      changes.push({ path: file, kind: 'modified' });
    }
  }
  for (const file of next.keys()) {
    if (!previous.has(file)) {
      changes.push({ path: file, kind: 'added' });
    }
  }
  return changes;
}

async function collectDirectory(directory, files) {
  let entries;
  try {
    entries = await readdir(directory, { withFileTypes: true });
  } catch (error) {
    if (error?.code === 'ENOENT') {
      return;
    }
    throw error;
  }
  for (const entry of entries) {
    if (entry.isSymbolicLink()) {
      continue;
    }
    const target = path.join(directory, entry.name);
    if (entry.isDirectory()) {
      if (!IGNORED_DIRECTORIES.has(entry.name)) {
        await collectDirectory(target, files);
      }
      continue;
    }
    if (entry.isFile() && !isIgnoredFile(entry.name)) {
      await collectFile(target, files);
    }
  }
}

async function collectFile(file, files) {
  try {
    const details = await stat(file);
    if (details.isFile()) {
      files.set(path.normalize(file), `${details.mtimeMs}:${details.size}`);
    }
  } catch (error) {
    if (error?.code !== 'ENOENT') {
      throw error;
    }
  }
}

function isIgnoredFile(name) {
  if (IGNORED_FILE_NAMES.has(name) || name.startsWith('.')) {
    return true;
  }
  return IGNORED_FILE_SUFFIXES.some((suffix) =>
    name.toLowerCase().endsWith(suffix)
  );
}

function defaultErrorHandler(error) {
  console.error(`[ksm] Development watcher error: ${error.message}`);
}
