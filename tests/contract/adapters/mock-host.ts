import {
  KSM_ANDROID_COMMANDS,
  KSM_COMMANDS,
  KSM_EVENTS,
  KSM_PERMISSIONS,
  KSM_PLUGINS,
  KSM_PLUGIN_METADATA,
  KSM_WINDOWS_COMMANDS,
  validateKSMArgs
} from '../../../frontend/src/generated/contract';
import type {
  KSMErrorCode,
  KSMPlatform,
  KSMRequest,
  KSMResponse,
  KSMEvent
} from '../../../frontend/src/bridge/types';
import {
  assertHttpUrl,
  assertVirtualDirectoryPath,
  assertVirtualPath,
  assertVirtualStatPath
} from '../../../frontend/src/api/guards';
import versionMetadata from '../../../version.json';

type IncomingMessage =
  | KSMRequest<unknown>
  | { type: 'cancel'; id: string };

type ResponseSender = (message: KSMResponse<unknown> | KSMEvent) => void;

const MAX_INLINE_TEXT_BYTES = 64 * 1024;
const KASUMI_VERSION = versionMetadata.version;

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

export class MockHost {
  private readonly directories = new Set<string>();
  private readonly files = new Map<string, string>([
    ['app://res/contract.txt', 'resource'],
    ['app://assets/index.html', '<!doctype html>'],
    ['app://data/large.txt', 'x'.repeat(MAX_INLINE_TEXT_BYTES + 1)]
  ]);
  private readonly store = new Map<string, unknown>();

  private nextTempId = 1;
  private capabilityQueries = 0;
  private subscribeCalls = 0;
  private unsubscribeCalls = 0;

  public constructor(private readonly platform: KSMPlatform) {}

