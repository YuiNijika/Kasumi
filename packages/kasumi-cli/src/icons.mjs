import { realpath } from 'node:fs/promises';
import path from 'node:path';

import { exists } from './process.mjs';

export async function resolveProjectIcon(
  projectRoot,
  relativePath,
  extension,
  label
) {
  if (!relativePath) {
    return null;
  }
  const root = path.resolve(projectRoot);
  const absolute = path.resolve(root, relativePath);
  if (!isInside(root, absolute)) {
    throw new Error(`${label} must stay inside the project.`);
  }
  if (path.extname(absolute).toLowerCase() !== extension) {
    throw new Error(`${label} must use the ${extension} extension.`);
  }
  if (!(await exists(absolute))) {
    throw new Error(`${label} does not exist: ${relativePath}`);
  }
  const resolvedRoot = await realpath(root);
  const resolved = await realpath(absolute);
  if (!isInside(resolvedRoot, resolved)) {
    throw new Error(`${label} must stay inside the project.`);
  }
  return resolved;
}

function isInside(root, candidate) {
  const relative = path.relative(root, candidate);
  return (
    relative === '' ||
    (!relative.startsWith(`..${path.sep}`) &&
      relative !== '..' &&
      !path.isAbsolute(relative))
  );
}
