import { KSM } from '../bridge/ksm.js';
import { assertJsonValue, assertStoreKey } from './guards.js';
import type {
  KSMJsonValue,
  KSMStoreGetResult
} from '../bridge/types.js';

export function get(args: { key: string }): Promise<KSMStoreGetResult> {
  assertStoreKey(args.key);
  return KSM.invoke('store.get', args);
}

export function set(args: {
  key: string;
  value: KSMJsonValue;
}): Promise<{ updated: true }> {
  assertStoreKey(args.key);
  assertJsonValue(args.value, 'value');
  return KSM.invoke('store.set', args);
}

export function remove(args: { key: string }): Promise<{ deleted: boolean }> {
  assertStoreKey(args.key);
  return KSM.invoke('store.delete', args);
}

export function clear(): Promise<{ cleared: true }> {
  return KSM.invoke('store.clear', {});
}

export function keys(): Promise<{ keys: string[] }> {
  return KSM.invoke('store.keys', {});
}
