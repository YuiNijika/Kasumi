import { KSM } from '../bridge/ksm.js';
import { assertBoundedString } from './guards.js';
import type { KSMGlobalShortcutTriggeredPayload } from '../bridge/types.js';

export function register(args: {
  shortcut: string;
}): Promise<{ registered: boolean }> {
  assertBoundedString(args.shortcut, 'shortcut', 256);
  return KSM.invoke('globalShortcut.register', args);
}

export function unregister(args: {
  shortcut: string;
}): Promise<{ unregistered: boolean }> {
  assertBoundedString(args.shortcut, 'shortcut', 256);
  return KSM.invoke('globalShortcut.unregister', args);
}

export function onTriggered(
  listener: (payload: KSMGlobalShortcutTriggeredPayload) => void
): () => void {
  return KSM.on('globalShortcut.triggered', listener);
}
