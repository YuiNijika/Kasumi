import { KSM } from '../bridge/ksm.js';
import type { KSMOperatingSystemInfo } from '../bridge/types.js';

export function getInfo(): Promise<KSMOperatingSystemInfo> {
  return KSM.invoke('os.getInfo', {});
}
