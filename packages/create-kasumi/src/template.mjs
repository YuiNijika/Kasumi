import {
  copyFile,
  mkdir,
  readFile,
  readdir,
  rm,
  stat,
  writeFile
} from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const sourceDirectory = path.dirname(fileURLToPath(import.meta.url));
const templateDirectory = path.join(sourceDirectory, '..', 'templates');
const MANIFEST_NAME = '_template.json';
const TEMPLATE_NAMES = new Set([
  'basic',
  'react',
  'vue',
  'nextjs',
  'nuxtjs'
]);
const TEMPLATE_ALIASES = new Map([
  ['next', 'nextjs'],
  ['nuxt', 'nuxtjs']
]);
const BINARY_EXTENSIONS = new Set([
  '.ico',
  '.jar',
  '.png',
  '.ttf',
  '.woff',
  '.woff2'
]);

export function normalizeTemplateName(name) {
  if (!/^[a-z0-9-]+$/.test(name)) {
    return undefined;
  }
  const canonical = TEMPLATE_ALIASES.get(name) ?? name;
  return TEMPLATE_NAMES.has(canonical) ? canonical : undefined;
}

export function resolveTemplate(name) {
  const normalized = normalizeTemplateName(name);
  if (normalized === undefined) {
    throw new Error(`Invalid template name: ${name}`);
  }
  return path.join(templateDirectory, normalized);
}

export async function renderTemplate({
  template,
  destination,
  replacements,
  force
}) {
  const layers = [];
  await collectTemplateLayers(template, layers, new Set());
  for (const [index, layer] of layers.entries()) {
    if (index > 0) {
      await removeTemplatePaths(destination, layer.manifest.remove);
    }
    await copyDirectory(
      resolveTemplate(layer.name),
      destination,
      replacements,
      force,
      index > 0
    );
  }
}

async function collectTemplateLayers(name, layers, visiting) {
  const source = resolveTemplate(name);
  if (!(await isDirectory(source))) {
    throw new Error(`Template not found: ${name}`);
  }
  if (visiting.has(source)) {
    throw new Error(
      `Template inheritance cycle: ${[...visiting, source].join(' -> ')}`
    );
  }
  visiting.add(source);
  const manifest = await readTemplateManifest(source);
  if (manifest.extends) {
    await collectTemplateLayers(manifest.extends, layers, visiting);
  }
  visiting.delete(source);
  layers.push({ name, manifest });
}

async function readTemplateManifest(source) {
  try {
    const content = await readFile(path.join(source, MANIFEST_NAME), 'utf8');
    const manifest = JSON.parse(content);
    if (
      typeof manifest !== 'object' ||
      manifest === null ||
      Array.isArray(manifest)
    ) {
      throw new Error(`Invalid template manifest: ${source}`);
    }
    const unknownFields = Object.keys(manifest).filter(
      (key) => !['extends', 'remove'].includes(key)
    );
    if (
      unknownFields.length > 0 ||
      (manifest.extends !== undefined && typeof manifest.extends !== 'string') ||
      (
        manifest.remove !== undefined &&
        (
          !Array.isArray(manifest.remove) ||
          manifest.remove.some((value) => typeof value !== 'string')
        )
      )
    ) {
      throw new Error(`Invalid template manifest: ${source}`);
    }
    return manifest;
  } catch (error) {
    if (error?.code === 'ENOENT') {
      return {};
    }
    throw error;
  }
}

async function removeTemplatePaths(destination, paths) {
  if (!paths) {
    return;
  }
  const root = path.resolve(destination);
  for (const relativePath of paths) {
    const target = path.resolve(root, relativePath);
    if (
      relativePath.length === 0 ||
      path.isAbsolute(relativePath) ||
      (target !== root && !target.startsWith(`${root}${path.sep}`))
    ) {
      throw new Error(`Invalid template removal path: ${relativePath}`);
    }
    await rm(target, { recursive: true, force: true });
  }
}

async function copyDirectory(
  source,
  destination,
  replacements,
  force,
  overwrite
) {
  await mkdir(destination, { recursive: true });
  const entries = await readdir(source, { withFileTypes: true });
  for (const entry of entries) {
    if (entry.name === MANIFEST_NAME) {
      continue;
    }
    const sourcePath = path.join(source, entry.name);
    const renderedName = replaceText(
      entry.name === 'gitignore' ? '.gitignore' : entry.name,
      replacements
    );
    const destinationPath = path.join(destination, renderedName);
    if (entry.isDirectory()) {
      await copyDirectory(
        sourcePath,
        destinationPath,
        replacements,
        force,
        overwrite
      );
      continue;
    }
    if (!entry.isFile()) {
      continue;
    }
    if (!force && !overwrite && (await exists(destinationPath))) {
      throw new Error(`Refusing to overwrite: ${destinationPath}`);
    }
    await mkdir(path.dirname(destinationPath), { recursive: true });
    if (BINARY_EXTENSIONS.has(path.extname(entry.name).toLowerCase())) {
      await copyFile(sourcePath, destinationPath);
      continue;
    }
    const content = await readFile(sourcePath, 'utf8');
    await writeFile(
      destinationPath,
      replaceText(content, replacements),
      'utf8'
    );
  }
}

function replaceText(value, replacements) {
  let result = value;
  for (const [token, replacement] of Object.entries(replacements)) {
    result = result.split(token).join(replacement);
  }
  return result;
}

async function exists(value) {
  try {
    await stat(value);
    return true;
  } catch {
    return false;
  }
}

async function isDirectory(value) {
  try {
    return (await stat(value)).isDirectory();
  } catch {
    return false;
  }
}
