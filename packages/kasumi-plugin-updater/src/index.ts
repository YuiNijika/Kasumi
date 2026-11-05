export {
  check,
  download,
  install,
  onProgress,
  onStatus
} from 'kasumi-bridge/updater';
export type {
  KSMUpdaterApi,
  KSMUpdaterCheckResult,
  KSMUpdaterInstallResult,
  KSMUpdaterProgressPayload,
  KSMUpdaterStatusPayload
} from 'kasumi-bridge';

export const PLUGIN_ID = 'updater' as const;
