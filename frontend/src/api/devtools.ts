import { KSM } from '../bridge/ksm.js';

export function open(): Promise<{ opened: boolean }> {
  return KSM.invoke('app.openDevtools', {});
}
