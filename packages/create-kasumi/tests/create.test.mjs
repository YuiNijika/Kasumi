import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import {
  mkdtemp,
  readFile,
  readdir,
  rm,
  stat
} from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { parseArgs } from '../src/args.mjs';
import { DEFAULT_RELEASE_VERSION, run } from '../src/cli.mjs';
import { platformCommands } from '../src/commands.mjs';
import { commandInvocation } from '../src/install.mjs';
import {
  androidPackageName,
  normalizeProjectName
} from '../src/naming.mjs';
import {
  resolveTemplateChoice
} from '../src/prompts.mjs';
import {
  normalizeTemplateName,
  resolveTemplate
} from '../src/template.mjs';

const testDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.resolve(testDirectory, '..', '..', '..');
const kasumiVersion = DEFAULT_RELEASE_VERSION;
const kasumiPackageVersion = JSON.parse(
  await readFile(
    path.join(testDirectory, '..', 'package.json'),
    'utf8'
  )
).version;

test('parseArgs selects both platforms by default', () => {
  const options = parseArgs(['my-app']);
  assert.equal(options.directory, 'my-app');
  assert.deepEqual([...options.platforms].sort(), ['android', 'windows']);
  assert.equal(options.install, true);
  assert.equal(options.sdk, true);
  assert.equal(options.kasumiVersion, undefined);
  assert.equal(options.template, undefined);
});

test('parseArgs supports short help and version flags', () => {
  assert.equal(parseArgs(['-h']).help, true);
  assert.equal(parseArgs(['-v']).version, true);
});

test('parseArgs supports source-only and single-platform initialization', () => {
  const options = parseArgs([
    'my-app',
    '--android',
    '--no-install',
    '--no-sdk',
    '--package-manager',
    'npm'
  ]);
  assert.deepEqual([...options.platforms], ['android']);
  assert.equal(options.install, false);
  assert.equal(options.sdk, false);
  assert.equal(options.packageManager, 'npm');
});

test('resolves framework template aliases', () => {
  assert.equal(resolveTemplate('next'), resolveTemplate('nextjs'));
  assert.equal(resolveTemplate('nuxt'), resolveTemplate('nuxtjs'));
  assert.equal(normalizeTemplateName('next'), 'nextjs');
  assert.equal(normalizeTemplateName('unknown'), undefined);
});

test('resolves interactive template choices', () => {
  assert.equal(resolveTemplateChoice(''), 'basic');
  assert.equal(resolveTemplateChoice('1'), 'basic');
  assert.equal(resolveTemplateChoice('2'), 'react');
  assert.equal(resolveTemplateChoice('react'), 'react');
  assert.equal(resolveTemplateChoice('next'), 'nextjs');
  assert.equal(resolveTemplateChoice('9'), undefined);
  assert.equal(resolveTemplateChoice('unknown'), undefined);
});

test('naming derives valid npm and Android names', () => {
  assert.equal(normalizeProjectName('My App'), 'my-app');
  assert.equal(androidPackageName('my-app'), 'dev.kasumi.my_app');
  assert.equal(androidPackageName('2fa-tool'), 'dev.kasumi.app_2fa_tool');
  assert.equal(androidPackageName('app'), 'dev.kasumi.apps.app');
});

test('wraps Windows package manager scripts through the system shell', () => {
  const invocation = commandInvocation('pnpm.cmd', ['install']);
  if (process.platform === 'win32') {
    assert.equal(invocation.command, process.env.ComSpec ?? 'cmd.exe');
    assert.deepEqual(invocation.args.slice(0, 3), ['/d', '/s', '/c']);
    assert.match(invocation.args[3], /pnpm\.cmd install/);
    return;
  }
  assert.equal(invocation.command, 'pnpm.cmd');
  assert.deepEqual(invocation.args, ['install']);
});

test('template command allowlists match the protocol', async () => {
  const protocol = JSON.parse(
    await readFile(
      path.join(repositoryRoot, 'protocol', 'commands.json'),
      'utf8'
    )
  );
  for (const platform of ['windows', 'android']) {
    const expected = Object.entries(protocol.commands)
      .filter(([, command]) => command.platforms.includes(platform))
      .map(([name]) => name)
      .sort();
    assert.deepEqual([...platformCommands[platform]].sort(), expected);
  }
});

test('initializes framework templates through the shared base layout', async () => {
  const templates = [
    ['react', 'frontend/src/App.tsx'],
    ['vue', 'frontend/src/App.vue'],
    ['nextjs', 'frontend/src/app/page.tsx'],
    ['nuxtjs', 'frontend/app/app.vue']
  ];
  for (const [template, frontendEntry] of templates) {
    const temporaryRoot = await mkdtemp(
      path.join(tmpdir(), 'create-kasumi-template-')
    );
    const destination = path.join(temporaryRoot, template);
    try {
      await run([
        destination,
        '--template',
        template,
        '--no-sdk',
        '--no-install',
        '--yes'
      ]);
      const packageJson = JSON.parse(
        await readFile(path.join(destination, 'package.json'), 'utf8')
      );
      assert.equal(packageJson.private, true);
      assert.equal(
        packageJson.dependencies['kasumi-bridge'],
        `^${kasumiPackageVersion}`
      );
      assert.equal(
        packageJson.devDependencies['kasumi-cli'],
        `^${kasumiPackageVersion}`
      );
      assert.equal(
        typeof packageJson.scripts['frontend:dev'],
        'string'
      );
      assert.equal(
        typeof packageJson.scripts['frontend:build'],
        'string'
      );
      if (template === 'nuxtjs') {
        const workspace = await readFile(
          path.join(destination, 'pnpm-workspace.yaml'),
          'utf8'
        );
        assert.match(workspace, /glob: 14\.0\.3/);
        assert.match(workspace, /minimatch: 11\.0\.3/);
      }
      assert.equal(
        await exists(path.join(destination, frontendEntry)),
        true
      );
      assert.equal(
        await exists(path.join(destination, 'frontend', 'vite.config.ts')),
        template === 'react' || template === 'vue'
      );
      assert.equal(
        await exists(path.join(destination, 'android', 'gradlew.bat')),
        true
      );
      assert.equal(
        await exists(path.join(destination, 'windows', 'vcpkg.json')),
        true
      );
    } finally {
      await rm(temporaryRoot, { recursive: true, force: true });
    }
  }
});

