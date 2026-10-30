import { KSM } from '../../../frontend/src/bridge/ksm';
import { KSMWebMessageTransport } from '../../../frontend/src/bridge/transport';
import { MockHost, MockWebView } from '../adapters/mock-host';

const RELOAD_WAIT_MS = 1000;
const RELOAD_POLL_MS = 5;

async function waitFor(condition: () => boolean): Promise<void> {
  const started = Date.now();
  while (!condition()) {
    if (Date.now() - started > RELOAD_WAIT_MS) {
      throw new Error('Timed out waiting for WebView reload recovery.');
    }
    await new Promise<void>((resolve) => {
      setTimeout(resolve, RELOAD_POLL_MS);
    });
  }
}

export async function runReloadRecoveryCase(): Promise<void> {
  const host = new MockHost('windows');
  const webView = new MockWebView(host);
  KSM.initialize(new KSMWebMessageTransport(webView));

  try {
    await KSM.capabilities();
    const unsubscribe = KSM.events.on('window.resized', () => undefined);
    await waitFor(() => host.counters().subscribeCalls === 1);

    host.resetCounters();
    webView.emit({
      type: 'event',
      event: 'webview.reloaded',
      payload: {}
    });
    await waitFor(() => {
      const counters = host.counters();
      return counters.capabilityQueries === 1 &&
        counters.subscribeCalls === 1;
    });

    unsubscribe();
    await waitFor(() => host.counters().unsubscribeCalls === 1);
  } finally {
    KSM.dispose();
  }
}
