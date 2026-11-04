import { KSM } from '../bridge/ksm.js';
import { assertHttpUrl, assertVirtualStatPath } from './guards.js';
import type { KSMShellOpenResult } from '../bridge/types.js';

export function openExternal(args: { url: string }): Promise<KSMShellOpenResult> {
  assertHttpUrl(args.url, 'url');
  return KSM.invoke('shell.openExternal', args);
}

export function openPath(args: { path: string }): Promise<{ opened: boolean }> {
  assertVirtualStatPath(args.path);
  return KSM.invoke('shell.openPath', args);
}

export function showInFolder(args: {
  path: string;
}): Promise<{ shown: boolean }> {
  assertVirtualStatPath(args.path);
  return KSM.invoke('shell.showInFolder', args);
}