  public handle(message: IncomingMessage, send: ResponseSender): void {
    if (message.type === 'cancel') {
      return;
    }
    const request = message;
    const id = request.id;
    const commands = this.platform === 'windows'
      ? KSM_WINDOWS_COMMANDS
      : KSM_ANDROID_COMMANDS;
    if (!(KSM_COMMANDS as readonly string[]).includes(request.cmd)) {
      send(this.error(id, 'UNKNOWN_COMMAND', 'The command is not registered.'));
      return;
    }
    if (!validateKSMArgs(request.cmd, request.args)) {
      send(this.error(id, 'INVALID_ARGS', 'Arguments do not match the contract.'));
      return;
    }

    if (request.cmd === 'ping') {
      send(this.ok(id, { pong: true }));
      return;
    }
    if (request.cmd === 'capabilities.query') {
      ++this.capabilityQueries;
      send(this.ok(id, {
        platform: this.platform,
        commands,
        permissions: KSM_PERMISSIONS,
        plugins: KSM_PLUGINS.map((id) => {
          const metadata = KSM_PLUGIN_METADATA[id];
          return {
            id,
            name: metadata?.name ?? id,
            version: metadata?.version ?? '1.0.0',
            enabled: true
          };
        })
      }));
      return;
    }
    if (request.cmd === 'app.getInfo') {
      send(this.ok(id, {
        name: 'Kasumi',
        version: KASUMI_VERSION,
        platform: this.platform,
        language: 'en-US'
      }));
      return;
    }
    if (request.cmd === 'permission.request') {
      const permission = this.stringArg(request.args, 'permission');
      if (!(KSM_PERMISSIONS as readonly string[]).includes(permission)) {
        send(this.error(id, 'INVALID_ARGS', 'permission is not registered.'));
        return;
      }
      send(this.ok(id, { granted: true }));
      return;
    }
    if (request.cmd === 'event.subscribe' || request.cmd === 'event.unsubscribe') {
      const event = this.stringArg(request.args, 'event');
      if (!(KSM_EVENTS as readonly string[]).includes(event)) {
        send(this.error(id, 'INVALID_ARGS', 'event is not registered.'));
        return;
      }
      if (request.cmd === 'event.subscribe') {
        ++this.subscribeCalls;
      } else {
        ++this.unsubscribeCalls;
      }
      send(this.ok(id, request.cmd === 'event.subscribe'
        ? { subscribed: true }
        : { unsubscribed: true }));
      return;
    }
    if (request.cmd === 'fs.readText') {
      const path = this.stringArg(request.args, 'path');
      if (!this.isVirtualPath(path, false)) {
        send(this.error(id, 'PERMISSION_DENIED', 'The path is outside the sandbox.'));
        return;
      }
      this.readText(id, path, send);
      return;
    }
    if (request.cmd === 'fs.writeText') {
      const path = this.stringArg(request.args, 'path');
      if (!this.isVirtualPath(path, true)) {
        send(this.error(id, 'PERMISSION_DENIED', 'The path is outside the sandbox.'));
        return;
      }
      this.files.set(path, this.stringArg(request.args, 'content'));
      send(this.ok(id, { written: true }));
      return;
    }
    if (request.cmd === 'fs.exists') {
      const path = this.stringArg(request.args, 'path');
      if (!this.isVirtualPath(path, false)) {
        send(this.error(id, 'PERMISSION_DENIED', 'The path is outside the sandbox.'));
        return;
      }
      send(this.ok(id, { exists: this.files.has(path) }));
      return;
    }
    if (request.cmd === 'fs.remove') {
      const path = this.stringArg(request.args, 'path');
      if (!this.isVirtualPath(path, true)) {
        send(this.error(id, 'PERMISSION_DENIED', 'The path is outside the sandbox.'));
        return;
      }
      send(this.ok(id, { removed: this.files.delete(path) }));
      return;
    }
    if (request.cmd === 'fs.readDir') {
      const path = this.stringArg(request.args, 'path');
      if (!this.isVirtualDirectoryPath(path, false)) {
        send(this.error(id, 'PERMISSION_DENIED', 'The path is not a directory.'));
        return;
      }
      const normalized = this.normalizeDirectory(path);
      if (!this.directoryExists(normalized)) {
        send(this.error(id, 'NOT_FOUND', 'The directory was not found.'));
        return;
      }
      const names = new Set<string>();
      for (const file of this.files.keys()) {
        const child = this.directChildName(normalized, file);
        if (child !== undefined) {
          names.add(child);
        }
      }
      for (const directory of this.directories) {
        const child = this.directChildName(normalized, directory);
        if (child !== undefined) {
          names.add(child);
        }
      }
      const entries = [...names].sort().map((name) => {
        const childPath = `${normalized}/${name}`;
        const isDirectory = this.directoryExists(childPath);
        return {
          name,
          path: childPath,
          kind: isDirectory ? 'directory' : 'file',
          size: isDirectory ? 0 : new TextEncoder().encode(
            this.files.get(childPath) ?? ''
          ).byteLength,
          modifiedAt: null
        };
      });
      send(this.ok(id, { entries }));
      return;
    }
    if (request.cmd === 'fs.mkdir') {
      const path = this.stringArg(request.args, 'path');
      if (!this.isVirtualDirectoryPath(path, true)) {
        send(this.error(id, 'PERMISSION_DENIED', 'The path is not writable.'));
        return;
      }
      const recursive = request.args !== null &&
        typeof request.args === 'object' &&
        !Array.isArray(request.args) &&
        (request.args as Record<string, unknown>).recursive === true;
      const normalized = this.normalizeDirectory(path);
      if (this.directoryExists(normalized)) {
        send(this.ok(id, { created: false }));
        return;
      }
      const parent = normalized.slice(0, normalized.lastIndexOf('/'));
      if (!recursive && !this.directoryExists(parent)) {
        send(this.error(id, 'IO_ERROR', 'The directory could not be created.'));
        return;
      }
      this.directories.add(normalized);
      send(this.ok(id, { created: true }));
      return;
    }
    if (request.cmd === 'fs.stat') {
      const path = this.stringArg(request.args, 'path');
      if (!this.isVirtualStatPath(path)) {
        send(this.error(id, 'PERMISSION_DENIED', 'The path is outside the sandbox.'));
        return;
      }
      const normalized = this.normalizeDirectory(path);
      if (this.files.has(normalized)) {
        send(this.ok(id, {
          kind: 'file',
          size: new TextEncoder().encode(this.files.get(normalized)).byteLength,
          modifiedAt: null
        }));
        return;
      }
      if (this.directoryExists(normalized)) {
        send(this.ok(id, { kind: 'directory', size: 0, modifiedAt: null }));
        return;
      }
      send(this.error(id, 'NOT_FOUND', 'The path was not found.'));
      return;
    }
    if (request.cmd === 'dialog.message' || request.cmd === 'dialog.confirm') {
      send(this.ok(id, { confirmed: true }));
      return;
    }
    if (request.cmd === 'dialog.openFile') {
      send(this.ok(id, { file: 'app://picked/1' }));
      return;
    }
    if (request.cmd === 'dialog.saveFile') {
      send(this.ok(id, { file: 'app://picked/2' }));
      return;
    }
    if (request.cmd === 'clipboard.readText') {
      send(this.ok(id, { text: '' }));
      return;
    }
    if (request.cmd === 'clipboard.writeText') {
      send(this.ok(id, { written: true }));
      return;
    }
    if (request.cmd === 'notification.show') {
      send(this.ok(id, { shown: true }));
      return;
    }
    if (request.cmd === 'http.download') {
      const url = this.stringArg(request.args, 'url');
      const path = this.stringArg(request.args, 'path');
      try {
        assertHttpUrl(url, 'url', true);
        assertVirtualPath(path, true, false);
      } catch {
        send(this.error(id, 'PERMISSION_DENIED', 'The download target is invalid.'));
        return;
      }
      const overwrite = isRecord(request.args) && request.args.overwrite === true;
      if (this.files.has(path) && !overwrite) {
        send(this.error(id, 'IO_ERROR', 'The target already exists.'));
        return;
      }
      const content = 'download';
      this.files.set(path, content);
      send(this.ok(id, {
        path,
        bytes: new TextEncoder().encode(content).byteLength
      }));
      return;
    }
    if (request.cmd === 'shell.openExternal') {
      const url = this.stringArg(request.args, 'url');
      let origin: string;
      try {
        assertHttpUrl(url, 'url');
        const parsed = new URL(url);
        origin = `${parsed.protocol}//${parsed.host}`;
      } catch {
        send(this.error(id, 'INVALID_ARGS', 'The external URL is invalid.'));
        return;
      }
      if (origin !== 'https://example.com') {
        send(this.error(id, 'PERMISSION_DENIED', 'The URL origin is not allowlisted.'));
        return;
      }
      send(this.ok(id, { opened: true }));
      return;
    }
    if (request.cmd === 'path.dirs') {
      send(this.ok(id, {
        data: 'app://data',
        cache: 'app://cache',
        temp: 'app://tmp',
        resource: 'app://res',
        asset: 'app://assets'
      }));
      return;
    }
    if (request.cmd === 'path.join') {
      const base = this.stringArg(request.args, 'base');
      const segments = isRecord(request.args) && Array.isArray(request.args.segments)
        ? request.args.segments
        : [];
      const path = [base, ...segments].join('/').replace(/\/+$/, '');
      send(this.ok(id, { path }));
      return;
    }
    if (request.cmd === 'path.normalize') {
      send(this.ok(id, {
        path: this.stringArg(request.args, 'path').replace(/\/+$/, '')
      }));
      return;
    }
    if (request.cmd === 'path.dirname') {
      const path = this.stringArg(request.args, 'path').replace(/\/+$/, '');
      const separator = path.lastIndexOf('/');
      const root = path.match(/^app:\/\/(?:data|cache|tmp|picked|res|assets)/)?.[0] ?? path;
      send(this.ok(id, {
        path: separator <= 'app://'.length + 1
          ? root
          : path.substring(0, separator)
      }));
      return;
    }
    if (request.cmd === 'path.basename') {
      const path = this.stringArg(request.args, 'path').replace(/\/+$/, '');
      const separator = path.lastIndexOf('/');
      let name = separator <= 'app://'.length + 1
        ? ''
        : path.substring(separator + 1);
      if (
        isRecord(request.args) &&
        typeof request.args.suffix === 'string' &&
        name.length > request.args.suffix.length &&
        name.endsWith(request.args.suffix)
      ) {
        name = name.slice(0, -request.args.suffix.length);
      }
      send(this.ok(id, { name }));
      return;
    }
    if (request.cmd === 'path.extname') {
      const path = this.stringArg(request.args, 'path').replace(/\/+$/, '');
      const separator = path.lastIndexOf('/');
      const dot = path.lastIndexOf('.');
      const nameStart = separator === -1 ? 'app://'.length : separator + 1;
      send(this.ok(id, {
        extension: dot <= nameStart || (separator !== -1 && dot < separator)
          ? ''
          : path.substring(dot)
      }));
      return;
    }
    if (request.cmd === 'path.isAbsolute') {
      const value = this.stringArg(request.args, 'path');
      send(this.ok(id, { absolute: /^app:\/\/(data|cache|tmp|picked|res|assets)(?:\/|$)/.test(value) }));
      return;
    }
    if (request.cmd === 'store.get') {
      const key = this.stringArg(request.args, 'key');
      send(this.ok(id, this.store.has(key)
        ? { found: true, value: this.store.get(key) }
        : { found: false, value: null }));
      return;
    }
    if (request.cmd === 'store.set') {
      if (!isRecord(request.args) || !('value' in request.args)) {
        throw new TypeError('value is required.');
      }
      this.store.set(this.stringArg(request.args, 'key'), request.args.value);
      send(this.ok(id, { updated: true }));
      return;
    }
    if (request.cmd === 'store.delete') {
      const key = this.stringArg(request.args, 'key');
      send(this.ok(id, { deleted: this.store.delete(key) }));
      return;
    }
    if (request.cmd === 'store.clear') {
      this.store.clear();
      send(this.ok(id, { cleared: true }));
      return;
    }
    if (request.cmd === 'store.keys') {
      send(this.ok(id, { keys: [...this.store.keys()].sort() }));
      return;
    }
    if (request.cmd === 'os.getInfo') {
      send(this.ok(id, {
        platform: this.platform,
        architecture: this.platform === 'windows' ? 'x86_64' : 'arm64-v8a',
        version: this.platform === 'windows' ? '10.0.22631' : '14',
        locale: 'en-US',
        timezone: 'Asia/Shanghai',
        appVersion: KASUMI_VERSION
      }));
      return;
    }
    if (request.cmd === 'log.write') {
      this.stringArg(request.args, 'level');
      this.stringArg(request.args, 'message');
      send(this.ok(id, { logged: true }));
      return;
    }
    if (request.cmd === 'sql.drivers') {
      send(this.ok(id, { drivers: ['sqlite'] }));
      return;
    }
    if (request.cmd === 'sql.open') {
      const driver = this.stringArg(request.args, 'driver');
      const url = this.stringArg(request.args, 'url');
      if (driver !== 'sqlite' || url.length === 0) {
        send(this.error(id, 'NOT_SUPPORTED', 'The SQL driver is unavailable.'));
        return;
      }
      send(this.ok(id, { connectionId: 'sql-mock-1' }));
      return;
    }
    if (request.cmd === 'sql.query') {
      const connectionId = this.stringArg(request.args, 'connectionId');
      this.stringArg(request.args, 'statement');
      if (connectionId !== 'sql-mock-1') {
        send(this.error(id, 'NOT_FOUND', 'The SQL connection does not exist.'));
        return;
      }
      send(this.ok(id, {
        columns: ['id', 'name'],
        rows: [{ id: 1, name: 'contract' }]
      }));
      return;
    }
    if (request.cmd === 'sql.execute') {
      const connectionId = this.stringArg(request.args, 'connectionId');
      const statement = this.stringArg(request.args, 'statement');
      if (connectionId !== 'sql-mock-1') {
        send(this.error(id, 'NOT_FOUND', 'The SQL connection does not exist.'));
        return;
      }
      const isDdl = /^\s*(create|alter|drop|truncate)\b/i.test(statement);
      send(this.ok(id, {
        rowsAffected: isDdl ? 0 : 1,
        lastInsertId: isDdl ? null : 1
      }));
      return;
    }
    if (request.cmd === 'sql.close') {
      const connectionId = this.stringArg(request.args, 'connectionId');
      send(this.ok(id, { closed: connectionId === 'sql-mock-1' }));
      return;
    }
    if (request.cmd === 'device.getInfo') {
      if (this.platform !== 'android') {
        send(this.error(id, 'NOT_SUPPORTED', 'The command is not supported.'));
        return;
      }
      send(this.ok(id, {
        platform: 'android',
        osVersion: '14',
        model: 'Pixel',
        manufacturer: 'Google',
        appVersion: KASUMI_VERSION,
        language: 'en-US',
        screenWidth: 1080,
        screenHeight: 2400,
        density: 2.75,
        deviceName: 'Contract',
        timezone: 'Asia/Shanghai',
        architecture: 'arm64-v8a'
      }));
      return;
    }
    if (request.cmd === 'window.setTitle') {
      if (this.platform !== 'windows') {
        send(this.error(id, 'NOT_SUPPORTED', 'The command is not supported.'));
        return;
      }
      send(this.ok(id, { updated: true }));
      return;
    }
    if (request.cmd === 'window.minimize' || request.cmd === 'window.maximize') {
      if (this.platform !== 'windows') {
        send(this.error(id, 'NOT_SUPPORTED', 'The command is not supported.'));
        return;
      }
      send(this.ok(id, { updated: true }));
      return;
    }
    if (request.cmd === 'window.setSize') {
      if (this.platform !== 'windows') {
        send(this.error(id, 'NOT_SUPPORTED', 'The command is not supported.'));
        return;
      }
      const width = this.numberArg(request.args, 'width');
      const height = this.numberArg(request.args, 'height');
      if (
        !Number.isSafeInteger(width) ||
        !Number.isSafeInteger(height) ||
        width <= 0 ||
        height <= 0
      ) {
        send(this.error(id, 'INVALID_ARGS', 'width and height must be positive integers.'));
        return;
      }
      send(this.ok(id, { updated: true }));
      return;
    }
    if (request.cmd === 'window.close') {
      send(this.ok(id, { closed: true }));
      return;
    }
    if (request.cmd === 'app.exit') {
      send(this.ok(id, { exited: true }));
      return;
    }
    send(this.error(id, 'NOT_SUPPORTED', 'The command is not implemented.'));
  }

