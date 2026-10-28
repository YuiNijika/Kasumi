import { expectError, type ContractAdapter } from '../helpers';

export async function runFsCases(adapter: ContractAdapter): Promise<void> {
  await expectError(
    adapter.invoke('fs.readText', { path: 'app://res/../data/config.json' }),
    'PERMISSION_DENIED'
  );

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
}
