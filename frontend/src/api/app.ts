import { KSM } from '../bridge/ksm.js';
import type { KSMAppInfo } from '../bridge/types.js';

export function getInfo(): Promise<KSMAppInfo> {
  return KSM.getInfo();
}

export function exit(): Promise<{ exited: boolean }> {
  return KSM.invoke('app.exit', {});
}
