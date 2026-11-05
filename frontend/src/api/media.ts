import { KSM } from '../bridge/ksm.js';
import {
  assertString,
  assertVirtualStatPath
} from './guards.js';
import type {
  KSMFfmpegDetection,
  KSMFfmpegSetPathArgs,
  KSMFfmpegValidateResult,
  KSMMediaMetadata,
  KSMMediaProbeArgs
} from '../bridge/types.js';

export function probe(args: KSMMediaProbeArgs): Promise<KSMMediaMetadata> {
  assertVirtualStatPath(args.path);
  return KSM.invoke('media.probe', args);
}

export function detectFfmpeg(): Promise<KSMFfmpegDetection> {
  return KSM.invoke('media.ffmpeg.detect', {});
}

export function validateFfmpeg(): Promise<KSMFfmpegValidateResult> {
  return KSM.invoke('media.ffmpeg.validate', {});
}

export function setFfmpegPath(
  args: KSMFfmpegSetPathArgs
): Promise<{ updated: boolean }> {
  assertString(args.path, 'path');
  return KSM.invoke('media.ffmpeg.setPath', args);
}

export function pickFfmpegExecutable(): Promise<{ path: string | null }> {
  return KSM.invoke('media.ffmpeg.pickExecutable', {}, 60000);
}
