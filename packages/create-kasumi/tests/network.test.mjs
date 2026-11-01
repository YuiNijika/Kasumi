import assert from 'node:assert/strict';
import test from 'node:test';

import { mergeCertificateStores } from '../src/network.mjs';

test('merges certificate stores without duplicate entries', () => {
  assert.deepEqual(
    mergeCertificateStores(['root-a', 'root-b'], ['root-b', 'root-c']),
    ['root-a', 'root-b', 'root-c']
  );
});
