import { spawnSync } from 'node:child_process';
import {
  mkdir,
  readFile,
  rm,
  writeFile
} from 'node:fs/promises';
import path from 'node:path';

import {
  exists,
  terminateProcessTree
} from './process.mjs';

const DEV_DIRECTORY = path.join('.ksm', 'dev');
const STATE_PATH = path.join(DEV_DIRECTORY, 'state.json');
const DEV_PORT = 5173;

export async function beginDevelopmentSession(projectRoot) {
  await cleanupStaleDevelopment(projectRoot);
  await writeState(projectRoot, {
    version: 1,
    processes: {}
  });
}

export async function recordDevelopmentProcess(projectRoot, role, pid) {
  if (!Number.isInteger(pid) || pid <= 0) {
    return;
  }
  const state = await readState(projectRoot);
  state.processes[role] = {
    pid,
    startedAt: new Date().toISOString()
  };
  await writeState(projectRoot, state);
}

export async function clearDevelopmentProcess(projectRoot, role, pid) {
  const state = await readState(projectRoot);
  const tracked = state.processes[role];
  if (!tracked || (pid !== undefined && tracked.pid !== pid)) {
    return;
  }
  delete state.processes[role];
  if (Object.keys(state.processes).length === 0) {
    await rm(statePath(projectRoot), { force: true });
    return;
  }
  await writeState(projectRoot, state);
}

export async function cleanupStaleDevelopment(projectRoot) {
  const state = await readState(projectRoot);
  await terminateRecordedProcess(projectRoot, state.processes.app);
  await terminateRecordedProcess(projectRoot, state.processes.frontend);
  await terminateProjectProcesses(projectRoot);
  await rm(statePath(projectRoot), { force: true });
}

async function terminateRecordedProcess(projectRoot, processInfo) {
  if (!processInfo || !Number.isInteger(processInfo.pid)) {
    return;
  }
  if (processInfo.pid === process.pid) {
    return;
  }
  if (process.platform === 'win32') {
    const details = await windowsProcessDetails(processInfo.pid);
    if (details && belongsToProject(details, projectRoot)) {
      terminateProcessTree(processInfo.pid);
    }
    return;
  }
  terminateProcessTree(processInfo.pid);
}

async function terminateProjectProcesses(projectRoot) {
  if (process.platform !== 'win32') {
    return;
  }
  const portProcesses = await windowsPortProcesses(DEV_PORT);
  const appProcesses = await windowsKasumiProcesses();
  const candidates = new Map(
    [...portProcesses, ...appProcesses]
      .filter((item) => item.pid !== process.pid)
      .map((item) => [item.pid, item])
  );
  for (const item of candidates.values()) {
    if (belongsToProject(item, projectRoot)) {
      terminateProcessTree(item.pid);
    }
  }
}

async function windowsPortProcesses(port) {
  const result = spawnSync(
    'netstat.exe',
    ['-ano', '-p', 'tcp'],
    { encoding: 'utf8', windowsHide: true }
  );
  if (result.status !== 0) {
    return [];
  }
  const processes = [];
  const pattern = new RegExp(`:${port}\\s+\\S+\\s+LISTENING\\s+(\\d+)`, 'i');
  for (const line of result.stdout.split(/\r?\n/)) {
    const match = pattern.exec(line);
    if (!match) {
      continue;
    }
    const pid = Number(match[1]);
    const details = await windowsProcessDetails(pid);
    if (details) {
      processes.push({ ...details, pid });
    }
  }
  return processes;
}

async function windowsKasumiProcesses() {
  const script = [
    "$items = @(Get-CimInstance Win32_Process |",
    "Where-Object { $_.Name -eq 'kasumi_app.exe' -or",
    "$_.Name -eq 'kasumi-app.exe' });",
    '$items | Select-Object ProcessId, ExecutablePath, CommandLine |',
    'ConvertTo-Json -Compress'
  ].join(' ');
  const result = spawnSync(
    'powershell.exe',
    ['-NoProfile', '-NonInteractive', '-Command', script],
    { encoding: 'utf8', windowsHide: true }
  );
  if (result.status !== 0 || !result.stdout.trim()) {
    return [];
  }
  try {
    const parsed = JSON.parse(result.stdout);
    return (Array.isArray(parsed) ? parsed : [parsed])
      .map((item) => ({
        pid: Number(item.ProcessId),
        executablePath: item.ExecutablePath ?? '',
        commandLine: item.CommandLine ?? ''
      }))
      .filter((item) => Number.isInteger(item.pid));
  } catch {
    return [];
  }
}

async function windowsProcessDetails(pid) {
  const script = [
    `$item = Get-CimInstance Win32_Process -Filter "ProcessId = ${pid}";`,
    'if ($item) {',
    '$item | Select-Object ProcessId, ExecutablePath, CommandLine |',
    'ConvertTo-Json -Compress',
    '}'
  ].join(' ');
  const result = spawnSync(
    'powershell.exe',
    ['-NoProfile', '-NonInteractive', '-Command', script],
    { encoding: 'utf8', windowsHide: true }
  );
  if (result.status !== 0 || !result.stdout.trim()) {
    return undefined;
  }
  try {
    const parsed = JSON.parse(result.stdout);
    return {
      pid,
      executablePath: parsed.ExecutablePath ?? '',
      commandLine: parsed.CommandLine ?? ''
    };
  } catch {
    return undefined;
  }
}

function belongsToProject(processInfo, projectRoot) {
  const root = normalizeWindowsPath(projectRoot);
  const executable = normalizeWindowsPath(processInfo.executablePath);
  const commandLine = normalizeWindowsPath(processInfo.commandLine);
  if (
    executable.startsWith(`${root}\\.ksm\\dev\\windows\\`) ||
    executable.startsWith(`${root}\\.ksm\\build\\windows`)
  ) {
    return true;
  }
  return commandLine.includes(root);
}

function normalizeWindowsPath(value) {
  return String(value ?? '')
    .replaceAll('/', '\\')
    .replace(/\\+/g, '\\')
    .toLowerCase();
}

async function readState(projectRoot) {
  try {
    const parsed = JSON.parse(
      await readFile(statePath(projectRoot), 'utf8')
    );
    if (
      parsed?.version !== 1 ||
      typeof parsed.processes !== 'object' ||
      parsed.processes === null
    ) {
      throw new Error('Invalid development state.');
    }
    return parsed;
  } catch {
    return {
      version: 1,
      processes: {}
    };
  }
}

async function writeState(projectRoot, state) {
  const target = statePath(projectRoot);
  await mkdir(path.dirname(target), { recursive: true });
  await writeFile(target, `${JSON.stringify(state, null, 2)}\n`, 'utf8');
}

function statePath(projectRoot) {
  return path.join(projectRoot, STATE_PATH);
}

export async function hasDevelopmentState(projectRoot) {
  return exists(statePath(projectRoot));
}
