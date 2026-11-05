import { KSM } from '../bridge/ksm.js';
import {
  assertBoolean,
  assertBoundedString,
  assertVirtualStatPath
} from './guards.js';
import type {
  KSMTrayActionPayload,
  KSMTrayClickPayload,
  KSMTrayConfigureResult,
  KSMTrayMenuConfig,
  KSMTrayMenuItem
} from '../bridge/types.js';

export function show(): Promise<{ shown: boolean }> {
  return KSM.invoke('tray.show', {});
}

export function hide(): Promise<{ hidden: boolean }> {
  return KSM.invoke('tray.hide', {});
}

export function configure(
  args: KSMTrayMenuConfig
): Promise<KSMTrayConfigureResult> {
  if (args.icon !== undefined) {
    assertVirtualStatPath(args.icon);
  }
  if (args.tooltip !== undefined) {
    assertBoundedString(args.tooltip, 'tooltip', 256, true);
  }
  if (args.items !== undefined) {
    if (!Array.isArray(args.items) || args.items.length > 64) {
      throw new TypeError('items must contain at most 64 entries.');
    }
    args.items.forEach((item: KSMTrayMenuItem, index) => {
      assertBoundedString(item.id, `items[${index}].id`, 128);
      if (item.label !== undefined) {
        assertBoundedString(item.label, `items[${index}].label`, 256, true);
      }
      if (item.kind !== undefined) {
        assertBoundedString(item.kind, `items[${index}].kind`, 32);
      }
      if (item.enabled !== undefined) {
        assertBoolean(item.enabled, `items[${index}].enabled`);
      }
      if (item.checked !== undefined) {
        assertBoolean(item.checked, `items[${index}].checked`);
      }
      if (item.shortcut !== undefined) {
        assertBoundedString(item.shortcut, `items[${index}].shortcut`, 256);
      }
    });
  }
  return KSM.invoke('tray.configure', args);
}

export function onAction(
  listener: (payload: KSMTrayActionPayload) => void
): () => void {
  return KSM.on('tray.action', listener);
}

export function onClick(
  listener: (payload: KSMTrayClickPayload) => void
): () => void {
  return KSM.on('tray.click', listener);
}
