import http from 'node:http';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { commandName, exists, run } from './process.mjs';

const DEV_PORT = 5173;
const DEV_URL = `http://localhost:${DEV_PORT}/index.html`;

export async function buildFrontend(projectRoot) {
  const viteBin = path.join(projectRoot, 'node_modules', 'vite', 'bin', 'vite.js');
  if (!(await exists(viteBin))) {
    throw new Error(
      'Vite is not installed. Run the package manager install command first.'
    );
  }
  await run(
    process.execPath,
    [viteBin, 'build', '--config', 'frontend/vite.config.ts'],
    { cwd: projectRoot }
  );
}

export async function startFrontendServer(projectRoot) {
  const viteBin = path.join(projectRoot, 'node_modules', 'vite', 'bin', 'vite.js');
  if (!(await exists(viteBin))) {
    throw new Error(
      'Vite is not installed. Run the package manager install command first.'
    );
  }
  const child = spawn(
    commandName(process.execPath),
    [
      viteBin,
      '--config',
      'frontend/vite.config.ts',
      '--host',
      '127.0.0.1',
      '--port',
      String(DEV_PORT),
      '--strictPort'
    ],
    {
      cwd: projectRoot,
      env: process.env,
      stdio: 'inherit',
      shell: false
    }
  );
  await waitForServer(child, DEV_URL);
  return {
    child,
    url: DEV_URL,
    stop() {
      if (!child.killed) {
        child.kill();
      }
    }
  };
}

async function waitForServer(child, url) {
  const deadline = Date.now() + 15_000;
  while (Date.now() < deadline) {
    if (child.exitCode !== null) {
      throw new Error('Vite exited before the development server became ready.');
    }
    if (await canConnect(url)) {
      return;
    }
    await new Promise((resolve) => setTimeout(resolve, 200));
  }
  child.kill();
  throw new Error(`Vite did not become ready at ${url}.`);
}

function canConnect(url) {
  return new Promise((resolve) => {
    const request = http.get(url, (response) => {
      response.resume();
      resolve(Boolean(response.statusCode && response.statusCode < 500));
    });
    request.setTimeout(500, () => {
      request.destroy();
      resolve(false);
    });
    request.once('error', () => resolve(false));
  });
}
