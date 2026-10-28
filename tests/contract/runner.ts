import type { ContractAdapter } from './helpers';
import {
  KSM_ANDROID_COMMANDS,
  KSM_PERMISSIONS,
  KSM_WINDOWS_COMMANDS
} from '../../frontend/src/generated/contract';
import { runFsCases } from './cases/fs.readText.test';
import { runLifecycleCases } from './cases/bridge-lifecycle.test';
import { runMessageLimitCase } from './cases/message-limit.test';
import { runTimeoutCases } from './cases/timeout.test';
import { expectError } from './helpers';

export async function runContractSuite(adapter: ContractAdapter): Promise<void> {
  const ping = await adapter.invoke<{ pong: boolean }>('ping', {});
  if (!ping.pong) {
    throw new Error('ping did not return pong=true.');
  }

  const capabilities = await adapter.invoke<{
    platform: string;
    commands: string[];
    permissions: string[];
  }>(
    'capabilities.query',
    {}
  );
  if (
    capabilities.platform !== 'windows' &&
    capabilities.platform !== 'android'
  ) {
    throw new Error(`Unexpected platform: ${capabilities.platform}`);
  }
  const expectedCommands = capabilities.platform === 'windows'
    ? KSM_WINDOWS_COMMANDS
    : KSM_ANDROID_COMMANDS;
  for (const command of expectedCommands) {
    if (!capabilities.commands.includes(command)) {
      throw new Error(`capabilities.query did not include ${command}.`);
    }
  }
  for (const permission of KSM_PERMISSIONS) {
    if (!capabilities.permissions.includes(permission)) {
      throw new Error(`capabilities.query did not include ${permission}.`);
    }
  }
  await expectError(
    adapter.invoke('unknown.command', {}),
    'UNKNOWN_COMMAND'
  );
  await runFsCases(adapter);
  await runLifecycleCases(adapter);
  await runMessageLimitCase(adapter);
  await runTimeoutCases();
}
