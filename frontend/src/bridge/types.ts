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
  | 'INTERNAL_ERROR'
  | 'MEDIA_UNSUPPORTED'
  | 'AUDIO_DEVICE_BUSY'
  | 'UPDATE_VERIFICATION_FAILED'
  | 'DEEP_LINK_INVALID'
  | 'EXTENSION_CONFLICT';

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
  appendText(args: { path: string; content: string }): Promise<{ written: boolean }>;
  copy(args: { from: string; to: string }): Promise<{ copied: boolean }>;
  move(args: { from: string; to: string }): Promise<{ moved: boolean }>;
  readBytes(args: { path: string }): Promise<{ base64: string }>;
  writeBytes(args: { path: string; base64: string }): Promise<{ written: boolean }>;
  exists(args: { path: string }): Promise<{ exists: boolean }>;
  remove(args: { path: string }): Promise<{ removed: boolean }>;
  readDir(args: { path: string }): Promise<{ entries: KSMDirectoryEntry[] }>;
  mkdir(args: { path: string; recursive?: boolean }): Promise<{ created: boolean }>;
  stat(args: { path: string }): Promise<KSMFileMetadata>;
  walk(args: KSMFsWalkArgs): Promise<KSMFsWalkResult>;
  scope: KSMFsScopeApi;
}

export interface KSMWindowApi {
  setTitle(title: string): Promise<{ updated: boolean }>;
  minimize(): Promise<{ updated: boolean }>;
  maximize(): Promise<{ updated: boolean }>;
  setSize(args: KSMWindowSize): Promise<{ updated: boolean }>;
  close(): Promise<{ closed: boolean }>;
  create(args: KSMWindowCreateOptions): Promise<KSMWindowState>;
  show(args?: KSMWindowTargetArgs): Promise<{ shown: boolean }>;
  hide(args?: KSMWindowTargetArgs): Promise<{ hidden: boolean }>;
  setPosition(args: KSMWindowPositionArgs): Promise<{ updated: boolean }>;
  setAlwaysOnTop(args: KSMWindowFlagArgs): Promise<{ updated: boolean }>;
  setTransparent(args: KSMWindowFlagArgs): Promise<{ updated: boolean }>;
  getState(args?: KSMWindowTargetArgs): Promise<KSMWindowState>;
  emit(
    event: string,
    payload: KSMJsonValue,
    windowId?: string
  ): Promise<KSMEmitResult>;
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
  openFolder(args?: KSMOpenFolderArgs): Promise<KSMOpenFolderResult>;
  openFiles(args?: KSMOpenFilesArgs): Promise<KSMOpenFilesResult>;
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
  request(args: KSMHttpRequestArgs): Promise<KSMHttpResponse>;
  transfer: KSMHttpTransferApi;
  cookies: KSMHttpCookiesApi;
}

export interface KSMShellOpenResult {
  opened: boolean;
}

