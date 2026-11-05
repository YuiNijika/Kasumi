import { KSMBus, MAX_REQUEST_TIMEOUT_MS } from './bus.js';
import { KSMWebMessageTransport } from './transport.js';
import {
  extensionJsonValue,
  matchesSchema,
  validateExtensionContract
} from './extension-contract.js';
import { KSM_PLANNED_COMMANDS, KSM_PLANNED_EVENTS } from './planned.js';
import {
  request as requestHttp,
  startTransfer as startHttpTransfer,
  cancelTransfer as cancelHttpTransfer,
  getCookies as getHttpCookies,
  setCookies as setHttpCookies,
  clearCookies as clearHttpCookies,
  onTransferProgress as onHttpTransferProgress,
  onTransferCompleted as onHttpTransferCompleted,
  onTransferFailed as onHttpTransferFailed
} from '../api/http.js';
import {
  walk as walkFs,
  releaseScope as releaseFsScope
} from '../api/fs.js';
import {
  openFolder,
  openFiles
} from '../api/dialog.js';
import {
  probe as probeMedia,
  detectFfmpeg,
  validateFfmpeg,
  setFfmpegPath,
  pickFfmpegExecutable
} from '../api/media.js';
import {
  listDevices as listAudioDevices,
  setDevice as setAudioDevice,
  getOutputMode as getAudioOutputMode,
  setExclusiveOutput as setExclusiveAudioOutput,
  setEq as setAudioEq,
  setSpeed as setAudioSpeed,
  probe as probeAudio,
  load as loadAudio,
  play as playAudio,
  pause as pauseAudio,
  stop as stopAudio,
  seek as seekAudio,
  setVolume as setAudioVolume,
  getState as getAudioState,
  onTick as onAudioTick,
  onEnded as onAudioEnded,
  onStateChanged as onAudioStateChanged,
  onError as onAudioError
} from '../api/audio.js';
import {
  update as updateMediaSession,
  clear as clearMediaSession,
  onCommand as onMediaSessionCommand
} from '../api/mediaSession.js';
import {
  create as createWindow,
  show as showWindow,
  hide as hideWindow,
  setPosition as setWindowPosition,
  setAlwaysOnTop as setWindowAlwaysOnTop,
  setTransparent as setWindowTransparent,
  getState as getWindowState
} from '../api/window.js';
import {
  configure as configureTray,
  onAction as onTrayAction,
  onClick as onTrayClick
} from '../api/tray.js';
import {
  onTriggered as onGlobalShortcutTriggered
} from '../api/globalShortcut.js';
import {
  install as installUpdate,
  onProgress as onUpdaterProgress,
  onStatus as onUpdaterStatus
} from '../api/updater.js';
import {
  onSecondInstance as onAppSecondInstance
} from '../api/appInstance.js';
import {
  getPending as getPendingDeepLink,
  onReceived as onDeepLinkReceived,
  onSecondInstance as onDeepLinkSecondInstance
} from '../api/deepLinks.js';
import { open as openDevtools } from '../api/devtools.js';
import {
  KSM_COMMANDS,
  KSM_EVENTS,
  validateKSMArgs,
  type KSMCommandName
} from '../generated/contract.js';
import {
  assertBoolean,
  assertBoundedString,
  assertHttpUrl,
  assertJsonValue,
  assertLogLevel,
  assertMessagePayloadSize,
  assertPositiveInteger,
  assertSqlConnectionId,
  assertSqlDriver,
  assertSqlParams,
  assertSqlStatement,
  assertStoreKey,
  assertString,
  assertVirtualDirectoryPath,
  assertVirtualPath,
  assertVirtualStatPath
} from '../api/guards.js';
import type {
  KSMAudioApi,
  KSMAppInfo,
  KSMAppApi,
  KSMAppInstanceApi,
  KSMClipboardApi,
  KSMDeviceApi,
  KSMDeviceInfo,
  KSMDirectoryEntry,
  KSMCapabilities,
  KSMDeepLinksApi,
  KSMDevtoolsApi,
  KSMDialogApi,
  KSMEmitResult,
  KSMExtensionContract,
  KSMExtensionsApi,
  KSMFileMetadata,
  KSMFsApi,
  KSMGlobalShortcutApi,
  KSMHttpApi,
  KSMHttpDownloadResult,
  KSMJsonValue,
  KSMMediaApi,
  KSMMediaSessionApi,
  KSMLogApi,
  KSMNotificationApi,
  KSMOperatingSystemApi,
  KSMOperatingSystemInfo,
  KSMPathApi,
  KSMPathDirectories,
  KSMPermissionApi,
  KSMShellApi,
  KSMSqlApi,
  KSMSqlDriver,
  KSMSqlValue,
  KSMStoreApi,
  KSMStoreGetResult,
  KSMTrayApi,
  KSMUpdaterApi,
  KSMUpdaterCheckResult,
  KSMTransport,
  KSMWindowApi,
  KSMWindowSize
} from './types.js';

