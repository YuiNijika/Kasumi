import {
  mkdir,
  readFile,
  readdir,
  stat,
  writeFile
} from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const sourceDirectory = path.dirname(fileURLToPath(import.meta.url));
const templateDirectory = path.join(sourceDirectory, '..', 'templates');

export function resolveTemplate(name) {
  if (!/^[a-z0-9-]+$/.test(name)) {
    throw new Error(`Invalid template name: ${name}`);
  }
  return path.join(templateDirectory, name);
}

export async function renderTemplate({
  template,
  destination,
  replacements,
  force
}) {
  const source = resolveTemplate(template);
  if (!(await isDirectory(source))) {
    throw new Error(`Template not found: ${template}`);
  }
  await copyDirectory(source, destination, replacements, force);
}

async function copyDirectory(source, destination, replacements, force) {
  await mkdir(destination, { recursive: true });
  const entries = await readdir(source, { withFileTypes: true });
  for (const entry of entries) {
    const sourcePath = path.join(source, entry.name);
    const renderedName = replaceText(
      entry.name === 'gitignore' ? '.gitignore' : entry.name,
      replacements
    );
    const destinationPath = path.join(destination, renderedName);
    if (entry.isDirectory()) {
      await copyDirectory(sourcePath, destinationPath, replacements, force);
      continue;
    }
    if (!entry.isFile()) {
      continue;
    }
    if (!force && (await exists(destinationPath))) {
      throw new Error(`Refusing to overwrite: ${destinationPath}`);
    }
    const content = await readFile(sourcePath, 'utf8');
    await mkdir(path.dirname(destinationPath), { recursive: true });
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

export function formatCommandArray(commands) {
  return JSON.stringify(commands, null, 2).replaceAll('\n', '\n    ');
}