  public counters(): {
    capabilityQueries: number;
    subscribeCalls: number;
    unsubscribeCalls: number;
  } {
    return {
      capabilityQueries: this.capabilityQueries,
      subscribeCalls: this.subscribeCalls,
      unsubscribeCalls: this.unsubscribeCalls
    };
  }

  public resetCounters(): void {
    this.capabilityQueries = 0;
    this.subscribeCalls = 0;
    this.unsubscribeCalls = 0;
  }

  private readText(id: string, path: string, send: ResponseSender): void {
    const value = this.files.get(path);
    if (value === undefined) {
      send(this.error(id, 'NOT_FOUND', 'The file was not found.'));
      return;
    }
    if (value.length > MAX_INLINE_TEXT_BYTES) {
      const file = path.startsWith('app://tmp/read-')
        ? path
        : `app://tmp/read-${this.nextTempId++}.txt`;
      if (!this.files.has(file)) {
        this.files.set(file, value);
      }
      send(this.ok(id, { file }));
      return;
    }
    send(this.ok(id, value));
  }

  private isVirtualPath(path: string, writable: boolean): boolean {
    try {
      assertVirtualPath(path, writable);
      return true;
    } catch {
      return false;
    }
  }

  private isVirtualDirectoryPath(path: string, writable: boolean): boolean {
    try {
      assertVirtualDirectoryPath(path, writable);
      return true;
    } catch {
      return false;
    }
  }

