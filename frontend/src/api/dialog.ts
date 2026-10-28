import { KSM } from '../bridge/ksm';
import { assertString } from './guards';

export function message(args: { title: string; message: string }): Promise<{ confirmed: boolean }> {
  assertString(args.title, 'title');
  assertString(args.message, 'message');
  return KSM.invoke('dialog.message', args);
}

export function confirm(args: { title: string; message: string }): Promise<{ confirmed: boolean }> {
  assertString(args.title, 'title');
  assertString(args.message, 'message');
  return KSM.invoke('dialog.confirm', args);
}

export function openFile(args: { filters?: string[] } = {}): Promise<{ file: string | null }> {
  if (args.filters !== undefined) {
    for (const filter of args.filters) {
      assertString(filter, 'filters');
    }
  }
  return KSM.invoke('dialog.openFile', args);
}

export function saveFile(args: { suggestedName?: string } = {}): Promise<{ file: string | null }> {
  if (args.suggestedName !== undefined) {
    assertString(args.suggestedName, 'suggestedName');
  }
  return KSM.invoke('dialog.saveFile', args);
}