export interface KSMShellApi {
  openExternal(args: { url: string }): Promise<KSMShellOpenResult>;
  openPath(args: { path: string }): Promise<{ opened: boolean }>;
  showInFolder(args: { path: string }): Promise<{ shown: boolean }>;
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
  home: string;
  config: string;
  log: string;
  runtime: string;
  executable: string;
  desktop: string;
  download: string;
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
  devMode: boolean;
  hostname?: string;
  username?: string;
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

export interface KSMAppInstanceApi {
  state(): Promise<{ primary: boolean }>;
  focus(): Promise<{ focused: boolean }>;
  onSecondInstance(
    listener: (payload: KSMSecondInstanceReceivedPayload) => void
  ): () => void;
}

export interface KSMUpdaterCheckResult {
  available: boolean;
  version: string | null;
}

export interface KSMUpdaterApi {
  check(): Promise<KSMUpdaterCheckResult>;
  download(args: { version: string }): Promise<{ downloaded: boolean }>;
  install(args: KSMUpdaterInstallArgs): Promise<KSMUpdaterInstallResult>;
  onProgress(listener: (payload: KSMUpdaterProgressPayload) => void): () => void;
  onStatus(listener: (payload: KSMUpdaterStatusPayload) => void): () => void;
}

export interface KSMGlobalShortcutApi {
  register(args: { shortcut: string }): Promise<{ registered: boolean }>;
  unregister(args: { shortcut: string }): Promise<{ unregistered: boolean }>;
  onTriggered(
    listener: (payload: KSMGlobalShortcutTriggeredPayload) => void
  ): () => void;
}

export interface KSMTrayApi {
  show(): Promise<{ shown: boolean }>;
  hide(): Promise<{ hidden: boolean }>;
  configure(args: KSMTrayMenuConfig): Promise<KSMTrayConfigureResult>;
  onAction(listener: (payload: KSMTrayActionPayload) => void): () => void;
  onClick(listener: (payload: KSMTrayClickPayload) => void): () => void;
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
  appId?: string;
  commands: string[];
  permissions: string[];
  plugins: Array<{
    id: string;
    name: string;
    version: string;
    enabled: boolean;
  }>;
  extensionIds?: string[];
  extensionCommands?: string[];
  extensionEvents?: string[];
}

export interface KSMAppInfo {
  name: string;
  version: string;
  platform: KSMPlatform;
  language: string;
}

export type KSMHttpMethod = 'GET' | 'POST';
export type KSMHttpResponseMode = 'text' | 'json' | 'file' | 'empty';
export type KSMHttpSameSite = 'strict' | 'lax' | 'none';

export interface KSMHttpHeader {
  name: string;
  value: string;
}

export interface KSMHttpCookie {
  name: string;
  value: string;
  domain?: string;
  path?: string;
  expiresAt?: number | null;
  secure?: boolean;
  httpOnly?: boolean;
  sameSite?: KSMHttpSameSite;
}

export type KSMHttpRequestBody =
  | { kind: 'empty' }
  | { kind: 'text'; text: string }
  | { kind: 'json'; value: KSMJsonValue }
  | { kind: 'file'; file: string };

export interface KSMHttpRequestArgs {
  url: string;
  method?: KSMHttpMethod;
  headers?: KSMHttpHeader[];
  body?: KSMHttpRequestBody;
  response?: KSMHttpResponseMode;
  timeoutMs?: number;
}

export type KSMHttpResponseBody =
  | { kind: 'empty' }
  | { kind: 'text'; text: string }
  | { kind: 'json'; value: KSMJsonValue }
  | { kind: 'file'; file: string; bytes: number };

export interface KSMHttpResponse {
  status: number;
  headers: KSMHttpHeader[];
  body: KSMHttpResponseBody;
}

export interface KSMHttpTransferStartArgs {
  url: string;
  method?: KSMHttpMethod;
  headers?: KSMHttpHeader[];
  body?: KSMHttpRequestBody;
  path?: string;
  overwrite?: boolean;
  timeoutMs?: number;
}

export interface KSMHttpTransferStartResult {
  transferId: string;
}

export interface KSMHttpTransferCancelArgs {
  transferId: string;
}

export interface KSMHttpTransferCancelResult {
  cancelled: boolean;
}

export interface KSMHttpTransferProgressPayload {
  transferId: string;
  bytes: number;
  totalBytes: number | null;
}

export interface KSMHttpTransferCompletedPayload {
  transferId: string;
  status: number;
  bytes: number;
  path: string | null;
}

export interface KSMHttpTransferFailedPayload {
  transferId: string;
  code: KSMErrorCode;
  message: string;
}

export interface KSMHttpTransferApi {
  start(args: KSMHttpTransferStartArgs): Promise<KSMHttpTransferStartResult>;
  cancel(args: KSMHttpTransferCancelArgs): Promise<KSMHttpTransferCancelResult>;
  onProgress(
    listener: (payload: KSMHttpTransferProgressPayload) => void
  ): () => void;
  onCompleted(
    listener: (payload: KSMHttpTransferCompletedPayload) => void
  ): () => void;
  onFailed(
    listener: (payload: KSMHttpTransferFailedPayload) => void
  ): () => void;
}

export interface KSMHttpCookiesGetArgs {
  url: string;
}

export interface KSMHttpCookiesGetResult {
  cookies: KSMHttpCookie[];
}

export interface KSMHttpCookiesSetArgs {
  url: string;
  cookies: KSMHttpCookie[];
}

export interface KSMHttpCookiesClearArgs {
  url: string;
}

export interface KSMHttpCookiesApi {
  get(args: KSMHttpCookiesGetArgs): Promise<KSMHttpCookiesGetResult>;
  set(args: KSMHttpCookiesSetArgs): Promise<{ updated: boolean }>;
  clear(args: KSMHttpCookiesClearArgs): Promise<{ cleared: boolean }>;
}

export interface KSMDirectoryFilter {
  extensions?: string[];
  mimeTypes?: string[];
}

export interface KSMAppDirectoryScope {
  id: string;
  path: string;
  persisted: boolean;
}

export interface KSMOpenFolderArgs {
  persist?: boolean;
  filters?: KSMDirectoryFilter;
}

export interface KSMOpenFolderResult {
  id: string;
  path: string;
  files: string[];
}

export interface KSMOpenFilesArgs {
  filters?: string[];
}

export interface KSMOpenFilesResult {
  id: string;
  path: string;
  files: string[];
}

export interface KSMFsWalkArgs {
  path: string;
  maxDepth?: number;
  filter?: KSMDirectoryFilter;
}

export interface KSMFsWalkResult {
  entries: KSMDirectoryEntry[];
}

export interface KSMScopeReleaseArgs {
  scopeId: string;
}

export interface KSMScopeReleaseResult {
  released: boolean;
}

export interface KSMFsScopeApi {
  release(args: KSMScopeReleaseArgs): Promise<KSMScopeReleaseResult>;
}

export interface KSMMediaMetadata {
  title: string | null;
  artist: string | null;
  album: string | null;
  durationMs: number | null;
  cover: string | null;
  lyric: string | null;
  format: string | null;
}

export interface KSMMediaProbeArgs {
  path: string;
}

export interface KSMFfmpegDetection {
  available: boolean;
  path: string | null;
  version: string | null;
  source: 'bundled' | 'configured' | 'picked' | null;
}

export interface KSMFfmpegValidateResult {
  valid: boolean;
  version: string | null;
}

export interface KSMFfmpegSetPathArgs {
  path: string;
}

export interface KSMFfmpegApi {
  detect(): Promise<KSMFfmpegDetection>;
  validate(): Promise<KSMFfmpegValidateResult>;
  setPath(args: KSMFfmpegSetPathArgs): Promise<{ updated: boolean }>;
  pickExecutable(): Promise<{ path: string | null }>;
}

export interface KSMMediaApi {
  probe(args: KSMMediaProbeArgs): Promise<KSMMediaMetadata>;
  ffmpeg: KSMFfmpegApi;
}

export type KSMAudioPlaybackState =
  | 'idle'
  | 'loading'
  | 'playing'
  | 'paused'
  | 'stopped'
  | 'error';

export type KSMAudioOutputMode = 'shared' | 'exclusive';

export type KSMAudioStateReason =
  | 'load'
  | 'play'
  | 'pause'
  | 'stop'
  | 'seek'
  | 'volume'
  | 'speed'
  | 'device'
  | 'error'
  | 'completed';

export interface KSMAudioDevice {
  id: string;
  name: string;
  isDefault: boolean;
}

export interface KSMAudioEqBand {
  frequency: number;
  gain: number;
}

export interface KSMAudioEqSettings {
  bands: KSMAudioEqBand[];
  preamp?: number;
}

export interface KSMAudioProbeArgs {
  source: string;
}

export interface KSMAudioProbeResult {
  durationMs: number | null;
  sampleRate: number | null;
  channels: number | null;
  codec: string | null;
}

export interface KSMAudioLoadArgs {
  source: string;
  startPositionMs?: number;
}

export interface KSMAudioSeekArgs {
  positionMs: number;
}

export interface KSMAudioVolumeArgs {
  volume: number;
  muted?: boolean;
}

export interface KSMAudioState {
  playback: KSMAudioPlaybackState;
  positionMs: number;
  durationMs: number | null;
  volume: number;
  muted: boolean;
  speed: number;
  outputMode: KSMAudioOutputMode;
  deviceId: string | null;
  source: string | null;
}

export interface KSMAudioTickPayload {
  positionMs: number;
  durationMs: number | null;
}

export interface KSMAudioEndedPayload {
  source: string | null;
}

export interface KSMAudioStateChangedPayload {
  state: KSMAudioState;
  reason: KSMAudioStateReason;
}

export interface KSMAudioErrorPayload {
  code: KSMErrorCode;
  message: string;
  source: string | null;
}

export interface KSMAudioDevicesApi {
  list(): Promise<{ devices: KSMAudioDevice[] }>;
  set(args: { deviceId: string }): Promise<{ updated: boolean }>;
}

export interface KSMAudioOutputApi {
  getMode(): Promise<{ mode: KSMAudioOutputMode }>;
  setExclusive(args: { enabled: boolean }): Promise<{ updated: boolean }>;
}

export interface KSMAudioEqApi {
  set(args: KSMAudioEqSettings): Promise<{ updated: boolean }>;
}

export interface KSMAudioSpeedApi {
  set(args: { speed: number }): Promise<{ updated: boolean }>;
}

export interface KSMAudioVolumeApi {
  set(args: KSMAudioVolumeArgs): Promise<{ updated: boolean }>;
}

export interface KSMAudioStateApi {
  get(): Promise<KSMAudioState>;
}

export interface KSMAudioApi {
  devices: KSMAudioDevicesApi;
  output: KSMAudioOutputApi;
  eq: KSMAudioEqApi;
  speed: KSMAudioSpeedApi;
  volume: KSMAudioVolumeApi;
  state: KSMAudioStateApi;
  probe(args: KSMAudioProbeArgs): Promise<KSMAudioProbeResult>;
  load(args: KSMAudioLoadArgs): Promise<{ loaded: boolean }>;
  play(): Promise<{ updated: boolean }>;
  pause(): Promise<{ updated: boolean }>;
  stop(): Promise<{ updated: boolean }>;
  seek(args: KSMAudioSeekArgs): Promise<{ updated: boolean }>;
  onTick(listener: (payload: KSMAudioTickPayload) => void): () => void;
  onEnded(listener: (payload: KSMAudioEndedPayload) => void): () => void;
  onStateChanged(
    listener: (payload: KSMAudioStateChangedPayload) => void
  ): () => void;
  onError(listener: (payload: KSMAudioErrorPayload) => void): () => void;
}

export type KSMMediaSessionPlaybackState =
  | 'playing'
  | 'paused'
  | 'stopped'
  | 'buffering';

export type KSMMediaSessionCommand =
  | 'play'
  | 'pause'
  | 'stop'
  | 'next'
  | 'previous'
  | 'seek';

export interface KSMMediaSessionMetadata {
  title: string;
  artist?: string | null;
  album?: string | null;
  durationMs?: number | null;
  positionMs?: number | null;
  artwork?: string | null;
  state: KSMMediaSessionPlaybackState;
}

export interface KSMMediaSessionCommandPayload {
  command: KSMMediaSessionCommand;
  positionMs?: number | null;
}

export interface KSMMediaSessionApi {
  update(args: KSMMediaSessionMetadata): Promise<{ updated: boolean }>;
  clear(): Promise<{ cleared: boolean }>;
  onCommand(
    listener: (payload: KSMMediaSessionCommandPayload) => void
  ): () => void;
}

export interface KSMWindowCreateOptions {
  url: string;
  title?: string;
  width?: number;
  height?: number;
  x?: number;
  y?: number;
  alwaysOnTop?: boolean;
  transparent?: boolean;
  resizable?: boolean;
  decorations?: boolean;
}

export interface KSMWindowTargetArgs {
  windowId?: string;
}

export interface KSMWindowPositionArgs extends KSMWindowTargetArgs {
  x: number;
  y: number;
}

export interface KSMWindowFlagArgs extends KSMWindowTargetArgs {
  enabled: boolean;
}

export interface KSMWindowState {
  windowId: string;
  visible: boolean;
  alwaysOnTop: boolean;
  transparent: boolean;
  width: number;
  height: number;
  x: number | null;
  y: number | null;
}

export interface KSMWindowVisibilityChangedPayload {
  windowId: string;
  visible: boolean;
}

export interface KSMWindowStateChangedPayload {
  windowId: string;
  state: KSMWindowState;
}

export type KSMTrayMenuItemKind = 'item' | 'checkbox' | 'separator';

export interface KSMTrayMenuItem {
  id: string;
  label?: string;
  kind?: KSMTrayMenuItemKind;
  enabled?: boolean;
  checked?: boolean;
  shortcut?: string;
}

export interface KSMTrayMenuConfig {
  icon?: string;
  tooltip?: string;
  items?: KSMTrayMenuItem[];
}

export interface KSMTrayConfigureResult {
  configured: boolean;
}

export interface KSMTrayActionPayload {
  id: string;
  checked: boolean | null;
}

export interface KSMTrayClickPayload {
  button: 'left' | 'right' | 'middle';
  x: number | null;
  y: number | null;
}

export interface KSMGlobalShortcutTriggeredPayload {
  shortcut: string;
}

export interface KSMUpdaterInstallArgs {
  version: string;
}

export interface KSMUpdaterInstallResult {
  installed: boolean;
  restarted: boolean;
}

export interface KSMUpdaterProgressPayload {
  version: string;
  phase: 'download' | 'verify' | 'install';
  bytes: number | null;
  totalBytes: number | null;
  percent: number | null;
}

export interface KSMUpdaterStatusPayload {
  version: string;
  status:
    | 'checking'
    | 'downloading'
    | 'verifying'
    | 'installing'
    | 'installed'
    | 'failed';
  message: string | null;
  code: KSMErrorCode | null;
}

export interface KSMDeepLink {
  url: string;
  source: 'coldStart' | 'runtime' | 'secondInstance';
}

export interface KSMPendingDeepLinkResult {
  pending: KSMDeepLink | null;
}

export interface KSMDeepLinkReceivedPayload {
  link: KSMDeepLink;
}

export interface KSMSecondInstanceReceivedPayload {
  args: string[];
  link: KSMDeepLink | null;
}

export interface KSMDeepLinksApi {
  getPending(): Promise<KSMPendingDeepLinkResult>;
  onReceived(
    listener: (payload: KSMDeepLinkReceivedPayload) => void
  ): () => void;
  onSecondInstance(
    listener: (payload: KSMSecondInstanceReceivedPayload) => void
  ): () => void;
}

export interface KSMDevtoolsApi {
  open(): Promise<{ opened: boolean }>;
}

export interface KSMEmitResult {
  emitted: boolean;
}

export type KSMExtensionSchemaType =
  | 'object'
  | 'array'
  | 'string'
  | 'number'
  | 'integer'
  | 'boolean'
  | 'null';

export interface KSMExtensionSchema {
  type: KSMExtensionSchemaType;
  properties?: Record<string, KSMExtensionSchema>;
  required?: string[];
  items?: KSMExtensionSchema;
  enum?: KSMJsonValue[];
  additionalProperties?: boolean;
}

export interface KSMExtensionCommand {
  name: string;
  platforms: KSMPlatform[];
  permissions: string[];
  args: KSMExtensionSchema;
  result: KSMExtensionSchema;
}

export interface KSMExtensionEvent {
  name: string;
  platforms: KSMPlatform[];
  payload: KSMExtensionSchema;
}

export interface KSMExtensionContract {
  id: string;
  commands: KSMExtensionCommand[];
  events: KSMExtensionEvent[];
}

export interface KSMExtensionsApi {
  configure(contract: KSMExtensionContract): void;
  emit(
    event: string,
    payload: KSMJsonValue,
    windowId?: string
  ): Promise<KSMEmitResult>;
  invoke<TResult extends KSMJsonValue>(
    command: string,
    args: KSMJsonValue,
    timeoutMs?: number
  ): Promise<TResult>;
  on<TPayload extends KSMJsonValue>(
    event: string,
    listener: (payload: TPayload) => void
  ): () => void;
}
