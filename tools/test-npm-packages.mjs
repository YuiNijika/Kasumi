import {
  mkdir,
  mkdtemp,
  readFile,
  rm,
  writeFile
} from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { run } from '../packages/kasumi-cli/src/process.mjs';

const root = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)),
  '..'
);
const packages = [
  'kasumi-bridge',
  'kasumi-cli',
  'create-kasumi'
];
const templates = ['basic', 'react', 'vue', 'nextjs', 'nuxtjs'];
const packageManager = process.platform === 'win32' ? 'pnpm.cmd' : 'pnpm';

const temporaryRoot = await mkdtemp(
  path.join(tmpdir(), 'kasumi-npm-packages-')
);
try {
  const tarballs = new Map();
  for (const name of packages) {
    const packageDirectory = path.join(root, 'packages', name);
    const packageJson = JSON.parse(
      await readFile(path.join(packageDirectory, 'package.json'), 'utf8')
    );
    await run(
      packageManager,
      [
        '--dir',
        packageDirectory,
        'pack',
        '--pack-destination',
        temporaryRoot
      ],
      { cwd: root }
    );
    tarballs.set(
      name,
      path.join(temporaryRoot, `${name}-${packageJson.version}.tgz`)
    );
  }

  const installRoot = path.join(temporaryRoot, 'install');
  await mkdir(installRoot, { recursive: true });
  await writeFile(
    path.join(installRoot, 'package.json'),
    JSON.stringify({
      name: 'kasumi-package-e2e',
      private: true,
      type: 'module'
    }, null, 2),
    'utf8'
  );
  await run(
    packageManager,
    [
      '--dir',
      installRoot,
      'add',
      ...packages.map((name) => tarballs.get(name))
    ],
    { cwd: root }
  );
  await run(packageManager, ['--dir', installRoot, 'exec', 'ksm', '--version'], {
    cwd: root
  });
  await run(
    packageManager,
    ['--dir', installRoot, 'exec', 'create-kasumi', '--help'],
    { cwd: root }
  );
  await run(
    'node',
    [
      '--input-type=module',
      '--eval',
      'import("kasumi-bridge").then((module) => { if (!module.KSM) process.exit(1); });'
    ],
    { cwd: installRoot }
  );

  for (const template of templates) {
    const project = path.join(temporaryRoot, `template-${template}`);
    await run(
      packageManager,
      [
        '--dir',
        installRoot,
        'exec',
        'create-kasumi',
        project,
        '--template',
        template,
        '--no-sdk',
        '--no-install',
        '--yes'
      ],
      { cwd: root }
    );
    const packagePath = path.join(project, 'package.json');
    const packageJson = JSON.parse(await readFile(packagePath, 'utf8'));
    packageJson.dependencies['kasumi-bridge'] =
      `file:../${path.basename(tarballs.get('kasumi-bridge'))}`;
    packageJson.devDependencies['kasumi-cli'] =
      `file:../${path.basename(tarballs.get('kasumi-cli'))}`;
    await writeFile(
      packagePath,
      `${JSON.stringify(packageJson, null, 2)}\n`,
      'utf8'
    );
    await run(packageManager, ['--dir', project, 'install'], { cwd: root });
    if (packageJson.scripts?.['frontend:build']) {
      await run(packageManager, ['--dir', project, 'run', 'frontend:build'], {
        cwd: root
      });
      continue;
    }
    await run(
      packageManager,
      [
        '--dir',
        project,
        'exec',
        'vite',
        'build',
        '--config',
        'frontend/vite.config.ts'
      ],
      { cwd: root }
    );
  }
} finally {
  await rm(temporaryRoot, { recursive: true, force: true });
}