  private isVirtualStatPath(path: string): boolean {
    try {
      assertVirtualStatPath(path);
      return true;
    } catch {
      return false;
    }
  }

  private normalizeDirectory(path: string): string {
    return path.replace(/\/+$/, '');
  }

  private directoryExists(path: string): boolean {
    if (this.directories.has(path)) {
      return true;
    }
    if (/^app:\/\/(data|cache|tmp|picked|res|assets)$/.test(path)) {
      return true;
    }
    return [...this.files.keys(), ...this.directories].some((candidate) =>
      candidate.startsWith(`${path}/`)
    );
  }

  private directChildName(parent: string, candidate: string): string | undefined {
    if (!candidate.startsWith(`${parent}/`)) {
      return undefined;
    }
    const relative = candidate.slice(parent.length + 1);
    if (relative.length === 0 || relative.includes('/')) {
      return undefined;
    }
    return relative;
  }

  private stringArg(args: unknown, field: string): string {
    if (!isRecord(args) || typeof args[field] !== 'string') {
      throw new TypeError(`${field} must be a string.`);
    }
    return args[field];
  }

  private numberArg(args: unknown, field: string): number {
    if (!isRecord(args) || typeof args[field] !== 'number') {
      throw new TypeError(`${field} must be a number.`);
    }
    return args[field];
  }

