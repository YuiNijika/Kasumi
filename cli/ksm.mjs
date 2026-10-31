import { spawnSync } from 'node:child_process';
import { existsSync, readFileSync, readdirSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const cliDirectory = path.dirname(fileURLToPath(import.meta.url));
const projectRoot = path.dirname(cliDirectory);
const cliScript = path.join(cliDirectory, 'ksm.py');

function isUsable(candidate, prefix = []) {
    const result = spawnSync(candidate, [...prefix, '--version'], {
        cwd: projectRoot,
        encoding: 'utf8',
        stdio: 'ignore'
    });
    return result.status === 0;
}

function findPython() {
    const configured = process.env.KASUMI_PYTHON;
    if (configured) {
        return { command: configured, prefix: [] };
    }

    const virtualEnvironments = process.platform === 'win32'
        ? ['.venv/Scripts/python.exe', 'venv/Scripts/python.exe']
        : ['.venv/bin/python', 'venv/bin/python'];
    for (const relativePath of virtualEnvironments) {
        const candidate = path.join(projectRoot, relativePath);
        if (existsSync(candidate)) {
            return { command: candidate, prefix: [] };
        }
    }

    if (process.platform === 'win32') {
        const runtimeCandidates = [];
        const workbuddyVersions = path.join(
            process.env.USERPROFILE ?? '',
            '.workbuddy',
            'binaries',
            'python',
            'versions'
        );
        const currentVersionFile = path.join(workbuddyVersions, 'current');
        if (existsSync(currentVersionFile)) {
            const version = readFileSync(currentVersionFile, 'utf8').trim();
            if (version) {
                runtimeCandidates.push(path.join(workbuddyVersions, version, 'python.exe'));
            }
        }

        const localPython = path.join(
            process.env.LOCALAPPDATA ?? '',
            'Programs',
            'Python'
        );
        if (existsSync(localPython)) {
            const versions = readdirSync(localPython, { withFileTypes: true })
                .filter((entry) => entry.isDirectory())
                .map((entry) => entry.name)
                .sort()
                .reverse();
            for (const version of versions) {
                runtimeCandidates.push(path.join(localPython, version, 'python.exe'));
            }
        }

        for (const candidate of runtimeCandidates) {
            if (existsSync(candidate)) {
                return { command: candidate, prefix: [] };
            }
        }
    }

    const candidates = process.platform === 'win32'
        ? [
            { command: 'py', prefix: ['-3'] },
            { command: 'python', prefix: [] },
            { command: 'python3', prefix: [] }
        ]
        : [
            { command: 'python3', prefix: [] },
            { command: 'python', prefix: [] }
        ];
    for (const candidate of candidates) {
        if (isUsable(candidate.command, candidate.prefix)) {
            return candidate;
        }
    }

    return null;
}

const python = findPython();
if (!python) {
    console.error(
        'Kasumi CLI requires Python 3.10 or newer. ' +
        'Set KASUMI_PYTHON to the Python executable or add python to PATH.'
    );
    process.exit(1);
}

const result = spawnSync(
    python.command,
    [...python.prefix, cliScript, ...process.argv.slice(2)],
    {
        cwd: projectRoot,
        stdio: 'inherit'
    }
);
process.exit(result.status ?? 1);
