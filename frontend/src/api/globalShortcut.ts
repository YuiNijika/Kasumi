import { KSM } from '../bridge/ksm.js';
import { assertBoundedString } from './guards.js';

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
