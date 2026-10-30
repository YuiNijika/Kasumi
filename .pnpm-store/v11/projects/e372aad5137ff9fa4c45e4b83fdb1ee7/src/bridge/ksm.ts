import { KSMBus } from './bus';
import { KSMWebMessageTransport } from './transport';
import {
  KSM_COMMANDS,
  KSM_EVENTS,
  validateKSMArgs,
  type KSMCommandName
} from '../generated/contract';
import {
  assertBoolean,
  assertString,
  assertVirtualDirectoryPath,
  assertVirtualPath,
  assertVirtualStatPath
} from '../api/guards';
import type {
  KSMAppInfo,
  KSMAppApi,
  KSMClipboardApi,
  KSMDirectoryEntry,
  KSMCapabilities,
  KSMDialogApi,
  KSMFileMetadata,
  KSMFsApi,
  KSMNotificationApi,
  KSMPermissionApi,
  KSMTransport,
  KSMWindowApi
} from './types';

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
  public static readonly window: KSMWindowApi = {
    setTitle: (title): Promise<{ updated: boolean }> => KSM.invoke('window.setTitle', { title }),
    minimize: (): Promise<{ updated: boolean }> => KSM.invoke('window.minimize', {}),
    maximize: (): Promise<{ updated: boolean }> => KSM.invoke('window.maximize', {}),
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

  public static async invoke<TResult, TArgs>(cmd: string, args: TArgs): Promise<TResult> {
    if (
      (KSM_COMMANDS as readonly string[]).includes(cmd) &&
      !validateKSMArgs(cmd, args)
    ) {
      const error = new Error(`Arguments do not match the ${cmd} contract.`);
      error.name = 'INVALID_ARGS';
      throw error;
    }
    return KSM.requireBus().invoke<TResult, TArgs>(cmd, args);
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
