export { KSM } from './bridge/ksm';
export { KSMWebMessageTransport } from './bridge/transport';
export {
  KSM_COMMANDS,
  KSM_ERROR_CODES,
  KSM_EVENTS,
  KSM_PERMISSIONS,
  KSM_ANDROID_COMMANDS,
  KSM_WINDOWS_COMMANDS,
  validateKSMArgs,
  type KSMCommandName,
  type KSMEventName,
  type KSMPermission
} from './generated/contract';
export * as app from './api/app';
export * as clipboard from './api/clipboard';
export * as device from './api/device';
export * as dialog from './api/dialog';
export * as fs from './api/fs';
export * as http from './api/http';
export * as notification from './api/notification';
export * as permission from './api/permission';
export * as shell from './api/shell';
export * as window from './api/window';
export type {
  KSMAppInfo,
  KSMAppApi,
  KSMClipboardApi,
  KSMDeviceApi,
  KSMDeviceInfo,
  KSMCapabilities,
  KSMDialogApi,
  KSMError,
  KSMErrorCode,
  KSMEvent,
  KSMFileHandle,
  KSMFileKind,
  KSMFileMetadata,
  KSMFsApi,
  KSMHttpApi,
  KSMHttpDownloadResult,
  KSMDirectoryEntry,
  KSMNotificationApi,
  KSMPlatform,
  KSMPermissionApi,
  KSMShellApi,
  KSMShellOpenResult,
  KSMRequest,
  KSMOkResponse,
  KSMErrorResponse,
  KSMResponse,
  KSMTransport,
  KSMWindowApi,
  KSMWindowSize
} from './bridge/types';
