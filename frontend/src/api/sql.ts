import { KSM } from '../bridge/ksm.js';
import {
  assertSqlConnectionId,
  assertSqlDriver,
  assertSqlParams,
  assertSqlStatement
} from './guards.js';
import type {
  KSMSqlDriver,
  KSMSqlDriversResult,
  KSMSqlExecuteResult,
  KSMSqlOpenResult,
  KSMSqlQueryResult,
  KSMSqlValue
} from '../bridge/types.js';

export function drivers(): Promise<KSMSqlDriversResult> {
  return KSM.invoke('sql.drivers', {});
}

export function open(args: {
  driver: KSMSqlDriver;
  url: string;
}): Promise<KSMSqlOpenResult> {
  assertSqlDriver(args.driver);
  if (typeof args.url !== 'string' || args.url.length === 0) {
    throw new TypeError('url must be a non-empty string.');
  }
  if (args.url.length > 4096) {
    throw new TypeError('url must not exceed 4096 characters.');
  }
  return KSM.invoke('sql.open', args);
}

export function query(args: {
  connectionId: string;
  statement: string;
  params?: KSMSqlValue[];
}): Promise<KSMSqlQueryResult> {
  assertSqlConnectionId(args.connectionId);
  assertSqlStatement(args.statement);
  if (args.params !== undefined) {
    assertSqlParams(args.params);
  }
  return KSM.invoke('sql.query', args);
}

export function execute(args: {
  connectionId: string;
  statement: string;
  params?: KSMSqlValue[];
}): Promise<KSMSqlExecuteResult> {
  assertSqlConnectionId(args.connectionId);
  assertSqlStatement(args.statement);
  if (args.params !== undefined) {
    assertSqlParams(args.params);
  }
  return KSM.invoke('sql.execute', args);
}

export function close(args: {
  connectionId: string;
}): Promise<{ closed: boolean }> {
  assertSqlConnectionId(args.connectionId);
  return KSM.invoke('sql.close', args);
}
