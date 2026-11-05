import { KSM } from '../bridge/ksm.js';
import { assertBoolean, assertString, assertStringArray } from './guards.js';
import type {
  KSMDirectoryFilter,
  KSMOpenFilesArgs,
  KSMOpenFilesResult,
  KSMOpenFolderArgs,
  KSMOpenFolderResult
} from '../bridge/types.js';

function assertFilters(
  value: KSMDirectoryFilter | undefined,
  field: string
): void {
  if (value === undefined) {
    return;
  }
  if (typeof value !== 'object' || value === null || Array.isArray(value)) {
    throw new TypeError(`${field} must be an object.`);
  }
  if (value.extensions !== undefined) {
    assertStringArray(value.extensions, `${field}.extensions`, 128);
  }
  if (value.mimeTypes !== undefined) {
    assertStringArray(value.mimeTypes, `${field}.mimeTypes`, 128);
  }
}

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

export function openFolder(
  args: KSMOpenFolderArgs = {}
): Promise<KSMOpenFolderResult> {
  if (args.persist !== undefined) {
    assertBoolean(args.persist, 'persist');
  }
  assertFilters(args.filters, 'filters');
  return KSM.invoke('dialog.openFolder', args, 60000);
}

export function openFiles(
  args: KSMOpenFilesArgs = {}
): Promise<KSMOpenFilesResult> {
  if (args.filters !== undefined) {
    assertStringArray(args.filters, 'filters', 128);
  }
  return KSM.invoke('dialog.openFiles', args, 60000);
}
