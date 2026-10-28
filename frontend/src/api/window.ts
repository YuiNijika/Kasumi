import { KSM } from '../bridge/ksm';
import { assertString } from './guards';

export function setTitle(title: string): Promise<{ updated: boolean }> {
  assertString(title, 'title');
  return KSM.invoke('window.setTitle', { title });
}

export function minimize(): Promise<{ updated: boolean }> {
  return KSM.invoke('window.minimize', {});
}

export function maximize(): Promise<{ updated: boolean }> {
  return KSM.invoke('window.maximize', {});
}

export function close(): Promise<{ closed: boolean }> {
  return KSM.invoke('window.close', {});
}