  private ok(id: string, data: unknown): KSMResponse<unknown> {
    return {
      type: 'response',
      id,
      ok: true,
      data
    };
  }

  private error(id: string, code: string, message: string): KSMResponse<unknown> {
    return {
      type: 'response',
      id,
      ok: false,
      error: {
        code: code as KSMErrorCode,
        message
      }
    };
  }
}

export class MockWebView {
  private readonly listeners = new Set<(event: MessageEvent<string>) => void>();

  public constructor(private readonly host: MockHost) {}

  public postMessage(data: string): void {
    const message = JSON.parse(data) as IncomingMessage;
    this.host.handle(message, (response) => {
      const event = { data: JSON.stringify(response) } as MessageEvent<string>;
      for (const listener of this.listeners) {
        listener(event);
      }
    });
  }

  public addEventListener(
    _type: 'message',
    listener: (event: MessageEvent<string>) => void
  ): void {
    this.listeners.add(listener);
  }

  public removeEventListener(
    _type: 'message',
    listener: (event: MessageEvent<string>) => void
  ): void {
    this.listeners.delete(listener);
  }

  public emit(message: KSMResponse<unknown> | KSMEvent): void {
    const event = { data: JSON.stringify(message) } as MessageEvent<string>;
    for (const listener of this.listeners) {
      listener(event);
    }
  }
}
