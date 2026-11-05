import { KSM } from '../bridge/ksm.js';
import {
  assertBoolean,
  assertBoundedString,
  assertHttpHeaders,
  assertHttpUrl,
  assertJsonValue,
  assertNonNegativeNumber,
  assertTimeout,
  assertVirtualPath,
  assertVirtualStatPath
} from './guards.js';
import type {
  KSMHttpCookie,
  KSMHttpCookiesClearArgs,
  KSMHttpCookiesGetArgs,
  KSMHttpCookiesGetResult,
  KSMHttpCookiesSetArgs,
  KSMHttpDownloadResult,
  KSMHttpMethod,
  KSMHttpRequestArgs,
  KSMHttpRequestBody,
  KSMHttpResponse,
  KSMHttpResponseMode,
  KSMHttpTransferCancelArgs,
  KSMHttpTransferCancelResult,
  KSMHttpTransferCompletedPayload,
  KSMHttpTransferFailedPayload,
  KSMHttpTransferProgressPayload,
  KSMHttpTransferStartArgs,
  KSMHttpTransferStartResult
} from '../bridge/types.js';

const DOWNLOAD_TIMEOUT_MS = 120000;
const REQUEST_TIMEOUT_MS = 30000;
const RESPONSE_MODES = new Set<KSMHttpResponseMode>([
  'text',
  'json',
  'file',
  'empty'
]);
const HTTP_METHODS = new Set<KSMHttpMethod>(['GET', 'POST']);
const SAME_SITE_VALUES = new Set(['strict', 'lax', 'none']);

function assertMethod(value: unknown, field: string): asserts value is KSMHttpMethod {
  if (typeof value !== 'string' || !HTTP_METHODS.has(value as KSMHttpMethod)) {
    throw new TypeError(`${field} must be GET or POST.`);
  }
}

function assertResponseMode(
  value: unknown,
  field: string
): asserts value is KSMHttpResponseMode {
  if (
    typeof value !== 'string' ||
    !RESPONSE_MODES.has(value as KSMHttpResponseMode)
  ) {
    throw new TypeError(`${field} must be text, json, file or empty.`);
  }
}

function assertRequestBody(
  value: KSMHttpRequestBody | undefined,
  field: string
): void {
  if (value === undefined) {
    return;
  }
  if (typeof value !== 'object' || value === null || Array.isArray(value)) {
    throw new TypeError(`${field} must be an object.`);
  }
  switch (value.kind) {
    case 'empty':
      return;
    case 'text':
      assertBoundedString(value.text, `${field}.text`, 32768, true);
      return;
    case 'json':
      assertJsonValue(value.value, `${field}.value`);
      return;
    case 'file':
      assertVirtualStatPath(value.file);
      return;
    default:
      throw new TypeError(`${field}.kind is not supported.`);
  }
}

function assertCookie(value: unknown, field: string): asserts value is KSMHttpCookie {
  if (typeof value !== 'object' || value === null || Array.isArray(value)) {
    throw new TypeError(`${field} must be an object.`);
  }
  const cookie = value as Record<string, unknown>;
  assertBoundedString(cookie.name, `${field}.name`, 256);
  assertBoundedString(cookie.value, `${field}.value`, 8192, true);
  if (cookie.domain !== undefined) {
    assertBoundedString(cookie.domain, `${field}.domain`, 256);
  }
  if (cookie.path !== undefined) {
    assertBoundedString(cookie.path, `${field}.path`, 1024, true);
  }
  if (cookie.expiresAt !== undefined && cookie.expiresAt !== null) {
    assertNonNegativeNumber(cookie.expiresAt, `${field}.expiresAt`);
  }
  if (cookie.secure !== undefined) {
    assertBoolean(cookie.secure, `${field}.secure`);
  }
  if (cookie.httpOnly !== undefined) {
    assertBoolean(cookie.httpOnly, `${field}.httpOnly`);
  }
  if (cookie.sameSite !== undefined) {
    if (
      typeof cookie.sameSite !== 'string' ||
      !SAME_SITE_VALUES.has(cookie.sameSite)
    ) {
      throw new TypeError(`${field}.sameSite must be strict, lax or none.`);
    }
  }
}

