import { KSM } from '../bridge/ksm.js';
import { assertString } from './guards.js';

export function readText(): Promise<{ text: string }> {
  return KSM.invoke('clipboard.readText', {});
}

export function writeText(text: string): Promise<{ written: boolean }> {
  assertString(text, 'text');
  return KSM.invoke('clipboard.writeText', { text });
}
