import { KSM } from '../bridge/ksm';
import { assertPositiveInteger, assertString } from './guards';
import type { KSMWindowSize } from '../bridge/types';

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

export function setSize(args: KSMWindowSize): Promise<{ updated: boolean }> {
  assertPositiveInteger(args.width, 'width');
  assertPositiveInteger(args.height, 'height');
  return KSM.invoke('window.setSize', args);
}

export function close(): Promise<{ closed: boolean }> {
  return KSM.invoke('window.close', {});
}
