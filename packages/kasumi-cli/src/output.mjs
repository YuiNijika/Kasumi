import path from 'node:path';

export function applicationOutputDirectory(projectRoot, platform) {
  return path.join(projectRoot, '.ksm', 'dist', platform);
}

export function packageOutputDirectory(projectRoot) {
  return path.join(projectRoot, '.ksm', 'dist', 'packages');
}
