import http from 'node:http';
import {
  cp,
  mkdir,
  readFile,
  rm,
  writeFile
} from 'node:fs/promises';
import path from 'node:path';
import { spawn, spawnSync } from 'node:child_process';

import {
  commandName,
  exists,
  run,
  spawnLongRunning
} from './process.mjs';

const DEV_PORT = 5173;
const DEV_TIMEOUT_MS = 120_000;
const FRONTEND_OUTPUT = path.join('frontend', 'dist');
const CONTENT_SECURITY_POLICY = [
  "default-src 'self'",
  "script-src 'self' 'unsafe-inline'",
  "style-src 'self' 'unsafe-inline'",
  "img-src 'self' data:",
  "connect-src 'self'",
  "object-src 'none'",
  "base-uri 'none'"
].join('; ');
const CSP_META_PATTERN =
  /<meta\s+http-equiv=["']Content-Security-Policy["'][^>]*>/i;

export async function readFrontendSettings(
  projectRoot,
  androidWebView,
  packageJson = undefined
) {
  const projectPackageJson = packageJson ?? await readPackageJson(projectRoot);
  const scripts = projectPackageJson.scripts ?? {};
  const dependencies = {
    ...projectPackageJson.dependencies,
    ...projectPackageJson.devDependencies
  };
  const developmentEntries = webviewDevelopmentEntries(androidWebView);
  const productionEntry = androidWebView?.productionEntry;
  const productionHtmlFile = typeof productionEntry === 'string'
    ? productionEntry.slice(productionEntry.lastIndexOf('/') + 1)
    : 'index.html';
  const explicitEntry = developmentEntries.find((entry) =>
    entry.endsWith(`/${productionHtmlFile}`)
  );
  const rootEntry = developmentEntries.find(
    (entry) => new URL(entry).pathname === '/'
  );
  const bareEntry = developmentEntries.find(
    (entry) => new URL(entry).pathname === ''
  );
  if (
    typeof scripts['frontend:dev'] !== 'string' ||
    typeof scripts['frontend:build'] !== 'string'
  ) {
    return {
      kind: 'legacy-vite',
      devUrl: explicitEntry ?? `http://localhost:${DEV_PORT}/index.html`,
      outputDirectory: FRONTEND_OUTPUT
    };
  }
  if ('next' in dependencies) {
    return {
      kind: 'next',
      devScript: 'frontend:dev',
      buildScript: 'frontend:build',
      devUrl: rootEntry ?? `http://localhost:${DEV_PORT}/`,
      outputDirectory: path.join('frontend', 'out')
    };
  }
  if ('nuxt' in dependencies) {
    return {
      kind: 'nuxt',
      devScript: 'frontend:dev',
      buildScript: 'frontend:build',
      devUrl: rootEntry ?? `http://localhost:${DEV_PORT}/`,
      outputDirectory: path.join('frontend', '.output', 'public')
    };
  }
  return {
    kind: 'vite',
    devScript: 'frontend:dev',
    buildScript: 'frontend:build',
    devUrl: explicitEntry ?? bareEntry ?? `http://localhost:${DEV_PORT}/index.html`,
    outputDirectory: FRONTEND_OUTPUT
  };
}

function webviewDevelopmentEntries(androidWebView) {
  if (!Array.isArray(androidWebView?.developmentEntries)) {
    return [];
  }
  return androidWebView.developmentEntries;
}

export async function buildFrontend(projectRoot) {
  const settings = await readFrontendSettings(projectRoot);
  if (settings.kind === 'legacy-vite') {
    const viteBin = await requireVite(projectRoot);
    await run(
      process.execPath,
      [viteBin, 'build', '--config', 'frontend/vite.config.ts'],
      { cwd: projectRoot }
    );
    await injectContentSecurityPolicy(projectRoot);
    return;
  }
  const packageManager = await detectPackageManager(projectRoot);
  await run(
    packageManagerExecutable(packageManager),
    scriptArguments(packageManager, settings.buildScript),
    { cwd: projectRoot }
  );
  await copyFrontendOutput(projectRoot, settings.outputDirectory);
  await injectContentSecurityPolicy(projectRoot);
}

export async function startFrontendServer(projectRoot, androidWebView) {
  const packageJson = await readPackageJson(projectRoot);
  const settings = await readFrontendSettings(
    projectRoot,
    androidWebView,
    packageJson
  );
  if (await canConnect(settings.devUrl)) {
    throw new Error(
      `Development port ${DEV_PORT} is already in use. Stop the other process and retry.`
    );
  }
  const child = await startFrontendProcess(projectRoot, settings, packageJson);
  await waitForServer(child, settings.devUrl);
  return {
    child,
    url: settings.devUrl,
    stop() {
      stopChild(child);
    }
  };
}

async function startFrontendProcess(projectRoot, settings, packageJson) {
  if (settings.kind === 'legacy-vite') {
    const viteBin = await requireVite(projectRoot);
    return spawn(
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
        shell: false,
        detached: process.platform !== 'win32'
      }
    );
  }
  const packageManager = await detectPackageManager(
    projectRoot,
    packageJson
  );
  return spawnLongRunning(
    packageManagerExecutable(packageManager),
    scriptArguments(packageManager, settings.devScript),
    {
      cwd: projectRoot,
      env: process.env,
      detached: process.platform !== 'win32'
    }
  );
}

