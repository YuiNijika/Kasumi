import { KSM } from '../bridge/ksm.js';
import {
  assertBoundedString,
  assertLogLevel
} from './guards.js';

export function write(args: {
  level: 'trace' | 'debug' | 'info' | 'warn' | 'error';
  message: string;
  target?: string;
}): Promise<{ logged: true }> {
  assertLogLevel(args.level);
  assertBoundedString(args.message, 'message', 65536);
  if (args.target !== undefined) {
    assertBoundedString(args.target, 'target', 128);
  }
  return KSM.invoke('log.write', args);
}
