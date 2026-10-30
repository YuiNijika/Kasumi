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
export * as dialog from './api/dialog';
export * as fs from './api/fs';
export * as notification from './api/notification';
export * as permission from './api/permission';
export * as window from './api/window';
export type {
  KSMAppInfo,
  KSMAppApi,
  KSMClipboardApi,
  KSMCapabilities,
  KSMDialogApi,
  KSMError,
  KSMErrorCode,
  KSMEvent,
  KSMFileHandle,
  KSMFileKind,
  KSMFileMetadata,
  KSMFsApi,
  KSMDirectoryEntry,
  KSMNotificationApi,
  KSMPlatform,
  KSMPermissionApi,
  KSMRequest,
  KSMOkResponse,
  KSMErrorResponse,
  KSMResponse,
  KSMTransport,
  KSMWindowApi
} from './bridge/types';