async function requireVite(projectRoot) {
  const viteBin = path.join(
    projectRoot,
    'node_modules',
    'vite',
    'bin',
    'vite.js'
  );
  if (!(await exists(viteBin))) {
    throw new Error(
      'Vite is not installed. Run the package manager install command first.'
    );
  }
  return viteBin;
}

async function copyFrontendOutput(projectRoot, outputDirectory) {
  const source = path.resolve(projectRoot, outputDirectory);
  const destination = path.resolve(projectRoot, FRONTEND_OUTPUT);
  if (source === destination) {
    return;
  }
  if (!(await exists(source))) {
    throw new Error(
      `Frontend build output not found: ${path.relative(projectRoot, source)}`
    );
  }
  await rm(destination, { recursive: true, force: true });
  await mkdir(path.dirname(destination), { recursive: true });
  await cp(source, destination, { recursive: true });
}

export async function injectContentSecurityPolicy(projectRoot) {
  const indexPath = path.resolve(projectRoot, FRONTEND_OUTPUT, 'index.html');
  let html;
  try {
    html = await readFile(indexPath, 'utf8');
  } catch (error) {
    if (error?.code === 'ENOENT') {
      throw new Error('Frontend build did not produce index.html.');
    }
    throw error;
  }
  if (CSP_META_PATTERN.test(html)) {
    return;
  }
  const meta = [
    '<meta',
    'http-equiv="Content-Security-Policy"',
    `content="${CONTENT_SECURITY_POLICY}"`,
    '>'
  ].join(' ');
  const updated = html.replace(
    /<head(?:\s[^>]*)?>/i,
    (head) => `${head}\n    ${meta}`
  );
  if (updated === html) {
    throw new Error('Frontend index.html does not contain a head element.');
  }
  await writeFile(indexPath, updated, 'utf8');
}

async function detectPackageManager(projectRoot, packageJson = undefined) {
  const projectPackageJson = packageJson ?? await readPackageJson(projectRoot);
  if (typeof projectPackageJson.packageManager === 'string') {
    const name = projectPackageJson.packageManager.split('@')[0];
    if (isSupportedPackageManager(name)) {
      return name;
    }
  }
  for (const [lockfile, packageManager] of [
    ['pnpm-lock.yaml', 'pnpm'],
    ['package-lock.json', 'npm'],
    ['yarn.lock', 'yarn']
  ]) {
    if (await exists(path.join(projectRoot, lockfile))) {
      return packageManager;
    }
  }
  return 'pnpm';
}

function scriptArguments(packageManager, script) {
  return packageManager === 'yarn'
    ? [script]
    : ['run', script];
}

function packageManagerExecutable(packageManager) {
  return process.platform === 'win32'
    ? `${packageManager}.cmd`
    : packageManager;
}

function isSupportedPackageManager(value) {
  return value === 'pnpm' || value === 'npm' || value === 'yarn';
}

async function readPackageJson(projectRoot) {
  try {
    return JSON.parse(
      await readFile(path.join(projectRoot, 'package.json'), 'utf8')
    );
  } catch (error) {
    if (error?.code === 'ENOENT') {
      return {};
    }
    throw error;
  }
}

async function waitForServer(child, url) {
  const deadline = Date.now() + DEV_TIMEOUT_MS;
  while (Date.now() < deadline) {
    if (child.exitCode !== null) {
      throw new Error(`Frontend exited before the development server became ready: ${url}`);
    }
    if (await canConnect(url)) {
      return;
    }
    await new Promise((resolve) => setTimeout(resolve, 200));
  }
  stopChild(child);
  throw new Error(`Frontend did not become ready at ${url}.`);
}

export function stopChild(child) {
  if (process.platform === 'win32' && child.pid) {
    spawnSync(
      'taskkill',
      ['/pid', String(child.pid), '/T', '/F'],
      { stdio: 'ignore' }
    );
    return;
  }
  if (child.pid) {
    try {
      process.kill(-child.pid, 'SIGTERM');
      return;
    } catch {
      // The child may not have started a process group.
    }
  }
  if (!child.killed) {
    child.kill();
  }
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
