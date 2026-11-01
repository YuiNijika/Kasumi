import { KSM } from '../bridge/ksm.js';
import type { KSMDeviceInfo } from '../bridge/types.js';

export function getInfo(): Promise<KSMDeviceInfo> {
  return KSM.invoke('device.getInfo', {});
}
