export {
  show,
  hide,
  configure,
  onAction,
  onClick
} from 'kasumi-bridge/tray';
export type {
  KSMTrayActionPayload,
  KSMTrayApi,
  KSMTrayMenuConfig
} from 'kasumi-bridge';

export const PLUGIN_ID = 'tray' as const;