export type KSMTextResult = string | { file: string };

export interface KSMEventApi {
  on<TPayload>(event: string, listener: (payload: TPayload) => void): () => void;
}

const EVENT_TIMEOUT_MS = 60000;
const KNOWN_COMMANDS = new Set<string>([
  ...KSM_COMMANDS,
  ...KSM_PLANNED_COMMANDS
]);
const KNOWN_EVENTS = new Set<string>([
  ...KSM_EVENTS,
  ...KSM_PLANNED_EVENTS
]);

export class KSM {
  private static bus: KSMBus | undefined;
  private static cachedCapabilities: KSMCapabilities | undefined;
  private static capabilitiesPromise: Promise<KSMCapabilities> | undefined;
  private static readonly eventListenerCounts = new Map<string, number>();
  private static readonly subscriptionSyncs = new Map<string, Promise<void>>();
  private static bridgeGeneration = 0;
  private static readonly extensionContracts = new Map<
    string,
    KSMExtensionContract
  >();
  private static readonly extensionCommands = new Map<
    string,
    KSMExtensionContract['commands'][number]
  >();
  private static readonly extensionEvents = new Map<
    string,
    KSMExtensionContract['events'][number]
  >();
  public static readonly app: KSMAppApi = {
    getInfo: (): Promise<KSMAppInfo> => KSM.getInfo(),
    exit: (): Promise<{ exited: boolean }> => KSM.invoke('app.exit', {})
  };
  public static readonly fs: KSMFsApi = {
    readText: (args): Promise<string | { file: string }> => {
      assertVirtualPath(args.path);
      return KSM.invoke('fs.readText', args);
    },
    appendText: (args): Promise<{ written: boolean }> => {
      assertVirtualPath(args.path, true);
      assertString(args.content, 'content');
      return KSM.invoke('fs.appendText', args);
    },
    copy: (args): Promise<{ copied: boolean }> => {
      assertVirtualPath(args.from);
      assertVirtualPath(args.to, true);
      return KSM.invoke('fs.copy', args);
    },
    move: (args): Promise<{ moved: boolean }> => {
      assertVirtualPath(args.from, true);
      assertVirtualPath(args.to, true);
      return KSM.invoke('fs.move', args);
    },
    readBytes: (args): Promise<{ base64: string }> => {
      assertVirtualPath(args.path);
      return KSM.invoke('fs.readBytes', args);
    },
    writeBytes: (args): Promise<{ written: boolean }> => {
      assertVirtualPath(args.path, true);
      assertString(args.base64, 'base64');
      return KSM.invoke('fs.writeBytes', args);
    },
    writeText: (args): Promise<{ written: boolean }> => {
      assertVirtualPath(args.path, true);
      assertString(args.content, 'content');
      return KSM.invoke('fs.writeText', args);
    },
    exists: (args): Promise<{ exists: boolean }> => {
      assertVirtualPath(args.path);
      return KSM.invoke('fs.exists', args);
    },
    remove: (args): Promise<{ removed: boolean }> => {
      assertVirtualPath(args.path, true);
      return KSM.invoke('fs.remove', args);
    },
    readDir: (args): Promise<{ entries: KSMDirectoryEntry[] }> => {
      assertVirtualDirectoryPath(args.path);
      return KSM.invoke('fs.readDir', args);
    },
    mkdir: (args): Promise<{ created: boolean }> => {
      assertVirtualDirectoryPath(args.path, true);
      if (args.recursive !== undefined) {
        assertBoolean(args.recursive, 'recursive');
      }
      return KSM.invoke('fs.mkdir', args);
    },
    stat: (args): Promise<KSMFileMetadata> => {
      assertVirtualStatPath(args.path);
      return KSM.invoke('fs.stat', args);
    },
    walk: (args) => walkFs(args),
    scope: {
      release: (args) => releaseFsScope(args)
    }
  };
  public static readonly dialog: KSMDialogApi = {
    message: (args): Promise<{ confirmed: boolean }> => KSM.invoke('dialog.message', args),
    confirm: (args): Promise<{ confirmed: boolean }> => KSM.invoke('dialog.confirm', args),
    openFile: (args = {}): Promise<{ file: string | null }> => KSM.invoke('dialog.openFile', args),
    saveFile: (args = {}): Promise<{ file: string | null }> => KSM.invoke('dialog.saveFile', args),
    openFolder: (args = {}) => openFolder(args),
    openFiles: (args = {}) => openFiles(args)
  };
  public static readonly clipboard: KSMClipboardApi = {
    readText: (): Promise<{ text: string }> => KSM.invoke('clipboard.readText', {}),
    writeText: (text): Promise<{ written: boolean }> => KSM.invoke('clipboard.writeText', { text })
  };
  public static readonly notification: KSMNotificationApi = {
    show: (args): Promise<{ shown: boolean }> => KSM.invoke('notification.show', args)
  };
  public static readonly permission: KSMPermissionApi = {
    request: (permission): Promise<{ granted: boolean }> => KSM.invoke('permission.request', { permission })
  };
  public static readonly http: KSMHttpApi = {
    download: (args): Promise<KSMHttpDownloadResult> => {
      assertHttpUrl(args.url, 'url', true);
      assertVirtualPath(args.path, true, false);
      if (args.overwrite !== undefined) {
        assertBoolean(args.overwrite, 'overwrite');
      }
      return KSM.invoke('http.download', args, 120000);
    },
    request: (args) => requestHttp(args),
    transfer: {
      start: (args) => startHttpTransfer(args),
      cancel: (args) => cancelHttpTransfer(args),
      onProgress: (listener) => onHttpTransferProgress(listener),
      onCompleted: (listener) => onHttpTransferCompleted(listener),
      onFailed: (listener) => onHttpTransferFailed(listener)
    },
    cookies: {
      get: (args) => getHttpCookies(args),
      set: (args) => setHttpCookies(args),
      clear: (args) => clearHttpCookies(args)
    }
  };
  public static readonly media: KSMMediaApi = {
    probe: (args) => probeMedia(args),
    ffmpeg: {
      detect: () => detectFfmpeg(),
      validate: () => validateFfmpeg(),
      setPath: (args) => setFfmpegPath(args),
      pickExecutable: () => pickFfmpegExecutable()
    }
  };
  public static readonly audio: KSMAudioApi = {
    devices: {
      list: () => listAudioDevices(),
      set: (args) => setAudioDevice(args)
    },
    output: {
      getMode: () => getAudioOutputMode(),
      setExclusive: (args) => setExclusiveAudioOutput(args)
    },
    eq: {
      set: (args) => setAudioEq(args)
    },
    speed: {
      set: (args) => setAudioSpeed(args)
    },
    volume: {
      set: (args) => setAudioVolume(args)
    },
    state: {
      get: () => getAudioState()
    },
    probe: (args) => probeAudio(args),
    load: (args) => loadAudio(args),
    play: () => playAudio(),
    pause: () => pauseAudio(),
    stop: () => stopAudio(),
    seek: (args) => seekAudio(args),
    onTick: (listener) => onAudioTick(listener),
    onEnded: (listener) => onAudioEnded(listener),
    onStateChanged: (listener) => onAudioStateChanged(listener),
    onError: (listener) => onAudioError(listener)
  };
  public static readonly mediaSession: KSMMediaSessionApi = {
    update: (args) => updateMediaSession(args),
    clear: () => clearMediaSession(),
    onCommand: (listener) => onMediaSessionCommand(listener)
  };
  public static readonly deepLinks: KSMDeepLinksApi = {
    getPending: () => getPendingDeepLink(),
    onReceived: (listener) => onDeepLinkReceived(listener),
    onSecondInstance: (listener) => onDeepLinkSecondInstance(listener)
  };
  public static readonly devtools: KSMDevtoolsApi = {
    open: () => openDevtools()
  };
  public static readonly extensions: KSMExtensionsApi = {
    configure: (contract): void => KSM.configureExtensions(contract),
    emit: (event, payload, windowId): Promise<KSMEmitResult> =>
      KSM.emit(event, payload, windowId),
    invoke: <TResult extends KSMJsonValue>(
      command: string,
      args: KSMJsonValue,
      timeoutMs = 10000
    ): Promise<TResult> => KSM.invoke<TResult, KSMJsonValue>(
      command,
      args,
      timeoutMs
    ),
    on: <TPayload extends KSMJsonValue>(
      event: string,
      listener: (payload: TPayload) => void
    ): (() => void) => KSM.on(event, listener)
  };
  public static readonly shell: KSMShellApi = {
    openExternal: (args): Promise<{ opened: boolean }> => {
      assertHttpUrl(args.url, 'url');
      return KSM.invoke('shell.openExternal', args);
    },
    openPath: (args): Promise<{ opened: boolean }> => {
      assertVirtualStatPath(args.path);
      return KSM.invoke('shell.openPath', args);
    },
    showInFolder: (args): Promise<{ shown: boolean }> => {
      assertVirtualStatPath(args.path);
      return KSM.invoke('shell.showInFolder', args);
    }
  };
  public static readonly appInstance: KSMAppInstanceApi = {
    state: (): Promise<{ primary: boolean }> =>
      KSM.invoke('app.singleInstance.state', {}),
    focus: (): Promise<{ focused: boolean }> =>
      KSM.invoke('app.singleInstance.focus', {}),
    onSecondInstance: (listener) => onAppSecondInstance(listener)
  };
  public static readonly updater: KSMUpdaterApi = {
    check: (): Promise<KSMUpdaterCheckResult> =>
      KSM.invoke('updater.check', {}),
    download: (args): Promise<{ downloaded: boolean }> => {
      assertBoundedString(args.version, 'version', 128);
      return KSM.invoke('updater.download', args);
    },
    install: (args) => installUpdate(args),
    onProgress: (listener) => onUpdaterProgress(listener),
    onStatus: (listener) => onUpdaterStatus(listener)
  };
  public static readonly globalShortcut: KSMGlobalShortcutApi = {
    register: (args): Promise<{ registered: boolean }> => {
      assertBoundedString(args.shortcut, 'shortcut', 256);
      return KSM.invoke('globalShortcut.register', args);
    },
    unregister: (args): Promise<{ unregistered: boolean }> => {
      assertBoundedString(args.shortcut, 'shortcut', 256);
      return KSM.invoke('globalShortcut.unregister', args);
    },
    onTriggered: (listener) => onGlobalShortcutTriggered(listener)
  };
  public static readonly tray: KSMTrayApi = {
    show: (): Promise<{ shown: boolean }> => KSM.invoke('tray.show', {}),
    hide: (): Promise<{ hidden: boolean }> => KSM.invoke('tray.hide', {}),
    configure: (args) => configureTray(args),
    onAction: (listener) => onTrayAction(listener),
    onClick: (listener) => onTrayClick(listener)
  };
  public static readonly device: KSMDeviceApi = {
    getInfo: (): Promise<KSMDeviceInfo> => KSM.invoke('device.getInfo', {})
  };
  public static readonly store: KSMStoreApi = {
    get: (args): Promise<KSMStoreGetResult> => {
      assertStoreKey(args.key);
      return KSM.invoke('store.get', args);
    },
    set: (args): Promise<{ updated: true }> => {
      assertStoreKey(args.key);
      assertJsonValue(args.value, 'value');
      return KSM.invoke('store.set', args);
    },
    delete: (args): Promise<{ deleted: boolean }> => {
      assertStoreKey(args.key);
      return KSM.invoke('store.delete', args);
    },
    clear: (): Promise<{ cleared: true }> => KSM.invoke('store.clear', {}),
    keys: (): Promise<{ keys: string[] }> => KSM.invoke('store.keys', {})
  };
  public static readonly path: KSMPathApi = {
    dirs: (): Promise<KSMPathDirectories> => KSM.invoke('path.dirs', {}),
    join: (args): Promise<{ path: string }> => {
      assertBoundedString(args.base, 'base', 8192);
      if (!Array.isArray(args.segments)) {
        throw new TypeError('segments must be an array.');
      }
      args.segments.forEach((segment, index) => {
        assertBoundedString(segment, `segments[${index}]`, 256);
      });
      return KSM.invoke('path.join', args);
    },
    normalize: (args): Promise<{ path: string }> => {
      assertBoundedString(args.path, 'path', 8192);
      return KSM.invoke('path.normalize', args);
    },
    dirname: (args): Promise<{ path: string }> => {
      assertBoundedString(args.path, 'path', 8192);
      return KSM.invoke('path.dirname', args);
    },
    basename: (args): Promise<{ name: string }> => {
      assertBoundedString(args.path, 'path', 8192);
      if (args.suffix !== undefined) {
        assertBoundedString(args.suffix, 'suffix', 256, true);
      }
      return KSM.invoke('path.basename', args);
    },
    extname: (args): Promise<{ extension: string }> => {
      assertBoundedString(args.path, 'path', 8192);
      return KSM.invoke('path.extname', args);
    },
    isAbsolute: (args): Promise<{ absolute: boolean }> => {
      assertString(args.path, 'path');
      if (args.path.length === 0) {
        throw new TypeError('path must not be empty.');
      }
      return KSM.invoke('path.isAbsolute', args);
    }
  };
  public static readonly os: KSMOperatingSystemApi = {
    getInfo: (): Promise<KSMOperatingSystemInfo> => KSM.invoke('os.getInfo', {})
  };
  public static readonly log: KSMLogApi = {
    write: (args): Promise<{ logged: true }> => {
      assertLogLevel(args.level);
      assertBoundedString(args.message, 'message', 65536);
      if (args.target !== undefined) {
        assertBoundedString(args.target, 'target', 128);
      }
      return KSM.invoke('log.write', args);
    }
  };
  public static readonly sql: KSMSqlApi = {
    drivers: (): Promise<{ drivers: KSMSqlDriver[] }> => KSM.invoke('sql.drivers', {}),
    open: (args): Promise<{ connectionId: string }> => {
      assertSqlDriver(args.driver);
      if (typeof args.url !== 'string' || args.url.length === 0) {
        throw new TypeError('url must be a non-empty string.');
      }
      return KSM.invoke('sql.open', args);
    },
    query: (args): Promise<{ columns: string[]; rows: Array<Record<string, KSMSqlValue>> }> => {
      assertSqlConnectionId(args.connectionId);
      assertSqlStatement(args.statement);
      if (args.params !== undefined) {
        assertSqlParams(args.params);
      }
      return KSM.invoke('sql.query', args);
    },
    execute: (args): Promise<{ rowsAffected: number; lastInsertId: number | null }> => {
      assertSqlConnectionId(args.connectionId);
      assertSqlStatement(args.statement);
      if (args.params !== undefined) {
        assertSqlParams(args.params);
      }
      return KSM.invoke('sql.execute', args);
    },
    close: (args): Promise<{ closed: boolean }> => {
      assertSqlConnectionId(args.connectionId);
      return KSM.invoke('sql.close', args);
    }
  };
  public static readonly window: KSMWindowApi = {
    setTitle: (title): Promise<{ updated: boolean }> => KSM.invoke('window.setTitle', { title }),
    minimize: (): Promise<{ updated: boolean }> => KSM.invoke('window.minimize', {}),
    maximize: (): Promise<{ updated: boolean }> => KSM.invoke('window.maximize', {}),
    setSize: (args: KSMWindowSize): Promise<{ updated: boolean }> => {
      assertPositiveInteger(args.width, 'width');
      assertPositiveInteger(args.height, 'height');
      return KSM.invoke('window.setSize', args);
    },
    close: (): Promise<{ closed: boolean }> => KSM.invoke('window.close', {}),
    create: (args) => createWindow(args),
    show: (args = {}) => showWindow(args),
    hide: (args = {}) => hideWindow(args),
    setPosition: (args) => setWindowPosition(args),
    setAlwaysOnTop: (args) => setWindowAlwaysOnTop(args),
    setTransparent: (args) => setWindowTransparent(args),
    getState: (args = {}) => getWindowState(args),
    emit: (event, payload, windowId) => KSM.emit(event, payload, windowId)
  };
  public static readonly events: KSMEventApi = {
    on: <TPayload>(event: string, listener: (payload: TPayload) => void): (() => void) => {
      if (!KSM.isKnownEvent(event)) {
        throw new TypeError(`Unknown Kasumi event: ${event}`);
      }
      const bus = KSM.requireBus();
      const unsubscribe = bus.on(event, listener);
      const count = (KSM.eventListenerCounts.get(event) ?? 0) + 1;
      KSM.eventListenerCounts.set(event, count);
      KSM.queueSubscriptionSync(event);
      return (): void => {
        unsubscribe();
        KSM.releaseEventListenerCount(event);
        KSM.queueSubscriptionSync(event);
      };
    }
  };

