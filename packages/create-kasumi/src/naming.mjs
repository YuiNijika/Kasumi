const NPM_NAME_PATTERN = /^[a-z0-9][a-z0-9._-]*$/;
const ANDROID_SEGMENT_PATTERN = /^[A-Za-z][A-Za-z0-9_]*$/;

export function normalizeProjectName(value) {
  const normalized = value
    .trim()
    .toLowerCase()
    .replace(/\s+/g, '-')
    .replace(/[^a-z0-9._-]/g, '-')
    .replace(/-+/g, '-')
    .replace(/^[._-]+|[._-]+$/g, '');
  if (!normalized || !NPM_NAME_PATTERN.test(normalized)) {
    throw new Error(
      'Project name must start with a letter or digit and contain only lowercase letters, digits, dots, underscores, or hyphens.'
    );
  }
  return normalized;
}

export function projectTitle(projectName) {
  return projectName
    .split(/[._-]+/)
    .filter(Boolean)
    .map((part) => `${part[0].toUpperCase()}${part.slice(1)}`)
    .join(' ');
}

export function androidPackageName(projectName) {
  const segment = projectName
    .toLowerCase()
    .replace(/[^a-z0-9_]/g, '_')
    .replace(/_+/g, '_')
    .replace(/^_+|_+$/g, '');
  const safeSegment = /^[a-z]/.test(segment) ? segment : `app_${segment}`;
  if (!ANDROID_SEGMENT_PATTERN.test(safeSegment)) {
    throw new Error(`Unable to derive an Android package name from ${projectName}.`);
  }
  return `dev.kasumi.${safeSegment}`;
}
