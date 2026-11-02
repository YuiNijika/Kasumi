import { KSMBus, MAX_REQUEST_TIMEOUT_MS } from './bus.js';
import { KSMWebMessageTransport } from './transport.js';
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
  KSMAppInfo,
  KSMAppApi,
  KSMClipboardApi,
  KSMDeviceApi,
  KSMDeviceInfo,
  KSMDirectoryEntry,
  KSMCapabilities,
  KSMDialogApi,
  KSMFileMetadata,
  KSMFsApi,
  KSMHttpApi,
  KSMHttpDownloadResult,
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
  KSMTransport,
  KSMWindowApi,
  KSMWindowSize
} from './types.js';

export type KSMTextResult = string | { file: string };

export interface KSMEventApi {
  on<TPayload>(event: string, listener: (payload: TPayload) => void): () => void;
}

export class KSM {
  private static bus: KSMBus | undefined;
  private static cachedCapabilities: KSMCapabilities | undefined;
  private static capabilitiesPromise: Promise<KSMCapabilities> | undefined;
  private static readonly eventListenerCounts = new Map<string, number>();
  public static readonly app: KSMAppApi = {
    getInfo: (): Promise<KSMAppInfo> => KSM.getInfo(),
    exit: (): Promise<{ exited: boolean }> => KSM.invoke('app.exit', {})
  };
  public static readonly fs: KSMFsApi = {
    readText: (args): Promise<string | { file: string }> => {
      assertVirtualPath(args.path);
      return KSM.invoke('fs.readText', args);
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
    }
  };
  public static readonly dialog: KSMDialogApi = {
    message: (args): Promise<{ confirmed: boolean }> => KSM.invoke('dialog.message', args),
    confirm: (args): Promise<{ confirmed: boolean }> => KSM.invoke('dialog.confirm', args),
    openFile: (args = {}): Promise<{ file: string | null }> => KSM.invoke('dialog.openFile', args),
    saveFile: (args = {}): Promise<{ file: string | null }> => KSM.invoke('dialog.saveFile', args)
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
    }
  };
  public static readonly shell: KSMShellApi = {
    openExternal: (args): Promise<{ opened: boolean }> => {
      assertHttpUrl(args.url, 'url');
      return KSM.invoke('shell.openExternal', args);
    }
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
    close: (): Promise<{ closed: boolean }> => KSM.invoke('window.close', {})
  };
  public static readonly events: KSMEventApi = {
    on: <TPayload>(event: string, listener: (payload: TPayload) => void): (() => void) => {
      if (!(KSM_EVENTS as readonly string[]).includes(event)) {
        throw new TypeError(`Unknown Kasumi event: ${event}`);
      }
      const bus = KSM.requireBus();
      const unsubscribe = bus.on(event, listener);
      const count = (KSM.eventListenerCounts.get(event) ?? 0) + 1;
      KSM.eventListenerCounts.set(event, count);
      if (count === 1) {
        void KSM.subscribe(event).catch(() => {
          unsubscribe();
          KSM.releaseEventListenerCount(event);
        });
      }
      return (): void => {
        unsubscribe();
        const next = KSM.releaseEventListenerCount(event);
        if (next === 0) {
          void KSM.unsubscribe(event).catch(() => {});
        }
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
    KSM.bus?.dispose();
    KSM.eventListenerCounts.clear();
    KSM.cachedCapabilities = undefined;
    KSM.capabilitiesPromise = undefined;
    KSM.bus = new KSMBus(resolvedTransport);
    KSM.bus.on('webview.reloaded', () => {
      void KSM.recoverAfterReload();
    });
  }

  public static dispose(): void {
    KSM.bus?.dispose();
    KSM.bus = undefined;
    KSM.cachedCapabilities = undefined;
    KSM.capabilitiesPromise = undefined;
    KSM.eventListenerCounts.clear();
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
      return KSM.cachedCapabilities;
    }
    if (!KSM.capabilitiesPromise) {
      KSM.capabilitiesPromise = KSM.requireBus()
        .invoke<KSMCapabilities, Record<string, never>>(
          'capabilities.query',
          {}
        )
        .then((capabilities) => {
          KSM.cachedCapabilities = capabilities;
          return capabilities;
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
    return KSM.requireBus().invoke<TResult, TArgs>(cmd, args, timeoutMs);
  }

  public static async subscribe(event: string): Promise<void> {
    if (!(KSM_EVENTS as readonly string[]).includes(event)) {
      throw new TypeError(`Unknown Kasumi event: ${event}`);
    }
    await KSM.requireBus().invoke('event.subscribe', { event });
    KSM.requireBus().trackSubscription(event);
  }

  public static async unsubscribe(event: string): Promise<void> {
    if (!(KSM_EVENTS as readonly string[]).includes(event)) {
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

  private static requireBus(): KSMBus {
    if (!KSM.bus) {
      throw new Error('KSM must be initialized before use.');
    }
    return KSM.bus;
  }

  private static async recoverAfterReload(): Promise<void> {
    const bus = KSM.requireBus();
    const subscriptions = bus.trackedSubscriptions();
    bus.discardPending('Kasumi WebView reloaded before the request completed.');
    KSM.cachedCapabilities = undefined;
    await KSM.capabilities();
    for (const event of subscriptions) {
      await bus.invoke('event.subscribe', { event });
    }
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