  private constructor() {
    throw new Error('KSM is a static entry point.');
  }

  public static init(transport?: KSMTransport): void {
    KSM.initialize(transport);
  }

  public static initialize(transport?: KSMTransport): void {
    const resolvedTransport = transport ?? new KSMWebMessageTransport();
    KSM.disposeBus(KSM.bus);
    KSM.bridgeGeneration += 1;
    KSM.eventListenerCounts.clear();
    KSM.subscriptionSyncs.clear();
    KSM.cachedCapabilities = undefined;
    KSM.capabilitiesPromise = undefined;
    KSM.extensionContracts.clear();
    KSM.extensionCommands.clear();
    KSM.extensionEvents.clear();
    KSM.bus = new KSMBus(resolvedTransport);
    KSM.bus.on('webview.reloaded', () => {
      void KSM.recoverAfterReload();
    });
  }

  public static dispose(): void {
    KSM.disposeBus(KSM.bus);
    KSM.bridgeGeneration += 1;
    KSM.bus = undefined;
    KSM.cachedCapabilities = undefined;
    KSM.capabilitiesPromise = undefined;
    KSM.eventListenerCounts.clear();
    KSM.subscriptionSyncs.clear();
    KSM.extensionContracts.clear();
    KSM.extensionCommands.clear();
    KSM.extensionEvents.clear();
  }

