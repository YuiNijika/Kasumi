import { KSM } from '../bridge/ksm';
import { assertVirtualPath, assertString } from './guards';

export function readText(args: { path: string }): Promise<string | { file: string }> {
  assertVirtualPath(args.path);
  return KSM.invoke('fs.readText', args);
}

export function writeText(args: { path: string; content: string }): Promise<{ written: boolean }> {
  assertVirtualPath(args.path);
  assertString(args.content, 'content');
  return KSM.invoke('fs.writeText', args);
}

export function exists(args: { path: string }): Promise<{ exists: boolean }> {
  assertVirtualPath(args.path);
  return KSM.invoke('fs.exists', args);
}

export function remove(args: { path: string }): Promise<{ removed: boolean }> {
  assertVirtualPath(args.path);
  return KSM.invoke('fs.remove', args);
}
