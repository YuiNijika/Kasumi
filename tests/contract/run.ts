import { KSM } from '../../frontend/src/bridge/ksm';
import type { KSMPlatform } from '../../frontend/src/bridge/types';
import { FrontendContractAdapter } from './adapters/frontend';
import { MockHost, MockWebView } from './adapters/mock-host';
import { runContractSuite } from './runner';
import { runTransportEventCases } from './cases/transport-events.test';
import { runReloadRecoveryCase } from './cases/reload-recovery.test';

async function runPlatform(platform: KSMPlatform): Promise<void> {
  const host = new MockHost(platform);
  const adapter = new FrontendContractAdapter(new MockWebView(host));
  try {
    await runContractSuite(adapter);
  } finally {
    KSM.dispose();
  }
}

async function main(): Promise<void> {
  runTransportEventCases();
  await runReloadRecoveryCase();
  await runPlatform('windows');
  await runPlatform('android');
  console.log('frontend contract passed for windows and android');
}

void main().catch((error: unknown) => {
  console.error(error);
  throw error;
});
