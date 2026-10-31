import { spawn } from 'node:child_process';

const SUPPORTED_PACKAGE_MANAGERS = new Set(['pnpm', 'npm', 'yarn']);

export function validatePackageManager(value) {
  if (!SUPPORTED_PACKAGE_MANAGERS.has(value)) {
    throw new Error(
      `Unsupported package manager: ${value}. Use pnpm, npm, or yarn.`
    );
  }
  return value;
}

export async function installDependencies(directory, packageManager) {
  const command = process.platform === 'win32'
    ? `${packageManager}.cmd`
    : packageManager;
  console.log(`Installing dependencies with ${packageManager}`);
  const invocation = commandInvocation(command, ['install']);
  await run(invocation.command, invocation.args, directory);
}

function run(command, args, cwd) {
  return new Promise((resolve, reject) => {
    const child = spawn(command, args, {
      cwd,
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
          `${command} ${args.join(' ')} failed with ${signal ?? code}.`
        )
      );
    });
  });
}

export function commandInvocation(command, args) {
  if (process.platform !== 'win32' || !command.endsWith('.cmd')) {
    return { command, args };
  }
  const commandLine = [command, ...args].map(quoteWindowsArgument).join(' ');
  return {
    command: process.env.ComSpec ?? 'cmd.exe',
    args: ['/d', '/s', '/c', commandLine]
  };
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
