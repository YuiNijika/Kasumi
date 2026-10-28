import { KSM } from '../bridge/ksm';
import type { KSMAppInfo } from '../bridge/types';

export function getInfo(): Promise<KSMAppInfo> {
  return KSM.getInfo();
}

export function exit(): Promise<{ exited: boolean }> {
  return KSM.invoke('app.exit', {});
}
