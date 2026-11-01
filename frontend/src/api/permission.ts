import { KSM } from '../bridge/ksm.js';
import { assertString } from './guards.js';

export function request(permission: string): Promise<{ granted: boolean }> {
  assertString(permission, 'permission');
  return KSM.invoke('permission.request', { permission });
}
