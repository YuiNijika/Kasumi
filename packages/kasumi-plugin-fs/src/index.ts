export * from 'kasumi-bridge/fs';
export type {
  KSMDirectoryEntry,
  KSMFileHandle,
  KSMFileKind,
  KSMFileMetadata,
  KSMFsWalkArgs,
  KSMFsWalkResult
} from 'kasumi-bridge';

export const PLUGIN_ID = 'fs' as const;