  public static shutdown(): void {
    KSM.dispose();
  }

  public static async ping(): Promise<{ pong: true }> {
    return KSM.requireBus().invoke<{ pong: true }, Record<string, never>>(
      'ping',
      {}
    );
  }

  public static async capabilities(): Promise<KSMCapabilities> {
    if (KSM.cachedCapabilities) {
      return KSM.withExtensionCapabilities(KSM.cachedCapabilities);
    }
    if (!KSM.capabilitiesPromise) {
      KSM.capabilitiesPromise = KSM.requireBus()
        .invoke<KSMCapabilities, Record<string, never>>(
          'capabilities.query',
          {}
        )
        .then((capabilities) => {
          KSM.cachedCapabilities = capabilities;
          return KSM.withExtensionCapabilities(capabilities);
        })
        .finally(() => {
          KSM.capabilitiesPromise = undefined;
        });
    }
    return KSM.capabilitiesPromise;
  }

  public static async get(): Promise<KSMCapabilities> {
    return KSM.capabilities();
  }

  public static async getInfo(): Promise<KSMAppInfo> {
    return KSM.requireBus().invoke<KSMAppInfo, Record<string, never>>(
      'app.getInfo',
      {}
    );
  }

  public static async invoke<TResult, TArgs>(
    cmd: string,
    args: TArgs,
    timeoutMs = 10000
  ): Promise<TResult> {
    if (
      !Number.isSafeInteger(timeoutMs) ||
      timeoutMs <= 0 ||
      timeoutMs > MAX_REQUEST_TIMEOUT_MS
    ) {
      const error = new Error(
        `timeoutMs must be an integer between 1 and ${MAX_REQUEST_TIMEOUT_MS}.`
      );
      error.name = 'INVALID_ARGS';
      throw error;
    }
    if (
      (KSM_COMMANDS as readonly string[]).includes(cmd) &&
      !validateKSMArgs(cmd, args)
    ) {
      const error = new Error(`Arguments do not match the ${cmd} contract.`);
      error.name = 'INVALID_ARGS';
      throw error;
    }
    const extensionCommand = KSM.extensionCommands.get(cmd);
    if (!KNOWN_COMMANDS.has(cmd) && !extensionCommand) {
      const error = new Error(`Unknown Kasumi command: ${cmd}`);
      error.name = 'UNKNOWN_COMMAND';
      throw error;
    }
    if (extensionCommand && !matchesSchema(args, extensionCommand.args)) {
      const error = new Error(`Arguments do not match the ${cmd} extension contract.`);
      error.name = 'INVALID_ARGS';
      throw error;
    }
    assertMessagePayloadSize(args, 'args');
    return KSM.requireBus().invoke<TResult, TArgs>(cmd, args, timeoutMs);
  }

