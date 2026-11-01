import { KSM } from '../bridge/ksm.js';
import { assertString } from './guards.js';

export function show(args: { title: string; body: string }): Promise<{ shown: boolean }> {
  assertString(args.title, 'title');
  assertString(args.body, 'body');
  return KSM.invoke('notification.show', args);
}
