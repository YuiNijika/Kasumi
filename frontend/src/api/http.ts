import { KSM } from '../bridge/ksm.js';
import { assertBoolean, assertHttpUrl, assertVirtualPath } from './guards.js';
import type { KSMHttpDownloadResult } from '../bridge/types.js';

const DOWNLOAD_TIMEOUT_MS = 120000;

export function download(args: {
  url: string;
  path: string;
  overwrite?: boolean;
}): Promise<KSMHttpDownloadResult> {
  assertHttpUrl(args.url, 'url', true);
  assertVirtualPath(args.path, true, false);
  if (args.overwrite !== undefined) {
    assertBoolean(args.overwrite, 'overwrite');
  }
  return KSM.invoke('http.download', args, DOWNLOAD_TIMEOUT_MS);
}
