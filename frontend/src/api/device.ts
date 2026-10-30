import { KSM } from '../bridge/ksm';
import type { KSMDeviceInfo } from '../bridge/types';

export function getInfo(): Promise<KSMDeviceInfo> {
  return KSM.invoke('device.getInfo', {});
}
