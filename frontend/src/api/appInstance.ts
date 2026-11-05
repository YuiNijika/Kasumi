import { KSM } from '../bridge/ksm.js';
import type { KSMSecondInstanceReceivedPayload } from '../bridge/types.js';

export function state(): Promise<{ primary: boolean }> {
  return KSM.invoke('app.singleInstance.state', {});
}

export function focus(): Promise<{ focused: boolean }> {
  return KSM.invoke('app.singleInstance.focus', {});
}

export function onSecondInstance(
  listener: (payload: KSMSecondInstanceReceivedPayload) => void
): () => void {
  return KSM.on('app.secondInstance.received', listener);
}
