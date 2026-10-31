#!/usr/bin/env node

import { run } from '../src/cli.mjs';

run(process.argv.slice(2)).catch((error) => {
  const message = error instanceof Error ? error.message : String(error);
  console.error(`create-kasumi: ${message}`);
  process.exitCode = 1;
});
