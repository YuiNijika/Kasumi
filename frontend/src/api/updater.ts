import { KSM } from '../bridge/ksm.js';
import { assertBoundedString } from './guards.js';

export function check(): Promise<{ available: boolean; version: string | null }> {
  return KSM.invoke('updater.check', {});
}

export function download(args: {
  version: string;
}): Promise<{ downloaded: boolean }> {
  assertBoundedString(args.version, 'version', 128);
  return KSM.invoke('updater.download', args);
}
