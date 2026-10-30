import { KSMWebMessageTransport } from '../../../frontend/src/bridge/transport';
import type { KSMEvent } from '../../../frontend/src/bridge/types';

type MessageListener<T> = (event: MessageEvent<T>) => void;

class FakeMessageTarget<T> {
  private readonly listeners = new Set<MessageListener<T>>();

  public addEventListener(type: string, listener: MessageListener<T>): void {
    if (type === 'message') {
      this.listeners.add(listener);
    }
  }

  public removeEventListener(type: string, listener: MessageListener<T>): void {
    if (type === 'message') {
      this.listeners.delete(listener);
    }
  }

  public dispatch(event: MessageEvent<T>): void {
    for (const listener of this.listeners) {
      listener(event);
    }
  }

  public listenerCount(): number {
    return this.listeners.size;
  }
}

const originalWindowDescriptor = Object.getOwnPropertyDescriptor(
  globalThis,
  'window'
);

export function runTransportEventCases(): void {
  const webViewMessages = new FakeMessageTarget<string>();
  const windowMessages = new FakeMessageTarget<unknown>();
  const webView = {
    postMessage: (_message: string): void => undefined,
    addEventListener: (
      type: 'message',
      listener: MessageListener<string>
    ): void => {
      webViewMessages.addEventListener(type, listener);
    },
    removeEventListener: (
      type: 'message',
      listener: MessageListener<string>
    ): void => {
      webViewMessages.removeEventListener(type, listener);
    }
  };
  Object.defineProperty(globalThis, 'window', {
    configurable: true,
    value: windowMessages
  });

  try {
    const transport = new KSMWebMessageTransport(webView);
    const received: KSMEvent[] = [];
    const unsubscribe = transport.onMessage((message) => {
      if (message.type === 'event') {
        received.push(message);
      }
    });
    const event: KSMEvent = {
      type: 'event',
      event: 'window.resized',
      payload: { width: 800, height: 600 }
    };
    const payload = JSON.stringify(event);

    windowMessages.dispatch({
      data: payload,
      source: windowMessages
    } as unknown as MessageEvent<unknown>);
    windowMessages.dispatch({
      data: payload,
      source: null
    } as unknown as MessageEvent<unknown>);

    if (received.length !== 1 || received[0]?.event !== event.event) {
      throw new Error('The transport did not isolate same-window events.');
    }
    unsubscribe();
    if (
      webViewMessages.listenerCount() !== 0 ||
      windowMessages.listenerCount() !== 0
    ) {
      throw new Error('The transport did not remove all message listeners.');
    }
  } finally {
    if (originalWindowDescriptor) {
      Object.defineProperty(globalThis, 'window', originalWindowDescriptor);
    } else {
      Reflect.deleteProperty(globalThis, 'window');
    }
  }
}
