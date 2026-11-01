import { KSM } from '../bridge/ksm.js';
import { assertHttpUrl } from './guards.js';
import type { KSMShellOpenResult } from '../bridge/types.js';

export function openExternal(args: { url: string }): Promise<KSMShellOpenResult> {
  assertHttpUrl(args.url, 'url');
  return KSM.invoke('shell.openExternal', args);
}
