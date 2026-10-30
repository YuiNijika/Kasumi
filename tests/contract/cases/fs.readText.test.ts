import { expectError, type ContractAdapter } from '../helpers';

export async function runFsCases(adapter: ContractAdapter): Promise<void> {
  await expectError(
    adapter.invoke('fs.readText', { path: 'app://res/../data/config.json' }),
    'PERMISSION_DENIED'
  );
  for (const path of [
    'app://data/C:/secret.txt',
    'app://data/C:secret.txt',
    'app://data/file.txt:stream',
    'app://data/\\server\\share',
    'app://data//server',
    'app://picked/1/2',
    'app://other/file.txt',
    'file:///secret.txt'
  ]) {
    await expectError(
      adapter.invoke('fs.readText', { path }),
      'PERMISSION_DENIED'
    );
  }

  await adapter.invoke('fs.writeText', {
    path: 'app://data/contract.txt',
    content: 'kasumi'
  });
  const result = await adapter.invoke<string | { file: string }>(
    'fs.readText',
    { path: 'app://data/contract.txt' }
  );
  if (result !== 'kasumi') {
    throw new Error('fs.readText did not return the written content.');
  }

  const firstHandle = await adapter.invoke<string | { file: string }>(
    'fs.readText',
    { path: 'app://data/large.txt' }
  );
  if (typeof firstHandle === 'string' || !firstHandle.file.startsWith('app://tmp/')) {
    throw new Error('Large text did not return a temporary handle.');
  }
  const secondHandle = await adapter.invoke<string | { file: string }>(
    'fs.readText',
    { path: firstHandle.file }
  );
  if (typeof secondHandle === 'string' || secondHandle.file !== firstHandle.file) {
    throw new Error('Reading a large temporary handle changed the handle.');
  }
}
