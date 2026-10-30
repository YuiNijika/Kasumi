import { KSM } from '../bridge/ksm';
import {
  assertBoolean,
  assertString,
  assertVirtualDirectoryPath,
  assertVirtualPath,
  assertVirtualStatPath
} from './guards';
import type {
  KSMDirectoryEntry,
  KSMFileMetadata
} from '../bridge/types';

export function readText(args: { path: string }): Promise<string | { file: string }> {
  assertVirtualPath(args.path);
  return KSM.invoke('fs.readText', args);
}

export function writeText(args: { path: string; content: string }): Promise<{ written: boolean }> {
  assertVirtualPath(args.path, true);
  assertString(args.content, 'content');
  return KSM.invoke('fs.writeText', args);
}

export function exists(args: { path: string }): Promise<{ exists: boolean }> {
  assertVirtualPath(args.path);
  return KSM.invoke('fs.exists', args);
}

export function remove(args: { path: string }): Promise<{ removed: boolean }> {
  assertVirtualPath(args.path, true);
  return KSM.invoke('fs.remove', args);
}

export function readDir(
  args: { path: string }
): Promise<{ entries: KSMDirectoryEntry[] }> {
  assertVirtualDirectoryPath(args.path);
  return KSM.invoke('fs.readDir', args);
}

export function mkdir(
  args: { path: string; recursive?: boolean }
): Promise<{ created: boolean }> {
  assertVirtualDirectoryPath(args.path, true);
  if (args.recursive !== undefined) {
    assertBoolean(args.recursive, 'recursive');
  }
  return KSM.invoke('fs.mkdir', args);
}

export function stat(args: { path: string }): Promise<KSMFileMetadata> {
  assertVirtualStatPath(args.path);
  return KSM.invoke('fs.stat', args);
}
