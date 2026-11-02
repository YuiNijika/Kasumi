import type { KSMJsonValue, KSMLogLevel } from '../bridge/types.js';

const MAX_JSON_DEPTH = 32;
const LOG_LEVELS = new Set<KSMLogLevel>([
  'trace',
  'debug',
  'info',
  'warn',
  'error'
]);

export function assertString(value: unknown, field: string): asserts value is string {
  if (typeof value !== 'string') {
    throw new TypeError(`${field} must be a string.`);
  }
}

export function assertBoundedString(
  value: unknown,
  field: string,
  maxBytes: number,
  allowEmpty = false
): asserts value is string {
  assertString(value, field);
  if (!allowEmpty && value.length === 0) {
    throw new TypeError(`${field} must not be empty.`);
  }
  if (new TextEncoder().encode(value).byteLength > maxBytes) {
    throw new TypeError(`${field} must not exceed ${maxBytes} bytes.`);
  }
}

const VIRTUAL_PATH = /^app:\/\/(data|cache|tmp|picked|res|assets)(?:\/(.*))?$/;
const READABLE_ROOTS = new Set(['data', 'cache', 'tmp', 'picked', 'res', 'assets']);
const WRITABLE_ROOTS = new Set(['data', 'cache', 'tmp', 'picked']);

interface VirtualPathOptions {
  allowRoot?: boolean;
  allowPicked?: boolean;
  writable?: boolean;
}

function validateVirtualPath(path: string, options: VirtualPathOptions): void {
  assertString(path, 'path');
  if (
    path.includes('\0') ||
    path.includes('..') ||
    path.includes('~')
  ) {
    throw new TypeError('path contains a forbidden segment.');
  }
  const match = VIRTUAL_PATH.exec(path);
  const root = match?.[1];
  const relative = match?.[2];
  if (root === undefined) {
    throw new TypeError('path must use a Kasumi virtual path.');
  }
  if (relative === undefined || relative.length === 0) {
    if (!options.allowRoot || root === 'picked') {
      throw new TypeError('path must use a Kasumi virtual path.');
    }
  } else if (
    relative.includes(':') ||
    relative.includes('\\') ||
    relative.startsWith('/') ||
    relative.split('/').some((segment) => segment.length === 0 || segment === '.')
  ) {
    throw new TypeError('path contains a forbidden segment.');
  }
  if (root === 'picked' && options.allowPicked === false) {
    throw new TypeError('path must use a directory virtual path.');
  }
  if (root === 'picked' && relative?.includes('/')) {
    throw new TypeError('picked paths must contain a single file token.');
  }
  const roots = options.writable ? WRITABLE_ROOTS : READABLE_ROOTS;
  if (!roots.has(root)) {
    throw new TypeError('path must use a writable Kasumi virtual path.');
  }
}

export function assertVirtualPath(
  path: string,
  writable = false,
  allowPicked = true
): void {
  validateVirtualPath(path, {
    allowPicked,
    writable
  });
}

export function assertVirtualDirectoryPath(
  path: string,
  writable = false
): void {
  validateVirtualPath(path, {
    allowPicked: false,
    allowRoot: true,
    writable
  });
}

export function assertVirtualStatPath(path: string): void {
  validateVirtualPath(path, {
    allowPicked: true,
    allowRoot: true
  });
}

export function assertBoolean(value: unknown, field: string): asserts value is boolean {
  if (typeof value !== 'boolean') {
    throw new TypeError(`${field} must be a boolean.`);
  }
}

export function assertHttpUrl(
  value: unknown,
  field: string,
  httpsOnly = false
): asserts value is string {
  assertString(value, field);
  if (value.length > 8192) {
    throw new TypeError(`${field} must not exceed 8192 characters.`);
  }

  let parsed: URL;
  try {
    parsed = new URL(value);
  } catch {
    throw new TypeError(`${field} must be an absolute URL.`);
  }

  if (parsed.username || parsed.password || !parsed.hostname) {
    throw new TypeError(`${field} must not contain credentials or omit the host.`);
  }
  if (parsed.protocol !== 'https:' && (httpsOnly || parsed.protocol !== 'http:')) {
    throw new TypeError(`${field} must use ${httpsOnly ? 'HTTPS' : 'HTTP or HTTPS'}.`);
  }
}

export function assertPositiveInteger(value: unknown, field: string): asserts value is number {
  if (
    typeof value !== 'number' ||
    !Number.isFinite(value) ||
    !Number.isSafeInteger(value) ||
    value <= 0
  ) {
    throw new TypeError(`${field} must be a positive integer.`);
  }
}

export function assertJsonValue(
  value: unknown,
  field: string,
  depth = 0,
  seen = new WeakSet<object>()
): asserts value is KSMJsonValue {
  if (depth > MAX_JSON_DEPTH) {
    throw new TypeError(`${field} exceeds the maximum JSON depth.`);
  }
  if (
    value === null ||
    typeof value === 'string' ||
    typeof value === 'boolean'
  ) {
    return;
  }
  if (typeof value === 'number') {
    if (!Number.isFinite(value)) {
      throw new TypeError(`${field} must contain only finite numbers.`);
    }
    return;
  }
  if (typeof value !== 'object') {
    throw new TypeError(`${field} must be a JSON value.`);
  }
  if (seen.has(value)) {
    throw new TypeError(`${field} must not contain cyclic references.`);
  }
  seen.add(value);
  if (Array.isArray(value)) {
    value.forEach((item, index) => {
      assertJsonValue(item, `${field}[${index}]`, depth + 1, seen);
    });
    seen.delete(value);
    return;
  }
  for (const [key, item] of Object.entries(value)) {
    assertJsonValue(item, `${field}.${key}`, depth + 1, seen);
  }
  seen.delete(value);
}

export function assertStoreKey(value: unknown): asserts value is string {
  assertBoundedString(value, 'key', 256);
}

export function assertLogLevel(value: unknown): asserts value is KSMLogLevel {
  if (typeof value !== 'string' || !LOG_LEVELS.has(value as KSMLogLevel)) {
    throw new TypeError('level must be trace, debug, info, warn or error.');
  }
}

const SQL_DRIVERS = new Set(['sqlite', 'mysql', 'postgres', 'mongodb']);

export function assertSqlDriver(value: unknown): asserts value is string {
  if (typeof value !== 'string' || !SQL_DRIVERS.has(value)) {
    throw new TypeError('driver must be sqlite, mysql, postgres or mongodb.');
  }
}

export function assertSqlConnectionId(value: unknown): asserts value is string {
  assertString(value, 'connectionId');
  if (value.length === 0 || value.length > 128) {
    throw new TypeError('connectionId must contain between 1 and 128 characters.');
  }
}

export function assertSqlStatement(value: unknown): asserts value is string {
  assertString(value, 'statement');
  if (value.length === 0) {
    throw new TypeError('statement must not be empty.');
  }
  if (new TextEncoder().encode(value).byteLength > 1024 * 1024) {
    throw new TypeError('statement must not exceed 1 MiB.');
  }
}

export function assertSqlParams(value: unknown): asserts value is unknown[] {
  if (!Array.isArray(value)) {
    throw new TypeError('params must be an array.');
  }
  for (const param of value) {
    if (
      param !== null &&
      typeof param !== 'string' &&
      typeof param !== 'boolean' &&
      (typeof param !== 'number' || !Number.isFinite(param))
    ) {
      throw new TypeError('params must contain only strings, finite numbers, booleans or null.');
    }
  }
}
