import type { KSMErrorCode } from '../../frontend/src/bridge/types';

export interface ContractAdapter {
  invoke<TResult>(command: string, args: unknown): Promise<TResult>;
}

export async function expectError(
  operation: Promise<unknown>,
  code: KSMErrorCode
): Promise<void> {
  try {
    await operation;
  } catch (error) {
    if (error instanceof Error && error.name === code) {
      return;
    }
    throw error;
  }
  throw new Error(`Expected ${code} but the operation succeeded.`);
}
