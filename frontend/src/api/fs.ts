import { KSM } from '../bridge/ksm.js';
import {
  assertBoolean,
  assertInteger,
  assertScopeId,
  assertString,
  assertStringArray,
  assertVirtualDirectoryPath,
  assertVirtualPath,
  assertVirtualStatPath
} from './guards.js';
import type {
  KSMDirectoryEntry,
  KSMDirectoryFilter,
  KSMFileMetadata,
  KSMFsWalkArgs,
  KSMFsWalkResult,
  KSMScopeReleaseArgs,
  KSMScopeReleaseResult
} from '../bridge/types.js';

const MAX_WALK_DEPTH = 64;

function assertDirectoryFilter(
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

export function readText(args: { path: string }): Promise<string | { file: string }> {
  assertVirtualPath(args.path);
  return KSM.invoke('fs.readText', args);
}

export function appendText(args: {
  path: string;
  content: string;
}): Promise<{ written: boolean }> {
  assertVirtualPath(args.path, true);
  assertString(args.content, 'content');
  return KSM.invoke('fs.appendText', args);
}

export function copy(args: {
  from: string;
  to: string;
}): Promise<{ copied: boolean }> {
  assertVirtualPath(args.from);
  assertVirtualPath(args.to, true);
  return KSM.invoke('fs.copy', args);
}

export function move(args: {
  from: string;
  to: string;
}): Promise<{ moved: boolean }> {
  assertVirtualPath(args.from, true);
  assertVirtualPath(args.to, true);
  return KSM.invoke('fs.move', args);
}

export function readBytes(args: {
  path: string;
}): Promise<{ base64: string }> {
  assertVirtualPath(args.path);
  return KSM.invoke('fs.readBytes', args);
}

export function writeBytes(args: {
  path: string;
  base64: string;
}): Promise<{ written: boolean }> {
  assertVirtualPath(args.path, true);
  assertString(args.base64, 'base64');
  return KSM.invoke('fs.writeBytes', args);
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

export function walk(args: KSMFsWalkArgs): Promise<KSMFsWalkResult> {
  assertVirtualDirectoryPath(args.path);
  if (args.maxDepth !== undefined) {
    assertInteger(args.maxDepth, 'maxDepth', { min: 0, max: MAX_WALK_DEPTH });
  }
  assertDirectoryFilter(args.filter, 'filter');
  return KSM.invoke('fs.walk', args, 60000);
}

export function releaseScope(
  args: KSMScopeReleaseArgs
): Promise<KSMScopeReleaseResult> {
  assertScopeId(args.scopeId, 'scopeId');
  return KSM.invoke('fs.scope.release', args);
}
