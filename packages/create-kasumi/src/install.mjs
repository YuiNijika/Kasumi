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
  await run(command, ['install'], directory);
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
