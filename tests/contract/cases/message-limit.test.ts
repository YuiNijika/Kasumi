import { expectError, type ContractAdapter } from '../helpers';

export async function runMessageLimitCase(adapter: ContractAdapter): Promise<void> {
  await expectError(
    adapter.invoke('fs.writeText', {
      path: 'app://data/large.txt',
      content: 'x'.repeat(70 * 1024)
    }),
    'INVALID_ARGS'
  );
}
