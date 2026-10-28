import { KSMBus } from '../../../frontend/src/bridge/bus';
import type { KSMTransport } from '../../../frontend/src/bridge/types';
import { expectError } from '../helpers';

class SilentTransport implements KSMTransport {
  public readonly sent: string[] = [];

  public send(message: Parameters<KSMTransport['send']>[0]): void {
    this.sent.push(JSON.stringify(message));
  }

  public onMessage(): () => void {
    return (): void => {};
  }
}

export async function runTimeoutCases(): Promise<void> {
  const transport = new SilentTransport();
  const bus = new KSMBus(transport);
  try {
    await expectError(bus.invoke('ping', {}, 5), 'TIMEOUT');
    const cancelled = bus.invoke('ping', {}, 1000);
    bus.cancel('req-2');
    await expectError(cancelled, 'CANCELLED');
  } finally {
    bus.dispose();
  }
}
