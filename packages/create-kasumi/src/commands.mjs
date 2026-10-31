import { readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const sourceDirectory = path.dirname(fileURLToPath(import.meta.url));
const dataPath = path.join(sourceDirectory, '..', 'data', 'commands.json');

export const platformCommands = JSON.parse(readFileSync(dataPath, 'utf8'));
