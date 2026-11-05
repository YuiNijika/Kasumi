import type { KSMJsonValue, KSMLogLevel } from '../bridge/types.js';
import { MAX_REQUEST_TIMEOUT_MS } from '../bridge/bus.js';
import { MAX_MESSAGE_BYTES } from '../bridge/transport.js';

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

const VIRTUAL_PATH = /^app:\/\/(data|cache|tmp|picked|res|assets|scope)(?:\/(.*))?$/;
const READABLE_ROOTS = new Set([
  'data',
  'cache',
  'tmp',
  'picked',
  'res',
  'assets',
  'scope'
]);
const WRITABLE_ROOTS = new Set(['data', 'cache', 'tmp', 'picked']);
const SCOPE_ID = /^[A-Za-z0-9_-]+$/;

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
  if (root === 'scope') {
    if (relative === undefined || relative.length === 0) {
      throw new TypeError('scope paths must include a scope ID.');
    }
    if (
      relative.includes(':') ||
      relative.includes('\\') ||
      relative.startsWith('/')
    ) {
      throw new TypeError('path contains a forbidden segment.');
    }
    const [scopeId, ...segments] = relative.split('/');
    if (!SCOPE_ID.test(scopeId ?? '')) {
      throw new TypeError('scope ID contains a forbidden character.');
    }
    if (segments.some((segment) => segment.length === 0 || segment === '.')) {
      throw new TypeError('path contains a forbidden segment.');
    }
  } else if (relative === undefined || relative.length === 0) {
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

export function assertTimeout(value: unknown, field: string): asserts value is number {
  if (
    typeof value !== 'number' ||
    !Number.isSafeInteger(value) ||
    value <= 0 ||
    value > MAX_REQUEST_TIMEOUT_MS
  ) {
    throw new TypeError(
      `${field} must be an integer between 1 and ${MAX_REQUEST_TIMEOUT_MS}.`
    );
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

export function assertNonNegativeNumber(
  value: unknown,
  field: string,
  options: { min?: number; max?: number; allowZero?: boolean } = {}
): asserts value is number {
  if (
    typeof value !== 'number' ||
    !Number.isFinite(value) ||
    value < (options.min ?? (options.allowZero === false ? Number.MIN_VALUE : 0)) ||
    (options.max !== undefined && value > options.max)
  ) {
    throw new TypeError(`${field} must be a finite number within its range.`);
  }
}

export function assertInteger(
  value: unknown,
  field: string,
  options: { min?: number; max?: number } = {}
): asserts value is number {
  if (
    typeof value !== 'number' ||
    !Number.isSafeInteger(value) ||
    (options.min !== undefined && value < options.min) ||
    (options.max !== undefined && value > options.max)
  ) {
    throw new TypeError(`${field} must be an integer.`);
  }
}

export function assertStringArray(
  value: unknown,
  field: string,
  maxEntries = 64
): asserts value is string[] {
  if (!Array.isArray(value) || value.length > maxEntries) {
    throw new TypeError(`${field} must contain at most ${maxEntries} strings.`);
  }
  (value as unknown[]).forEach((item, index) => {
    assertBoundedString(item, `${field}[${index}]`, 256);
  });
}

export function assertPlainObject(
  value: unknown,
  field: string
): asserts value is Record<string, unknown> {
  if (typeof value !== 'object' || value === null || Array.isArray(value)) {
    throw new TypeError(`${field} must be an object.`);
  }
}

export function assertRecord(
  value: unknown,
  field: string
): asserts value is Record<string, KSMJsonValue> {
  assertPlainObject(value, field);
  assertJsonValue(value, field);
}

export function assertHeaderMap(
  value: unknown,
  field: string
): asserts value is Record<string, string> {
  assertPlainObject(value, field);
  const entries = Object.entries(value);
  if (entries.length > 32) {
    throw new TypeError(`${field} must contain at most 32 headers.`);
  }
  for (const [name, headerValue] of entries) {
    assertBoundedString(name, `${field}.${name}`, 128);
    assertBoundedString(headerValue, `${field}.${name}`, 8192, true);
    if (!/^[A-Za-z0-9-]+$/.test(name)) {
      throw new TypeError(`${field}.${name} contains a forbidden character.`);
    }
  }
}

export function assertHttpHeaders(
  value: unknown,
  field: string
): asserts value is Array<{ name: string; value: string }> {
  if (!Array.isArray(value) || value.length > 32) {
    throw new TypeError(`${field} must contain at most 32 headers.`);
  }
  for (const [index, header] of (value as unknown[]).entries()) {
    assertPlainObject(header, `${field}[${index}]`);
    assertBoundedString(header.name, `${field}[${index}].name`, 128);
    assertBoundedString(
      header.value,
      `${field}[${index}].value`,
      8192,
      true
    );
    if (!/^[A-Za-z0-9-]+$/.test(header.name)) {
      throw new TypeError(`${field}[${index}].name is invalid.`);
    }
  }
}

export function assertOptionalVirtualPath(
  value: unknown,
  field: string,
  writable: boolean
): void {
  if (value !== undefined) {
    assertVirtualPath(value as string, writable, false);
  }
}

export function assertScopeId(value: unknown, field: string): asserts value is string {
  assertBoundedString(value, field, 128);
  if (!SCOPE_ID.test(value)) {
    throw new TypeError(`${field} contains a forbidden character.`);
  }
}

export function assertMessagePayloadSize(value: unknown, field: string): void {
  let serialized: string;
  try {
    serialized = JSON.stringify(value);
  } catch {
    const error = new TypeError(`${field} must be serializable as JSON.`);
    error.name = 'INVALID_ARGS';
    throw error;
  }
  if (serialized === undefined) {
    const error = new TypeError(`${field} must be serializable as JSON.`);
    error.name = 'INVALID_ARGS';
    throw error;
  }
  if (new TextEncoder().encode(serialized).byteLength > MAX_MESSAGE_BYTES) {
    const error = new TypeError(`${field} must not exceed 64KB.`);
    error.name = 'INVALID_ARGS';
    throw error;
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
