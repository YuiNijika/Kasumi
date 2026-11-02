import { KSMBus } from '../../../frontend/src/bridge/bus';
import type {
  KSMRequest,
  KSMTransport
} from '../../../frontend/src/bridge/types';
import { expectError } from '../helpers';

class SilentTransport implements KSMTransport {
  public readonly sent: string[] = [];

  public send(message: Parameters<KSMTransport['send']>[0]): void {
    this.sent.push(JSON.stringify(message));
  }

  public onMessage(): () => void {
    return (): void => {};
  }

  public lastRequestId(): string {
    const message = this.sent.at(-1);
    if (!message) {
      throw new Error('The bus did not send a request.');
    }
    return (JSON.parse(message) as KSMRequest<unknown>).id;
  }
}

async function expectInvalidTimeout(
  operation: Promise<unknown>
): Promise<void> {
  try {
    await operation;
  } catch (error) {
    if (
      error instanceof Error &&
      error.message.includes('between 1 and 600000')
    ) {
      return;
    }
    throw error;
  }
  throw new Error('Expected an invalid timeout to be rejected.');
}

export async function runTimeoutCases(): Promise<void> {
  const transport = new SilentTransport();
  const bus = new KSMBus(transport);
  try {
    await expectInvalidTimeout(bus.invoke('ping', {}, 0));
    await expectInvalidTimeout(bus.invoke('ping', {}, 600001));
    await expectError(bus.invoke('ping', {}, 5), 'TIMEOUT');
    const firstId = transport.lastRequestId();
    const cancelled = bus.invoke('ping', {}, 1000);
    const secondId = transport.lastRequestId();
    if (firstId === secondId || secondId === 'req-2') {
      throw new Error('Request IDs must be unique and session scoped.');
    }
    bus.cancel(secondId);
    await expectError(cancelled, 'CANCELLED');
  } finally {
    bus.dispose();
  }
}
