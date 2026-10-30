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
    relative.startsWith('/')
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

export function assertVirtualPath(path: string, writable = false): void {
  validateVirtualPath(path, {
    allowPicked: true,
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
