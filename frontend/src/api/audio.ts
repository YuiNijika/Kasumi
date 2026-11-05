import { KSM } from '../bridge/ksm.js';
import {
  assertBoolean,
  assertBoundedString,
  assertHttpUrl,
  assertNonNegativeNumber,
  assertVirtualStatPath
} from './guards.js';
import type {
  KSMAudioEndedPayload,
  KSMAudioEqBand,
  KSMAudioEqSettings,
  KSMAudioErrorPayload,
  KSMAudioLoadArgs,
  KSMAudioProbeArgs,
  KSMAudioProbeResult,
  KSMAudioSeekArgs,
  KSMAudioState,
  KSMAudioStateChangedPayload,
  KSMAudioTickPayload,
  KSMAudioVolumeArgs,
  KSMAudioDevice
} from '../bridge/types.js';

const MAX_EQ_BANDS = 32;

function assertSource(value: unknown, field: string): asserts value is string {
  assertBoundedString(value, field, 8192);
  if (value.startsWith('https://')) {
    assertHttpUrl(value, field, true);
    return;
  }
  assertVirtualStatPath(value);
}

function assertEqBands(value: unknown, field: string): asserts value is KSMAudioEqBand[] {
  if (!Array.isArray(value) || value.length > MAX_EQ_BANDS) {
    throw new TypeError(`${field} must contain at most ${MAX_EQ_BANDS} bands.`);
  }
  for (const [index, band] of (value as unknown[]).entries()) {
    if (typeof band !== 'object' || band === null || Array.isArray(band)) {
      throw new TypeError(`${field}[${index}] must be an object.`);
    }
    const entry = band as Record<string, unknown>;
    assertNonNegativeNumber(entry.frequency, `${field}[${index}].frequency`, {
      allowZero: false
    });
    assertNonNegativeNumber(entry.gain, `${field}[${index}].gain`, {
      min: -24,
      max: 24
    });
  }
}

function assertEqSettings(
  value: KSMAudioEqSettings,
  field: string
): void {
  assertEqBands(value.bands, `${field}.bands`);
  if (value.preamp !== undefined) {
    assertNonNegativeNumber(value.preamp, `${field}.preamp`, {
      min: -24,
      max: 24
    });
  }
}

export function listDevices(): Promise<{ devices: KSMAudioDevice[] }> {
  return KSM.invoke('audio.devices.list', {});
}

export function setDevice(args: {
  deviceId: string;
}): Promise<{ updated: boolean }> {
  assertBoundedString(args.deviceId, 'deviceId', 256);
  return KSM.invoke('audio.devices.set', args);
}

export function getOutputMode(): Promise<{ mode: 'shared' | 'exclusive' }> {
  return KSM.invoke<{ mode: 'shared' | 'exclusive' }, Record<string, never>>(
    'audio.output.getMode',
    {}
  );
}

export function setExclusiveOutput(args: {
  enabled: boolean;
}): Promise<{ updated: boolean }> {
  assertBoolean(args.enabled, 'enabled');
  return KSM.invoke('audio.output.setExclusive', {
    exclusive: args.enabled
  });
}

export function setEq(args: KSMAudioEqSettings): Promise<{ updated: boolean }> {
  assertEqSettings(args, 'args');
  return KSM.invoke('audio.eq.set', {
    gains: args.bands.map((band) => band.gain)
  });
}

export function setSpeed(args: {
  speed: number;
}): Promise<{ updated: boolean }> {
  assertNonNegativeNumber(args.speed, 'speed', {
    min: 0.25,
    max: 4
  });
  return KSM.invoke('audio.speed.set', args);
}

export function probe(args: KSMAudioProbeArgs): Promise<KSMAudioProbeResult> {
  assertSource(args.source, 'source');
  return KSM.invoke('audio.probe', { path: args.source });
}

export function load(args: KSMAudioLoadArgs): Promise<{ loaded: boolean }> {
  assertSource(args.source, 'source');
  return KSM.invoke('audio.load', { path: args.source });
}

export function play(): Promise<{ updated: boolean }> {
  return KSM.invoke('audio.play', {});
}

export function pause(): Promise<{ updated: boolean }> {
  return KSM.invoke('audio.pause', {});
}

export function stop(): Promise<{ updated: boolean }> {
  return KSM.invoke('audio.stop', {});
}

export function seek(args: KSMAudioSeekArgs): Promise<{ updated: boolean }> {
  assertNonNegativeNumber(args.positionMs, 'positionMs');
  return KSM.invoke('audio.seek', args);
}

export function setVolume(args: KSMAudioVolumeArgs): Promise<{ updated: boolean }> {
  assertNonNegativeNumber(args.volume, 'volume', { max: 1 });
  if (args.muted !== undefined) {
    assertBoolean(args.muted, 'muted');
  }
  return KSM.invoke('audio.volume.set', { volume: args.volume });
}

export function getState(): Promise<KSMAudioState> {
  return KSM.invoke('audio.state.get', {});
}

export function onTick(
  listener: (payload: KSMAudioTickPayload) => void
): () => void {
  return KSM.on('audio.tick', listener);
}

export function onEnded(
  listener: (payload: KSMAudioEndedPayload) => void
): () => void {
  return KSM.on('audio.ended', listener);
}

export function onStateChanged(
  listener: (payload: KSMAudioStateChangedPayload) => void
): () => void {
  return KSM.on('audio.stateChanged', listener);
}

export function onError(
  listener: (payload: KSMAudioErrorPayload) => void
): () => void {
  return KSM.on('audio.error', listener);
}
