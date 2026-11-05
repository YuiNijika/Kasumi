export {
  register,
  unregister,
  onTriggered
} from 'kasumi-bridge/globalShortcut';
export type {
  KSMGlobalShortcutApi,
  KSMGlobalShortcutTriggeredPayload
} from 'kasumi-bridge';

export const PLUGIN_ID = 'globalShortcut' as const;
