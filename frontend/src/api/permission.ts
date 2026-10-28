import { KSM } from '../bridge/ksm';
import { assertString } from './guards';

export function request(permission: string): Promise<{ granted: boolean }> {
  assertString(permission, 'permission');
  return KSM.invoke('permission.request', { permission });
}