  public static async subscribe(event: string): Promise<void> {
    if (!KSM.isKnownEvent(event)) {
      throw new TypeError(`Unknown Kasumi event: ${event}`);
    }
    await KSM.requireBus().invoke('event.subscribe', { event });
    KSM.requireBus().trackSubscription(event);
  }

  public static async unsubscribe(event: string): Promise<void> {
    if (!KSM.isKnownEvent(event)) {
      throw new TypeError(`Unknown Kasumi event: ${event}`);
    }
    await KSM.requireBus().invoke('event.unsubscribe', { event });
    KSM.requireBus().untrackSubscription(event);
  }

  public static on<TPayload>(
    event: string,
    listener: (payload: TPayload) => void
  ): () => void {
    return KSM.events.on(event, listener);
  }

  public static configureExtensions(contract: KSMExtensionContract): void {
    const previous = KSM.extensionContracts.get(contract.id);
    const previousCommands = new Set(
      previous?.commands.map((command) => command.name) ?? []
    );
    const previousEvents = new Set(
      previous?.events.map((event) => event.name) ?? []
    );
    const validated = validateExtensionContract(contract, {
      ids: new Set(KSM.extensionContracts.keys()),
      commands: new Set(
        [...KSM.extensionCommands.keys()].filter(
          (name) => !previousCommands.has(name)
        )
      ),
      events: new Set(
        [...KSM.extensionEvents.keys()].filter(
          (name) => !previousEvents.has(name)
        )
      ),
      excludeId: contract.id
    });
    if (previous) {
      for (const command of previous.commands) {
        KSM.extensionCommands.delete(command.name);
      }
      for (const event of previous.events) {
        KSM.extensionEvents.delete(event.name);
      }
    }
    KSM.extensionContracts.set(validated.id, validated);
    for (const command of validated.commands) {
      KSM.extensionCommands.set(command.name, command);
    }
    for (const event of validated.events) {
      KSM.extensionEvents.set(event.name, event);
    }
  }

