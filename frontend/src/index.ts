export { KSM } from './bridge/ksm.js';
export { KSMWebMessageTransport } from './bridge/transport.js';
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
} from './generated/contract.js';
export * as app from './api/app.js';
export * as clipboard from './api/clipboard.js';
export * as device from './api/device.js';
export * as dialog from './api/dialog.js';
export * as fs from './api/fs.js';
export * as http from './api/http.js';
export * as notification from './api/notification.js';
export * as permission from './api/permission.js';
export * as shell from './api/shell.js';
export * as window from './api/window.js';
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
} from './bridge/types.js';
