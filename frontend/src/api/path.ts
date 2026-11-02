import { KSM } from '../bridge/ksm.js';
import { assertBoundedString, assertString } from './guards.js';
import type { KSMPathDirectories } from '../bridge/types.js';

export function dirs(): Promise<KSMPathDirectories> {
  return KSM.invoke('path.dirs', {});
}

export function join(args: {
  base: string;
  segments: string[];
}): Promise<{ path: string }> {
  assertBoundedString(args.base, 'base', 8192);
  if (!Array.isArray(args.segments)) {
    throw new TypeError('segments must be an array.');
  }
  args.segments.forEach((segment, index) => {
    assertBoundedString(segment, `segments[${index}]`, 256);
  });
  return KSM.invoke('path.join', args);
}

export function normalize(args: { path: string }): Promise<{ path: string }> {
  assertBoundedString(args.path, 'path', 8192);
  return KSM.invoke('path.normalize', args);
}

export function dirname(args: { path: string }): Promise<{ path: string }> {
  assertBoundedString(args.path, 'path', 8192);
  return KSM.invoke('path.dirname', args);
}

export function basename(args: {
  path: string;
  suffix?: string;
}): Promise<{ name: string }> {
  assertBoundedString(args.path, 'path', 8192);
  if (args.suffix !== undefined) {
    assertBoundedString(args.suffix, 'suffix', 256, true);
  }
  return KSM.invoke('path.basename', args);
}

export function extname(args: { path: string }): Promise<{ extension: string }> {
  assertBoundedString(args.path, 'path', 8192);
  return KSM.invoke('path.extname', args);
}

export function isAbsolute(args: { path: string }): Promise<{ absolute: boolean }> {
  assertString(args.path, 'path');
  if (args.path.length === 0) {
    throw new TypeError('path must not be empty.');
  }
  return KSM.invoke('path.isAbsolute', args);
}
