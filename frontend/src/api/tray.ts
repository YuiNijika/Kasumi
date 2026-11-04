import { KSM } from '../bridge/ksm.js';

export function show(): Promise<{ shown: boolean }> {
  return KSM.invoke('tray.show', {});
}

export function hide(): Promise<{ hidden: boolean }> {
  return KSM.invoke('tray.hide', {});
}
