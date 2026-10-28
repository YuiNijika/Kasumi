export type KSMPlatform = 'windows' | 'android';

export type KSMErrorCode =
  | 'INVALID_ARGS'
  | 'UNKNOWN_COMMAND'
  | 'PERMISSION_DENIED'
  | 'NOT_SUPPORTED'
  | 'NOT_FOUND'
  | 'IO_ERROR'
  | 'CANCELLED'
  | 'TIMEOUT'
  | 'BUSY'
  | 'INTERNAL_ERROR';

export interface KSMError {
  code: KSMErrorCode;
  message: string;
  detail?: unknown;
}

export interface KSMRequest<TArgs> {
  type: 'request';
  id: string;
  cmd: string;
  args: TArgs;
  timeoutMs?: number;
  windowId?: string;
}

export interface KSMOkResponse<TResult> {
  type: 'response';
  id: string;
  ok: true;
  data: TResult;
}

export interface KSMErrorResponse {
  type: 'response';
  id: string;
  ok: false;
  error: KSMError;
}

export type KSMResponse<TResult> = KSMOkResponse<TResult> | KSMErrorResponse;

export interface KSMFileHandle {
  file: string;
}

export interface KSMEvent<TPayload = unknown> {
  type: 'event';
  event: string;
  payload: TPayload;
}

export interface KSMTransport {
  send(message: KSMRequest<unknown> | { type: 'cancel'; id: string }): void;
  onMessage(listener: (message: KSMResponse<unknown> | KSMEvent) => void): () => void;
}

export interface KSMFsApi {
  readText(args: { path: string }): Promise<string | KSMFileHandle>;
  writeText(args: { path: string; content: string }): Promise<{ written: boolean }>;
  exists(args: { path: string }): Promise<{ exists: boolean }>;
  remove(args: { path: string }): Promise<{ removed: boolean }>;
}

export interface KSMWindowApi {
  setTitle(title: string): Promise<{ updated: boolean }>;
  minimize(): Promise<{ updated: boolean }>;
  maximize(): Promise<{ updated: boolean }>;
  close(): Promise<{ closed: boolean }>;
}

export interface KSMAppApi {
  getInfo(): Promise<KSMAppInfo>;
  exit(): Promise<{ exited: boolean }>;
}

export interface KSMDialogApi {
  message(args: { title: string; message: string }): Promise<{ confirmed: boolean }>;
  confirm(args: { title: string; message: string }): Promise<{ confirmed: boolean }>;
  openFile(args?: { filters?: string[] }): Promise<{ file: string | null }>;
  saveFile(args?: { suggestedName?: string }): Promise<{ file: string | null }>;
}

export interface KSMClipboardApi {
  readText(): Promise<{ text: string }>;
  writeText(text: string): Promise<{ written: boolean }>;
}

export interface KSMNotificationApi {
  show(args: { title: string; body: string }): Promise<{ shown: boolean }>;
}

export interface KSMPermissionApi {
  request(permission: string): Promise<{ granted: boolean }>;
}

export interface KSMCapabilities {
  platform: KSMPlatform;
  commands: string[];
  permissions: string[];
}

export interface KSMAppInfo {
  name: string;
  version: string;
  platform: KSMPlatform;
  language: string;
}
