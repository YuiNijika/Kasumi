export function assertString(value: unknown, field: string): asserts value is string {
  if (typeof value !== 'string') {
    throw new TypeError(`${field} must be a string.`);
  }
}

export function assertVirtualPath(path: string): void {
  assertString(path, 'path');
  if (
    path.includes('\0') ||
    path.includes('..') ||
    path.includes('~') ||
    /^([A-Za-z]:[\\/]|[\\/]{1,2})/.test(path)
  ) {
    throw new TypeError('path contains a forbidden segment.');
  }
  if (!/^app:\/\/(data|cache|tmp|picked)(\/|$)/.test(path)) {
    throw new TypeError('path must use a Kasumi virtual path.');
  }
}
