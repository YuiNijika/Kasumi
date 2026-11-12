import { access } from 'node:fs/promises';
import { spawn, spawnSync } from 'node:child_process';

export async function exists(file) {
  try {
    await access(file);
    return true;
  } catch {
    return false;
  }
}

export function commandName(command) {
  if (process.platform !== 'win32') {
    return command;
  }
  if (command.endsWith('.exe') || command.endsWith('.cmd') || command.endsWith('.bat')) {
    return command;
  }
  return `${command}.exe`;
}

export function commandInvocation(command, args) {
  const executable = commandName(command);
  if (process.platform !== 'win32' || !/\.(?:cmd|bat)$/i.test(executable)) {
    return { command: executable, args };
  }
  const commandLine = [executable, ...args]
    .map(quoteWindowsArgument)
    .join(' ');
  return {
    command: process.env.ComSpec ?? 'cmd.exe',
    args: ['/d', '/s', '/c', commandLine]
  };
}

export function findCommand(command) {
  const lookup = process.platform === 'win32' ? 'where.exe' : 'which';
  const result = spawnSync(lookup, [command], {
    encoding: 'utf8',
    stdio: ['ignore', 'pipe', 'ignore']
  });
  if (result.status !== 0) {
    return undefined;
  }
  return result.stdout
    .split(/\r?\n/)
    .map((line) => line.trim())
    .find(Boolean);
}

export async function run(command, args, options = {}) {
  const invocation = commandInvocation(command, args);
  console.log(`> ${[command, ...args].join(' ')}`);
  await new Promise((resolve, reject) => {
    const child = spawn(invocation.command, invocation.args, {
      cwd: options.cwd,
      env: options.env ?? process.env,
      stdio: 'inherit',
      shell: false,
      windowsVerbatimArguments: options.windowsVerbatimArguments === true
    });
    options.onSpawn?.(child);
    child.once('error', reject);
    child.once('exit', (code, signal) => {
      if (code === 0) {
        resolve();
        return;
      }
      reject(
        new Error(
          `${command} exited with ${signal ?? code}.`
        )
      );
    });
  });
}

export function spawnLongRunning(command, args, options = {}) {
  const invocation = commandInvocation(command, args);
  return spawn(invocation.command, invocation.args, {
    cwd: options.cwd,
    env: options.env ?? process.env,
    stdio: 'inherit',
    shell: false,
    detached: options.detached === true
  });
}

export function waitForProcessSpawn(child) {
  if (child.pid) {
    return Promise.resolve();
  }
  return new Promise((resolve, reject) => {
    const onSpawn = () => {
      child.removeListener('error', onError);
      resolve();
    };
    const onError = (error) => {
      child.removeListener('spawn', onSpawn);
      reject(error);
    };
    child.once('spawn', onSpawn);
    child.once('error', onError);
  });
}

export function waitForProcessExit(child, timeoutMs = 5000) {
  if (child.exitCode !== null || child.signalCode !== null) {
    return Promise.resolve({
      code: child.exitCode,
      signal: child.signalCode
    });
  }
  return new Promise((resolve, reject) => {
    const onExit = (code, signal) => {
      clearTimeout(timer);
      resolve({ code, signal });
    };
    const timer = setTimeout(() => {
      child.removeListener('exit', onExit);
      reject(new Error(`Process ${child.pid ?? 'unknown'} did not exit.`));
    }, timeoutMs);
    child.once('exit', onExit);
  });
}

export function terminateProcessTree(pid) {
  if (!Number.isInteger(pid) || pid <= 0) {
    return;
  }
  if (process.platform === 'win32') {
    spawnSync(
      'taskkill',
      ['/pid', String(pid), '/T', '/F'],
      { stdio: 'ignore', windowsHide: true }
    );
    return;
  }
  try {
    process.kill(-pid, 'SIGTERM');
    return;
  } catch {
    // The process may not have started a process group.
  }
  try {
    process.kill(pid, 'SIGTERM');
  } catch {
    // A process that already exited needs no cleanup.
  }
}

function quoteWindowsArgument(value) {
  if (value.length === 0) {
    return '""';
  }
  if (!/[\s"&|<>^()]/.test(value)) {
    return value;
  }
  const escaped = value
    .replace(/(\\*)"/g, '$1$1\\"')
    .replace(/(\\+)$/, '$1$1');
  return `"${escaped}"`;
}