test('initializes a source-only project without framework source', async () => {
  const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'create-kasumi-test-'));
  const destination = path.join(temporaryRoot, 'my-app');
  try {
    await run([
      destination,
      '--no-sdk',
      '--no-install',
      '--yes'
    ]);
    const config = JSON.parse(
      await readFile(path.join(destination, 'kasumi.config.json'), 'utf8')
    );
    assert.equal(config.windows.packageName, 'my-app');
    assert.equal(config.windows.version, kasumiVersion);
    assert.equal(config.android.packageName, 'dev.kasumi.my_app');
    assert.equal(config.android.version, kasumiVersion);
    assert.equal(config.windows.commands, 'all');
    assert.equal(config.windows.decorations, true);
    assert.equal(config.android.commands, 'all');
    const mainActivity = path.join(
      destination,
      'android',
      'app',
      'src',
      'main',
      'java',
      'dev',
      'kasumi',
      'my_app',
      'MainActivity.kt'
    );
    assert.equal(await exists(mainActivity), true);
    const mainActivitySource = await readFile(mainActivity, 'utf8');
    assert.match(
      mainActivitySource,
      /import dev\.kasumi\.app\.bridge\.KSMWebViewHost/
    );
    assert.equal(mainActivitySource.includes('ACTION_VIEW'), false);
    assert.match(mainActivitySource, /if \(!BuildConfig\.DEBUG\)/);
    assert.equal(
      await exists(path.join(destination, 'windows', 'src')),
      false
    );
    assert.equal(await exists(path.join(destination, '.gitignore')), true);
    assert.equal(
      await exists(
        path.join(
          destination,
          'android',
          'gradle',
          'wrapper',
          'gradle-wrapper.jar'
        )
      ),
      true
    );
    assert.equal(
      await exists(path.join(destination, 'android', 'gradlew.bat')),
      true
    );
    const templateWrapper = path.join(
      testDirectory,
      '..',
      'templates',
      'basic',
      'android',
      'gradle',
      'wrapper',
      'gradle-wrapper.jar'
    );
    const generatedWrapper = path.join(
      destination,
      'android',
      'gradle',
      'wrapper',
      'gradle-wrapper.jar'
    );
    assert.equal(
      sha256(await readFile(generatedWrapper)),
      sha256(await readFile(templateWrapper))
    );
    assert.equal(
      await readFile(
        path.join(destination, 'pnpm-workspace.yaml'),
        'utf8'
      ),
      'allowBuilds:\n  esbuild: true\n  sharp: false\n  unrs-resolver: false\n'
    );
    const vcpkg = JSON.parse(
      await readFile(
        path.join(destination, 'windows', 'vcpkg.json'),
        'utf8'
      )
    );
    assert.equal(vcpkg['version-string'], kasumiVersion);
    assert.match(vcpkg['builtin-baseline'], /^[a-f0-9]{40}$/);
    assert.deepEqual(vcpkg.dependencies, [
      'nlohmann-json',
      'sqlite3',
      'webview2'
    ]);
    const files = await collectFiles(destination);
    for (const file of files) {
      if (path.extname(file).toLowerCase() === '.jar') {
        continue;
      }
      const content = await readFile(file, 'utf8');
      assert.equal(/__[A-Z_]+__/.test(content), false, file);
    }
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

test('refuses to initialize a non-empty directory without force', async () => {
  const temporaryRoot = await mkdtemp(path.join(tmpdir(), 'create-kasumi-test-'));
  const destination = path.join(temporaryRoot, 'occupied');
  try {
    await run([destination, '--no-sdk', '--no-install', '--yes']);
    await assert.rejects(
      run([destination, '--no-sdk', '--no-install', '--yes']),
      /not empty/
    );
  } finally {
    await rm(temporaryRoot, { recursive: true, force: true });
  }
});

async function collectFiles(directory) {
  const result = [];
  const entries = await readdir(directory, { withFileTypes: true });
  for (const entry of entries) {
    const file = path.join(directory, entry.name);
    if (entry.isDirectory()) {
      result.push(...(await collectFiles(file)));
    } else {
      result.push(file);
    }
  }
  return result;
}

async function exists(file) {
  try {
    await stat(file);
    return true;
  } catch {
    return false;
  }
}

function sha256(content) {
  return createHash('sha256').update(content).digest('hex');
}
