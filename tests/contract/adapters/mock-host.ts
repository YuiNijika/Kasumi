import {
  KSM_ANDROID_COMMANDS,
  KSM_COMMANDS,
  KSM_EVENTS,
  KSM_PERMISSIONS,
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
  assertVirtualDirectoryPath,
  assertVirtualPath,
  assertVirtualStatPath
} from '../../../frontend/src/api/guards';

type IncomingMessage =
  | KSMRequest<unknown>
  | { type: 'cancel'; id: string };

type ResponseSender = (message: KSMResponse<unknown> | KSMEvent) => void;

const MAX_INLINE_TEXT_BYTES = 64 * 1024;

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
        permissions: KSM_PERMISSIONS
      }));
      return;
    }
    if (request.cmd === 'app.getInfo') {
      send(this.ok(id, {
        name: 'Kasumi',
        version: '0.1.0',
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
