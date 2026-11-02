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
  | 'RESOURCE_LIMIT'
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

export type KSMFileKind = 'file' | 'directory';

export interface KSMFileMetadata {
  kind: KSMFileKind;
  size: number;
  modifiedAt: number | null;
}

export interface KSMDirectoryEntry extends KSMFileMetadata {
  name: string;
  path: string;
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
  readDir(args: { path: string }): Promise<{ entries: KSMDirectoryEntry[] }>;
  mkdir(args: { path: string; recursive?: boolean }): Promise<{ created: boolean }>;
  stat(args: { path: string }): Promise<KSMFileMetadata>;
}

export interface KSMWindowApi {
  setTitle(title: string): Promise<{ updated: boolean }>;
  minimize(): Promise<{ updated: boolean }>;
  maximize(): Promise<{ updated: boolean }>;
  setSize(args: KSMWindowSize): Promise<{ updated: boolean }>;
  close(): Promise<{ closed: boolean }>;
}

export interface KSMWindowSize {
  width: number;
  height: number;
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

export interface KSMHttpDownloadResult {
  path: string;
  bytes: number;
}

export interface KSMHttpApi {
  download(args: {
    url: string;
    path: string;
    overwrite?: boolean;
  }): Promise<KSMHttpDownloadResult>;
}

export interface KSMShellOpenResult {
  opened: boolean;
}

export interface KSMShellApi {
  openExternal(args: { url: string }): Promise<KSMShellOpenResult>;
}

export type KSMJsonValue =
  | string
  | number
  | boolean
  | null
  | KSMJsonValue[]
  | { [key: string]: KSMJsonValue };

export interface KSMStoreGetResult {
  found: boolean;
  value: KSMJsonValue;
}

export interface KSMStoreApi {
  get(args: { key: string }): Promise<KSMStoreGetResult>;
  set(args: { key: string; value: KSMJsonValue }): Promise<{ updated: true }>;
  delete(args: { key: string }): Promise<{ deleted: boolean }>;
  clear(): Promise<{ cleared: true }>;
  keys(): Promise<{ keys: string[] }>;
}

export interface KSMPathDirectories {
  data: string;
  cache: string;
  temp: string;
  resource: string;
  asset: string;
}

export interface KSMPathApi {
  dirs(): Promise<KSMPathDirectories>;
  join(args: { base: string; segments: string[] }): Promise<{ path: string }>;
  normalize(args: { path: string }): Promise<{ path: string }>;
  dirname(args: { path: string }): Promise<{ path: string }>;
  basename(args: {
    path: string;
    suffix?: string;
  }): Promise<{ name: string }>;
  extname(args: { path: string }): Promise<{ extension: string }>;
  isAbsolute(args: { path: string }): Promise<{ absolute: boolean }>;
}

export interface KSMOperatingSystemInfo {
  platform: KSMPlatform;
  architecture: string;
  version: string;
  locale: string;
  timezone: string;
  appVersion: string;
}

export interface KSMOperatingSystemApi {
  getInfo(): Promise<KSMOperatingSystemInfo>;
}

export type KSMLogLevel = 'trace' | 'debug' | 'info' | 'warn' | 'error';

export interface KSMLogApi {
  write(args: {
    level: KSMLogLevel;
    message: string;
    target?: string;
  }): Promise<{ logged: true }>;
}

export interface KSMDeviceInfo {
  platform: 'android';
  osVersion: string;
  model: string;
  manufacturer: string;
  appVersion: string;
  language: string;
  screenWidth: number;
  screenHeight: number;
  density: number;
  deviceName: string;
  timezone: string;
  architecture: string;
}

export interface KSMDeviceApi {
  getInfo(): Promise<KSMDeviceInfo>;
}

export type KSMSqlDriver = 'sqlite' | 'mysql' | 'postgres' | 'mongodb';

export type KSMSqlValue = string | number | boolean | null;

export interface KSMSqlDriversResult {
  drivers: KSMSqlDriver[];
}

export interface KSMSqlOpenResult {
  connectionId: string;
}

export interface KSMSqlQueryResult {
  columns: string[];
  rows: Array<Record<string, KSMSqlValue>>;
}

export interface KSMSqlExecuteResult {
  rowsAffected: number;
  lastInsertId: number | null;
}

export interface KSMSqlApi {
  drivers(): Promise<KSMSqlDriversResult>;
  open(args: { driver: KSMSqlDriver; url: string }): Promise<KSMSqlOpenResult>;
  query(args: {
    connectionId: string;
    statement: string;
    params?: KSMSqlValue[];
  }): Promise<KSMSqlQueryResult>;
  execute(args: {
    connectionId: string;
    statement: string;
    params?: KSMSqlValue[];
  }): Promise<KSMSqlExecuteResult>;
  close(args: { connectionId: string }): Promise<{ closed: boolean }>;
}

export interface KSMCapabilities {
  platform: KSMPlatform;
  commands: string[];
  permissions: string[];
  plugins: string[];
}

export interface KSMAppInfo {
  name: string;
  version: string;
  platform: KSMPlatform;
  language: string;
}
