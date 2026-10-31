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
  const executable = commandName(command);
  console.log(`> ${[command, ...args].join(' ')}`);
  await new Promise((resolve, reject) => {
    const child = spawn(executable, args, {
      cwd: options.cwd,
      env: options.env ?? process.env,
      stdio: 'inherit',
      shell: false
    });
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
  return spawn(commandName(command), args, {
    cwd: options.cwd,
    env: options.env ?? process.env,
    stdio: 'inherit',
    shell: false
  });
}
