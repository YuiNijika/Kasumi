import { KSM } from '../bridge/ksm.js';
import {
  assertBoolean,
  assertBoundedString,
  assertHttpUrl,
  assertInteger,
  assertPositiveInteger,
  assertString
} from './guards.js';
import type {
  KSMJsonValue,
  KSMEmitResult,
  KSMWindowCreateOptions,
  KSMWindowFlagArgs,
  KSMWindowPositionArgs,
  KSMWindowResizeArgs,
  KSMWindowResizeDirection,
  KSMWindowSize,
  KSMWindowState,
  KSMWindowTargetArgs,
  KSMWindowStateChangedPayload,
  KSMWindowVisibilityChangedPayload
} from '../bridge/types.js';

function assertTarget(args: KSMWindowTargetArgs | undefined, field: string): void {
  if (args?.windowId !== undefined) {
    assertBoundedString(args.windowId, `${field}.windowId`, 128);
  }
}

const RESIZE_DIRECTIONS: readonly KSMWindowResizeDirection[] = [
  'top',
  'bottom',
  'left',
  'right',
  'topLeft',
  'topRight',
  'bottomLeft',
  'bottomRight'
];

function assertResizeDirection(
  value: unknown,
  field: string
): asserts value is KSMWindowResizeDirection {
  if (
    typeof value !== 'string' ||
    !RESIZE_DIRECTIONS.includes(value as KSMWindowResizeDirection)
  ) {
    throw new TypeError(`${field} must be a supported resize direction.`);
  }
}

export function setTitle(title: string): Promise<{ updated: boolean }> {
  assertString(title, 'title');
  return KSM.invoke('window.setTitle', { title });
}

export function minimize(): Promise<{ updated: boolean }> {
  return KSM.invoke('window.minimize', {});
}

export function maximize(): Promise<{ updated: boolean }> {
  return KSM.invoke('window.maximize', {});
}

export function setSize(args: KSMWindowSize): Promise<{ updated: boolean }> {
  assertPositiveInteger(args.width, 'width');
  assertPositiveInteger(args.height, 'height');
  return KSM.invoke('window.setSize', args);
}

export function startDragging(
  args: KSMWindowTargetArgs = {}
): Promise<{ dragging: boolean }> {
  assertTarget(args, 'args');
  return KSM.invoke('window.startDragging', args);
}

export function startResizing(
  args: KSMWindowResizeArgs
): Promise<{ resizing: boolean }> {
  assertResizeDirection(args.direction, 'direction');
  assertTarget(args, 'args');
  return KSM.invoke('window.startResizing', args);
}

export function close(): Promise<{ closed: boolean }> {
  return KSM.invoke('window.close', {});
}

export function create(args: KSMWindowCreateOptions): Promise<KSMWindowState> {
  assertBoundedString(args.url, 'url', 8192);
  if (args.url.startsWith('https://')) {
    assertHttpUrl(args.url, 'url', true);
  } else if (!args.url.startsWith('app://')) {
    throw new TypeError('url must use https:// or app://.');
  }
  if (args.title !== undefined) {
    assertBoundedString(args.title, 'title', 512, true);
  }
  if (args.width !== undefined) {
    assertPositiveInteger(args.width, 'width');
  }
  if (args.height !== undefined) {
    assertPositiveInteger(args.height, 'height');
  }
  if (args.x !== undefined) {
    assertInteger(args.x, 'x');
  }
  if (args.y !== undefined) {
    assertInteger(args.y, 'y');
  }
  for (const name of [
    'alwaysOnTop',
    'transparent',
    'resizable',
    'decorations'
  ] as const) {
    if (args[name] !== undefined) {
      assertBoolean(args[name], name);
    }
  }
  return KSM.invoke('window.create', args);
}

export function show(args: KSMWindowTargetArgs = {}): Promise<{ shown: boolean }> {
  assertTarget(args, 'args');
  return KSM.invoke('window.show', args);
}

export function hide(args: KSMWindowTargetArgs = {}): Promise<{ hidden: boolean }> {
  assertTarget(args, 'args');
  return KSM.invoke('window.hide', args);
}

export function setPosition(
  args: KSMWindowPositionArgs
): Promise<{ updated: boolean }> {
  assertTarget(args, 'args');
  assertInteger(args.x, 'x');
  assertInteger(args.y, 'y');
  return KSM.invoke('window.setPosition', args);
}

export function setAlwaysOnTop(
  args: KSMWindowFlagArgs
): Promise<{ updated: boolean }> {
  assertTarget(args, 'args');
  assertBoolean(args.enabled, 'enabled');
  return KSM.invoke('window.setAlwaysOnTop', args);
}

export function setTransparent(
  args: KSMWindowFlagArgs
): Promise<{ updated: boolean }> {
  assertTarget(args, 'args');
  assertBoolean(args.enabled, 'enabled');
  return KSM.invoke('window.setTransparent', args);
}

export function getState(
  args: KSMWindowTargetArgs = {}
): Promise<KSMWindowState> {
  assertTarget(args, 'args');
  return KSM.invoke('window.getState', args);
}

export function onVisibilityChanged(
  listener: (payload: KSMWindowVisibilityChangedPayload) => void
): () => void {
  return KSM.on('window.visibilityChanged', listener);
}

export function onStateChanged(
  listener: (payload: KSMWindowStateChangedPayload) => void
): () => void {
  return KSM.on('window.stateChanged', listener);
}

export function emit(
  event: string,
  payload: KSMJsonValue,
  windowId?: string
): Promise<KSMEmitResult> {
  assertBoundedString(event, 'event', 256);
  if (windowId !== undefined) {
    assertBoundedString(windowId, 'windowId', 128);
  }
  const args = {
    event,
    payload,
    ...(windowId === undefined ? {} : { windowId })
  };
  return KSM.emit(event, payload, windowId);
}
