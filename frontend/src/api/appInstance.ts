import { KSM } from '../bridge/ksm.js';

export function state(): Promise<{ primary: boolean }> {
  return KSM.invoke('app.singleInstance.state', {});
}

export function focus(): Promise<{ focused: boolean }> {
  return KSM.invoke('app.singleInstance.focus', {});
}
