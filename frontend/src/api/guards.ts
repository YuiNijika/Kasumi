export function assertString(value: unknown, field: string): asserts value is string {
  if (typeof value !== 'string') {
    throw new TypeError(`${field} must be a string.`);
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
