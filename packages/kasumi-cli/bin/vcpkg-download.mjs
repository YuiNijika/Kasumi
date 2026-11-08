#!/usr/bin/env node

import { createHash } from 'node:crypto';
import { createWriteStream } from 'node:fs';
import {
  mkdir,
  rename,
  rm,
  stat
} from 'node:fs/promises';
import path from 'node:path';
import { Readable } from 'node:stream';
import { pipeline } from 'node:stream/promises';

const DEFAULT_MIRRORS = [
  'https://gh-proxy.com/',
  'https://ghfast.top/'
];
const DEFAULT_TIMEOUT_MS = 60_000;

async function main() {
  const [url, expectedSha512, destination] = process.argv.slice(2);
  if (!url || !expectedSha512 || !destination) {
    throw new Error(
      'Usage: vcpkg-download.mjs <url> <sha512> <destination>'
    );
  }

  await mkdir(path.dirname(destination), { recursive: true });
  if (await matchesHash(destination, expectedSha512)) {
    return;
  }
  await rm(destination, { force: true });

  const timeoutMs = readTimeout();
  const sources = [url, ...mirrors(url)];
  const failures = [];
  for (const source of sources) {
    const temporary = `${destination}.${process.pid}-${Date.now()}.part`;
    try {
      console.error(`[ksm] downloading ${source}`);
      await download(source, temporary, timeoutMs);
      if (!(await matchesHash(temporary, expectedSha512))) {
        throw new Error('SHA512 mismatch');
      }
      await rename(temporary, destination);
      return;
    } catch (error) {
      await rm(temporary, { force: true }).catch(() => {});
      failures.push(`${source}: ${formatError(error)}`);
    }
  }

  throw new Error(
    [
      `Unable to download ${url}.`,
      ...failures.map((failure) => `- ${failure}`),
      'Set KASUMI_VCPKG_MIRRORS to a semicolon-separated list of HTTPS prefixes,',
      'or set it to an empty string to try only the original URL.'
    ].join('\n')
  );
}

async function download(url, destination, timeoutMs) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const response = await fetch(url, {
      redirect: 'follow',
      signal: controller.signal,
      headers: {
        'user-agent': 'Kasumi-vcpkg-downloader/1.0'
      }
    });
    if (!response.ok || !response.body) {
      throw new Error(`HTTP ${response.status}`);
    }
    await pipeline(
      Readable.fromWeb(response.body),
      createWriteStream(destination)
    );
  } finally {
    clearTimeout(timeout);
  }
}

async function matchesHash(file, expected) {
  try {
    const metadata = await stat(file);
    if (!metadata.isFile()) {
      return false;
    }
    const digest = createHash('sha512');
    const { createReadStream } = await import('node:fs');
    await pipeline(createReadStream(file), digest);
    return digest.digest('hex').toLowerCase() === expected.toLowerCase();
  } catch {
    return false;
  }
}

function mirrors(url) {
  const configured = process.env.KASUMI_VCPKG_MIRRORS;
  const prefixes = configured === undefined
    ? DEFAULT_MIRRORS
    : configured
        .split(';')
        .map((value) => value.trim())
        .filter(Boolean);
  return prefixes
    .map((prefix) => `${prefix}${url}`)
    .filter((source) => source !== url);
}

function readTimeout() {
  const parsed = Number(process.env.KASUMI_VCPKG_TIMEOUT_MS);
  if (
    Number.isSafeInteger(parsed) &&
    parsed > 0 &&
    parsed <= 10 * 60_000
  ) {
    return parsed;
  }
  return DEFAULT_TIMEOUT_MS;
}

function formatError(error) {
  if (error?.name === 'AbortError') {
    return 'request timed out';
  }
  return error instanceof Error ? error.message : String(error);
}

main().catch((error) => {
  console.error(`[ksm] ${formatError(error)}`);
  process.exitCode = 1;
});
