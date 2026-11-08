import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { spawn } from 'node:child_process';
import {
  mkdtemp,
  readFile,
  rm,
  writeFile
} from 'node:fs/promises';
import http from 'node:http';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const script = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)),
  '..',
  'bin',
  'vcpkg-download.mjs'
);
const payload = Buffer.from('kasumi-vcpkg-download-fixture');
const digest = createHash('sha512').update(payload).digest('hex');

test('downloads directly when the original URL succeeds', async () => {
  const fixture = await startServer([
    { status: 200, body: payload }
  ]);
  const temporary = await mkdtemp(path.join(tmpdir(), 'ksm-vcpkg-direct-'));
  try {
    const destination = path.join(temporary, 'download.tar.gz');
    await runDownloader(fixture.url, digest, destination, {
      KASUMI_VCPKG_MIRRORS: `${fixture.url}/mirror/`
    });
    assert.deepEqual(await readFile(destination), payload);
    assert.equal(fixture.requests.length, 1);
  } finally {
    await fixture.close();
    await rm(temporary, { recursive: true, force: true });
  }
});

test('falls back to configured mirrors in order', async () => {
  const fixture = await startServer([
    { status: 502, body: 'bad gateway' },
    { status: 502, body: 'bad gateway' },
    { status: 200, body: payload }
  ]);
  const temporary = await mkdtemp(path.join(tmpdir(), 'ksm-vcpkg-mirror-'));
  try {
    const destination = path.join(temporary, 'download.tar.gz');
    await runDownloader(fixture.url, digest, destination, {
      KASUMI_VCPKG_MIRRORS:
        `${fixture.url}/mirror-a/;${fixture.url}/mirror-b/`
    });
    assert.deepEqual(await readFile(destination), payload);
    assert.equal(fixture.requests.length, 3);
    assert.match(fixture.requests[1], /\/mirror-a\//);
    assert.match(fixture.requests[2], /\/mirror-b\//);
  } finally {
    await fixture.close();
    await rm(temporary, { recursive: true, force: true });
  }
});

test('removes partial files and reports all download failures', async () => {
  const fixture = await startServer([
    { status: 502, body: 'bad gateway' }
  ]);
  const temporary = await mkdtemp(path.join(tmpdir(), 'ksm-vcpkg-fail-'));
  try {
    const destination = path.join(temporary, 'download.tar.gz');
    await assert.rejects(
      runDownloader(fixture.url, digest, destination, {
        KASUMI_VCPKG_MIRRORS: ''
      }),
      /Unable to download/
    );
    await assert.rejects(
      readFile(destination),
      (error) => error.code === 'ENOENT'
    );
    const leftovers = await import('node:fs/promises').then((fs) =>
      fs.readdir(temporary)
    );
    assert.deepEqual(leftovers, []);
  } finally {
    await fixture.close();
    await rm(temporary, { recursive: true, force: true });
  }
});

test('rejects content with the wrong SHA512', async () => {
  const fixture = await startServer([
    { status: 200, body: Buffer.from('wrong') }
  ]);
  const temporary = await mkdtemp(path.join(tmpdir(), 'ksm-vcpkg-hash-'));
  try {
    await assert.rejects(
      runDownloader(
        fixture.url,
        digest,
        path.join(temporary, 'download.tar.gz'),
        { KASUMI_VCPKG_MIRRORS: '' }
      ),
      /SHA512 mismatch/
    );
  } finally {
    await fixture.close();
    await rm(temporary, { recursive: true, force: true });
  }
});

async function startServer(responses) {
  const requests = [];
  const server = http.createServer((request, response) => {
    requests.push(request.url);
    const spec = responses[Math.min(requests.length - 1, responses.length - 1)];
    response.writeHead(spec.status, {
      'content-type': 'application/octet-stream'
    });
    response.end(spec.body);
  });
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  const address = server.address();
  return {
    requests,
    url: `http://127.0.0.1:${address.port}/archive.tar.gz`,
    close: () => new Promise((resolve, reject) => {
      server.close((error) => error ? reject(error) : resolve());
    })
  };
}

function runDownloader(url, sha512, destination, environment) {
  return new Promise((resolve, reject) => {
    const child = spawn(process.execPath, [script, url, sha512, destination], {
      env: {
        ...process.env,
        KASUMI_VCPKG_TIMEOUT_MS: '5000',
        ...environment
      },
      stdio: ['ignore', 'pipe', 'pipe']
    });
    let stdout = '';
    let stderr = '';
    child.stdout.on('data', (chunk) => {
      stdout += chunk;
    });
    child.stderr.on('data', (chunk) => {
      stderr += chunk;
    });
    child.once('error', reject);
    child.once('exit', (code) => {
      if (code === 0) {
        resolve({ stdout, stderr });
        return;
      }
      reject(new Error(stderr || stdout || `exit ${code}`));
    });
  });
}