function assertCookies(value: unknown, field: string): asserts value is KSMHttpCookie[] {
  if (!Array.isArray(value) || value.length > 64) {
    throw new TypeError(`${field} must contain at most 64 cookies.`);
  }
  (value as unknown[]).forEach((cookie, index) => {
    assertCookie(cookie, `${field}[${index}]`);
  });
}

export function download(args: {
  url: string;
  path: string;
  overwrite?: boolean;
}): Promise<KSMHttpDownloadResult> {
  assertHttpUrl(args.url, 'url', true);
  assertVirtualPath(args.path, true, false);
  if (args.overwrite !== undefined) {
    assertBoolean(args.overwrite, 'overwrite');
  }
  return KSM.invoke('http.download', args, DOWNLOAD_TIMEOUT_MS);
}

export function request(args: KSMHttpRequestArgs): Promise<KSMHttpResponse> {
  assertHttpUrl(args.url, 'url', true);
  if (args.method !== undefined) {
    assertMethod(args.method, 'method');
  }
  if (args.headers !== undefined) {
    assertHttpHeaders(args.headers, 'headers');
  }
  assertRequestBody(args.body, 'body');
  if (args.response !== undefined) {
    assertResponseMode(args.response, 'response');
  }
  if (args.timeoutMs !== undefined) {
    assertTimeout(args.timeoutMs, 'timeoutMs');
  }
  return KSM.invoke<KSMHttpResponse, KSMHttpRequestArgs>(
    'http.request',
    args,
    args.timeoutMs ?? REQUEST_TIMEOUT_MS
  );
}

export function startTransfer(
  args: KSMHttpTransferStartArgs
): Promise<KSMHttpTransferStartResult> {
  assertHttpUrl(args.url, 'url', true);
  if (args.method !== undefined) {
    assertMethod(args.method, 'method');
  }
  if (args.headers !== undefined) {
    assertHttpHeaders(args.headers, 'headers');
  }
  assertRequestBody(args.body, 'body');
  if (args.path !== undefined) {
    assertVirtualPath(args.path, true, false);
  }
  if (args.overwrite !== undefined) {
    assertBoolean(args.overwrite, 'overwrite');
  }
  if (args.timeoutMs !== undefined) {
    assertTimeout(args.timeoutMs, 'timeoutMs');
  }
  return KSM.invoke<KSMHttpTransferStartResult, KSMHttpTransferStartArgs>(
    'http.transfer.start',
    args,
    REQUEST_TIMEOUT_MS
  );
}

export function cancelTransfer(
  args: KSMHttpTransferCancelArgs
): Promise<KSMHttpTransferCancelResult> {
  assertBoundedString(args.transferId, 'transferId', 128);
  return KSM.invoke<KSMHttpTransferCancelResult, KSMHttpTransferCancelArgs>(
    'http.transfer.cancel',
    args
  );
}

export function getCookies(
  args: KSMHttpCookiesGetArgs
): Promise<KSMHttpCookiesGetResult> {
  assertHttpUrl(args.url, 'url', true);
  return KSM.invoke<KSMHttpCookiesGetResult, KSMHttpCookiesGetArgs>(
    'http.cookies.get',
    args
  );
}

export function setCookies(
  args: KSMHttpCookiesSetArgs
): Promise<{ updated: boolean }> {
  assertHttpUrl(args.url, 'url', true);
  assertCookies(args.cookies, 'cookies');
  return KSM.invoke<{ updated: boolean }, KSMHttpCookiesSetArgs>(
    'http.cookies.set',
    args
  );
}

export function clearCookies(
  args: KSMHttpCookiesClearArgs
): Promise<{ cleared: boolean }> {
  assertHttpUrl(args.url, 'url', true);
  return KSM.invoke<{ cleared: boolean }, KSMHttpCookiesClearArgs>(
    'http.cookies.clear',
    args
  );
}

export function onTransferProgress(
  listener: (payload: KSMHttpTransferProgressPayload) => void
): () => void {
  return KSM.on('http.transfer.progress', listener);
}

export function onTransferCompleted(
  listener: (payload: KSMHttpTransferCompletedPayload) => void
): () => void {
  return KSM.on('http.transfer.completed', listener);
}

export function onTransferFailed(
  listener: (payload: KSMHttpTransferFailedPayload) => void
): () => void {
  return KSM.on('http.transfer.failed', listener);
}