  public static async emit<TPayload extends KSMJsonValue>(
    event: string,
    payload: TPayload,
    windowId?: string
  ): Promise<KSMEmitResult> {
    if (!KSM.isKnownEvent(event)) {
      const error = new Error(`Unknown Kasumi event: ${event}`);
      error.name = 'UNKNOWN_COMMAND';
      throw error;
    }
    if (windowId !== undefined) {
      assertBoundedString(windowId, 'windowId', 128);
    }
    const args = {
      event,
      payload: extensionJsonValue(payload, 'payload'),
      ...(windowId === undefined ? {} : { windowId })
    };
    assertMessagePayloadSize(args, 'args');
    return await KSM.invoke<KSMEmitResult, typeof args>(
      'event.emit',
      args,
      EVENT_TIMEOUT_MS
    );
  }

  private static isKnownEvent(event: string): boolean {
    return KNOWN_EVENTS.has(event) || KSM.extensionEvents.has(event);
  }

  private static withExtensionCapabilities(
    capabilities: KSMCapabilities
  ): KSMCapabilities {
    return {
      ...capabilities,
      extensionIds: [...KSM.extensionContracts.keys()],
      extensionCommands: [...KSM.extensionCommands.keys()],
      extensionEvents: [...KSM.extensionEvents.keys()]
    };
  }

