import { KSM } from '../bridge/ksm.js';
import type {
  KSMDeepLinkReceivedPayload,
  KSMPendingDeepLinkResult,
  KSMSecondInstanceReceivedPayload
} from '../bridge/types.js';

export function getPending(): Promise<KSMPendingDeepLinkResult> {
  return KSM.invoke('app.deepLink.getPending', {});
}

export function onReceived(
  listener: (payload: KSMDeepLinkReceivedPayload) => void
): () => void {
  return KSM.on('app.deepLink.received', listener);
}

export function onSecondInstance(
  listener: (payload: KSMSecondInstanceReceivedPayload) => void
): () => void {
  return KSM.on('app.secondInstance.received', listener);
}
