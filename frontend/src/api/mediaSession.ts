import { KSM } from '../bridge/ksm.js';
import {
  assertBoundedString,
  assertNonNegativeNumber,
  assertVirtualStatPath
} from './guards.js';
import type {
  KSMMediaSessionCommandPayload,
  KSMMediaSessionMetadata,
  KSMMediaSessionPlaybackState
} from '../bridge/types.js';

const PLAYBACK_STATES = new Set<KSMMediaSessionPlaybackState>([
  'playing',
  'paused',
  'stopped',
  'buffering'
]);

function assertNullableString(value: unknown, field: string): void {
  if (value !== undefined && value !== null) {
    assertBoundedString(value, field, 8192, true);
  }
}

function assertNullableNumber(value: unknown, field: string): void {
  if (value !== undefined && value !== null) {
    assertNonNegativeNumber(value, field);
  }
}

export function update(
  args: KSMMediaSessionMetadata
): Promise<{ updated: boolean }> {
  assertBoundedString(args.title, 'title', 512);
  assertNullableString(args.artist, 'artist');
  assertNullableString(args.album, 'album');
  assertNullableNumber(args.durationMs, 'durationMs');
  assertNullableNumber(args.positionMs, 'positionMs');
  if (args.artwork !== undefined && args.artwork !== null) {
    assertVirtualStatPath(args.artwork);
  }
  if (
    typeof args.state !== 'string' ||
    !PLAYBACK_STATES.has(args.state)
  ) {
    throw new TypeError('state must be playing, paused, stopped or buffering.');
  }
  return KSM.invoke('mediaSession.update', args);
}

export function clear(): Promise<{ cleared: boolean }> {
  return KSM.invoke('mediaSession.clear', {});
}

export function onCommand(
  listener: (payload: KSMMediaSessionCommandPayload) => void
): () => void {
  return KSM.on('mediaSession.command', listener);
}
