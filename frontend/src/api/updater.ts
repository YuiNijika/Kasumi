import { KSM } from '../bridge/ksm.js';
import { assertBoundedString } from './guards.js';
import type {
  KSMUpdaterInstallArgs,
  KSMUpdaterInstallResult,
  KSMUpdaterProgressPayload,
  KSMUpdaterStatusPayload
} from '../bridge/types.js';

export function check(): Promise<{ available: boolean; version: string | null }> {
  return KSM.invoke('updater.check', {});
}

export function download(args: {
  version: string;
}): Promise<{ downloaded: boolean }> {
  assertBoundedString(args.version, 'version', 128);
  return KSM.invoke('updater.download', args);
}

export function install(
  args: KSMUpdaterInstallArgs
): Promise<KSMUpdaterInstallResult> {
  assertBoundedString(args.version, 'version', 128);
  return KSM.invoke<KSMUpdaterInstallResult, KSMUpdaterInstallArgs>(
    'updater.install',
    args,
    600000
  );
}

export function onProgress(
  listener: (payload: KSMUpdaterProgressPayload) => void
): () => void {
  return KSM.on('updater.progress', listener);
}

export function onStatus(
  listener: (payload: KSMUpdaterStatusPayload) => void
): () => void {
  return KSM.on('updater.status', listener);
}
