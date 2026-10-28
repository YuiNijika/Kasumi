import { KSM } from '../bridge/ksm';
import { assertString } from './guards';

export function readText(): Promise<{ text: string }> {
  return KSM.invoke('clipboard.readText', {});
}

export function writeText(text: string): Promise<{ written: boolean }> {
  assertString(text, 'text');
  return KSM.invoke('clipboard.writeText', { text });
}
