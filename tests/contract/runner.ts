import type { ContractAdapter } from './helpers';
import {
  KSM_ANDROID_COMMANDS,
  KSM_PERMISSIONS,
  KSM_PLUGINS,
  KSM_PLUGIN_METADATA,
  KSM_PLUGIN_COMMANDS,
  KSM_WINDOWS_COMMANDS
} from '../../frontend/src/generated/contract';
import { runFsCases } from './cases/fs.readText.test';
import { runLifecycleCases } from './cases/bridge-lifecycle.test';
import { runMessageLimitCase } from './cases/message-limit.test';
import { runPathGuardCases } from './cases/path-guard.test';
import { runTimeoutCases } from './cases/timeout.test';
import { runBuiltInApiCases } from './cases/built-in-api.test';
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
    plugins: string[];
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
  for (const plugin of KSM_PLUGINS) {
    if (!capabilities.plugins.includes(plugin)) {
      throw new Error(`capabilities.query did not include plugin ${plugin}.`);
    }
    const metadata = KSM_PLUGIN_METADATA[plugin];
    if (!metadata) {
      throw new Error(`Generated metadata is missing plugin ${plugin}.`);
    }
    if (
      metadata.commands.join('\n') !== KSM_PLUGIN_COMMANDS[plugin]?.join('\n')
    ) {
      throw new Error(`Plugin ${plugin} metadata commands drifted.`);
    }
    for (const permission of metadata.defaultPermissions) {
      if (!metadata.permissions.includes(permission)) {
        throw new Error(
          `Plugin ${plugin} default permission ${permission} is undeclared.`
        );
      }
    }
    for (const command of KSM_PLUGIN_COMMANDS[plugin] ?? []) {
      if (
        (expectedCommands as readonly string[]).includes(command) &&
        !capabilities.commands.includes(command)
      ) {
        throw new Error(`Plugin ${plugin} command ${command} is missing.`);
      }
    }
  }
  await expectError(
    adapter.invoke('unknown.command', {}),
    'UNKNOWN_COMMAND'
  );
  runPathGuardCases();
  await runFsCases(adapter);
  await runBuiltInApiCases(adapter, capabilities.platform);
  await runLifecycleCases(adapter);
  await runMessageLimitCase(adapter);
  await runTimeoutCases();
}
