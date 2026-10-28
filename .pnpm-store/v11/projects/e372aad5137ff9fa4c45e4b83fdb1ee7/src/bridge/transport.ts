import type {
  KSMEvent,
  KSMResponse,
  KSMTransport
} from './types';

const MAX_MESSAGE_BYTES = 64 * 1024;

interface KSMWebView {
  postMessage(message: string): void;
  addEventListener?(
    type: 'message',
    listener: (event: MessageEvent<string>) => void
  ): void;
  removeEventListener?(
    type: 'message',
    listener: (event: MessageEvent<string>) => void
  ): void;
  onmessage?: ((event: MessageEvent<string>) => void) | null;
}

declare global {
  interface Window {
    chrome?: {
      webview?: KSMWebView;
    };
    ksm?: KSMWebView;
  }
}

export class KSMWebMessageTransport implements KSMTransport {
  private readonly webview: KSMWebView;

  public constructor(webview?: KSMWebView) {
    const resolvedWebview = webview ?? window.chrome?.webview ?? window.ksm;
    if (
      !resolvedWebview ||
      typeof resolvedWebview.postMessage !== 'function'
    ) {
      throw new Error('Kasumi WebView transport is unavailable.');
    }
    this.webview = resolvedWebview;
  }

  public send(message: Parameters<KSMTransport['send']>[0]): void {
    const payload = JSON.stringify(message);
    if (new TextEncoder().encode(payload).byteLength > MAX_MESSAGE_BYTES) {
      throw new Error('Kasumi messages must not exceed 64KB.');
    }
    this.webview.postMessage(payload);
  }

  public onMessage(
    listener: (message: KSMResponse<unknown> | KSMEvent) => void
  ): () => void {
    const handlePayload = (data: unknown): void => {
      if (typeof data !== 'string') {
        return;
      }
      if (new TextEncoder().encode(data).byteLength > MAX_MESSAGE_BYTES) {
        return;
      }
      try {
        const message = JSON.parse(data) as KSMResponse<unknown> | KSMEvent;
        if (message.type !== 'response' && message.type !== 'event') {
          return;
        }
        listener(message);
      } catch {
        return;
      }
    };
    const handleWebViewMessage = (event: MessageEvent<string>): void => {
      handlePayload(event.data);
    };
    const handleWindowMessage = (event: MessageEvent<unknown>): void => {
      handlePayload(event.data);
    };
    const addWebViewListener = this.webview.addEventListener;
    const hasWebViewMessages = typeof addWebViewListener === 'function';
    const hasOnMessage = 'onmessage' in this.webview;
    if (hasWebViewMessages) {
      addWebViewListener('message', handleWebViewMessage);
    } else if (hasOnMessage) {
      this.webview.onmessage = handleWebViewMessage;
    } else {
      window.addEventListener('message', handleWindowMessage);
    }
    return (): void => {
      if (hasWebViewMessages) {
        this.webview.removeEventListener?.('message', handleWebViewMessage);
      } else if (hasOnMessage) {
        this.webview.onmessage = null;
      } else {
        window.removeEventListener('message', handleWindowMessage);
      }
    };
  }
}
