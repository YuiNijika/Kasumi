import { KSM, KSMWebMessageTransport } from '../../../frontend/src/index';
import type {
  KSMExtensionContract,
  KSMEvent,
  KSMRequest,
  KSMResponse,
  KSMTransport
} from '../../../frontend/src/bridge/types';
import { expectError } from '../helpers';

interface RecordedMessage {
  type: string;
  cmd?: string;
  id?: string;
  args?: unknown;
}

class RecordingTransport implements KSMTransport {
  public readonly sent: RecordedMessage[] = [];
  private counter = 0;

  public send(message: Parameters<KSMTransport['send']>[0]): void {
    this.sent.push(JSON.parse(JSON.stringify(message)) as RecordedMessage);
    if (message.type === 'request') {
      const id = message.id;
      const data = message.cmd === 'event.emit'
        ? { emitted: true }
        : { accepted: true };
      queueMicrotask(() => {
        this.emit({
          type: 'response',
          id,
          ok: true,
          data
        });
      });
    }
  }

  public onMessage(
    listener: (message: KSMResponse<unknown> | KSMEvent) => void
  ): () => void {
    this.listener = listener;
    return (): void => {
      this.listener = undefined;
    };
  }

  public emit(message: KSMResponse<unknown> | KSMEvent): void {
    this.listener?.(message);
  }

  public lastRequest(): RecordedMessage {
    let request: RecordedMessage | undefined;
    for (const entry of this.sent) {
      if (entry.type === 'request') {
        request = entry;
      }
    }
    if (!request) {
      throw new Error('No request was sent.');
    }
    return request;
  }

  private listener:
    | ((message: KSMResponse<unknown> | KSMEvent) => void)
    | undefined;
}

const contract: KSMExtensionContract = {
  id: 'library',
  commands: [
    {
      name: 'app.library.scan.start',
      platforms: ['windows', 'android'],
      permissions: ['fs.read'],
      args: {
        type: 'object',
        properties: {
          path: { type: 'string' }
        },
        required: ['path'],
        additionalProperties: false
      },
      result: {
        type: 'object',
        properties: {
          accepted: { type: 'boolean' }
        }
      }
    }
  ],
  events: [
    {
      name: 'app.library.scan.progress',
      platforms: ['windows', 'android'],
      payload: {
        type: 'object',
        properties: {
          count: { type: 'integer' }
        },
        required: ['count']
      }
    }
  ]
};

function withTransport(
  callback: (transport: RecordingTransport) => Promise<void>
): Promise<void> {
  const transport = new RecordingTransport();
  KSM.initialize(transport);
  return callback(transport).finally(() => KSM.dispose());
}

export async function runCapabilityApiCases(): Promise<void> {
  await withTransport(async (transport) => {
    KSM.configureExtensions(contract);
    const accepted = await KSM.extensions.invoke<{ accepted: boolean }>(
      'app.library.scan.start',
      { path: 'app://scope/library' }
    );
    if (!accepted.accepted || transport.lastRequest().cmd !== 'app.library.scan.start') {
      throw new Error('The extension command did not reach the transport.');
    }
    await expectError(
      KSM.extensions.invoke('app.library.scan.start', { path: 42 }),
      'INVALID_ARGS'
    );
    await expectError(
      KSM.extensions.invoke('app.library.missing', {}),
      'UNKNOWN_COMMAND'
    );
    await expectError(
      KSM.emit('app.library.unknown', { count: 1 }),
      'UNKNOWN_COMMAND'
    );
    const emitted = await KSM.emit('app.library.scan.progress', { count: 1 }, 'main');
    const request = transport.lastRequest();
    if (
      !emitted.emitted ||
      request.cmd !== 'event.emit' ||
      !request.args ||
      typeof request.args !== 'object' ||
      (request.args as Record<string, unknown>).windowId !== 'main'
    ) {
      throw new Error('event.emit did not preserve the target window.');
    }
    await expectError(
      KSM.emit('app.library.scan.progress', { value: 'x'.repeat(70 * 1024) }),
      'INVALID_ARGS'
    );
    const capabilities = await KSM.capabilities();
    if (!capabilities.extensionCommands?.includes('app.library.scan.start')) {
      throw new Error('capabilities did not include extension commands.');
    }
  });

  await withTransport(async () => {
    KSM.configureExtensions(contract);
    try {
      KSM.configureExtensions({
        ...contract,
        id: 'second',
        commands: contract.commands,
        events: []
      });
    } catch (error) {
      if (!(error instanceof Error) || error.name !== 'EXTENSION_CONFLICT') {
        throw error;
      }
      return;
    }
    throw new Error('Duplicate extension commands were accepted.');
  });

  await withTransport(async () => {
    try {
      KSM.configureExtensions({
        ...contract,
        commands: [
          {
            ...contract.commands[0]!,
            name: 'fs.readText'
          }
        ],
        events: []
      });
    } catch (error) {
      if (error instanceof TypeError) {
        return;
      }
      throw error;
    }
    throw new Error('A reserved extension command name was accepted.');
  });

  await withTransport(async (transport) => {
    KSM.configureExtensions(contract);
    const unsubscribe = KSM.on('app.library.scan.progress', () => undefined);
    await new Promise<void>((resolve) => setTimeout(resolve, 0));
    const beforeReload = transport.sent.filter(
      (entry) => entry.cmd === 'event.subscribe'
    ).length;
    if (beforeReload === 0) {
      throw new Error('The extension event was not subscribed.');
    }
    transport.emit({
      type: 'event',
      event: 'webview.reloaded',
      payload: {}
    });
    await new Promise<void>((resolve) => setTimeout(resolve, 0));
    const afterReload = transport.sent.filter(
      (entry) => entry.cmd === 'event.subscribe'
    ).length;
    if (afterReload <= beforeReload) {
      throw new Error('The extension subscription was not restored.');
    }
    unsubscribe();
  });
}
