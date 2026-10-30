import { KSM } from '../bridge/ksm';
import { assertHttpUrl } from './guards';
import type { KSMShellOpenResult } from '../bridge/types';

export function openExternal(args: { url: string }): Promise<KSMShellOpenResult> {
  assertHttpUrl(args.url, 'url');
  return KSM.invoke('shell.openExternal', args);
}