  private static requireBus(): KSMBus {
    if (!KSM.bus) {
      throw new Error('KSM must be initialized before use.');
    }
    return KSM.bus;
  }

  private static async recoverAfterReload(): Promise<void> {
    const bus = KSM.requireBus();
    const subscriptions = [
      ...bus.trackedSubscriptions(),
      ...[...KSM.eventListenerCounts.entries()]
        .filter(([event, count]) =>
          count > 0 &&
          bus.eventListenerCount(event) > 0 &&
          !bus.trackedSubscriptions().includes(event) &&
          KSM.isKnownEvent(event)
        )
        .map(([event]) => event)
    ];
    bus.discardPending('Kasumi WebView reloaded before the request completed.');
    KSM.cachedCapabilities = undefined;
    await KSM.capabilities();
    for (const event of subscriptions) {
      await bus.invoke('event.subscribe', { event });
      bus.trackSubscription(event);
    }
  }

  private static queueSubscriptionSync(event: string): void {
    const generation = KSM.bridgeGeneration;
    const previous = KSM.subscriptionSyncs.get(event) ?? Promise.resolve();
    const next = previous
      .catch(() => {})
      .then(async () => {
        if (generation !== KSM.bridgeGeneration) {
          return;
        }
        const bus = KSM.requireBus();
        const desired = KSM.eventListenerCounts.get(event) ?? 0;
        const tracked = bus.trackedSubscriptions().includes(event);
        if (desired > 0 && !tracked) {
          await bus.invoke('event.subscribe', { event });
          bus.trackSubscription(event);
        } else if (desired === 0 && tracked) {
          await bus.invoke('event.unsubscribe', { event });
          bus.untrackSubscription(event);
        }
      })
      .catch((error: unknown) => {
        console.error(`Kasumi could not synchronize ${event}.`, error);
      });
    KSM.subscriptionSyncs.set(event, next);
    void next.finally(() => {
      if (KSM.subscriptionSyncs.get(event) === next) {
        KSM.subscriptionSyncs.delete(event);
      }
    });
  }

  private static disposeBus(bus: KSMBus | undefined): void {
    if (!bus) {
      return;
    }
    if (KSM.bus === bus) {
      KSM.bus = undefined;
    }
    const subscriptions = bus.trackedSubscriptions();
    if (subscriptions.length === 0) {
      bus.dispose();
      return;
    }
    void Promise.allSettled(
      subscriptions.map((event) =>
        bus.invoke('event.unsubscribe', { event })
      )
    ).finally(() => {
      bus.dispose();
    });
  }

  private static releaseEventListenerCount(event: string): number {
    const count = KSM.eventListenerCounts.get(event) ?? 0;
    if (count <= 1) {
      KSM.eventListenerCounts.delete(event);
      return 0;
    }
    const next = count - 1;
    KSM.eventListenerCounts.set(event, next);
    return next;
  }
}
