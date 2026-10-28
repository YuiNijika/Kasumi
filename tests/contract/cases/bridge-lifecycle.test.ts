import { expectError, type ContractAdapter } from '../helpers';

export async function runLifecycleCases(adapter: ContractAdapter): Promise<void> {
  await adapter.invoke('event.subscribe', { event: 'window.resized' });
  await adapter.invoke('event.unsubscribe', { event: 'window.resized' });

  await expectError(
    adapter.invoke('fs.writeText', { path: 'app://data/missing-content.txt' }),
    'INVALID_ARGS'
  );

}
