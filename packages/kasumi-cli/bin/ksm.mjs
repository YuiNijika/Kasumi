#!/usr/bin/env node

import { run } from '../src/main.mjs';

run(process.argv.slice(2)).catch((error) => {
  const message = error instanceof Error ? error.message : String(error);
  console.error(`ksm: ${message}`);
  process.exitCode = 1;
});
