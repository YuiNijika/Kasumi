import type { KSMPlatform } from '../../../frontend/src/bridge/types';
import { expectError, type ContractAdapter } from '../helpers';

export async function runBuiltInApiCases(
  adapter: ContractAdapter,
  platform: KSMPlatform
): Promise<void> {
  const resource = await adapter.invoke<string | { file: string }>(
    'fs.readText',
    { path: 'app://res/contract.txt' }
  );
  if (resource !== 'resource') {
    throw new Error('fs.readText did not read app://res.');
  }
  const asset = await adapter.invoke<string | { file: string }>(
    'fs.readText',
    { path: 'app://assets/index.html' }
  );
  if (asset !== '<!doctype html>') {
    throw new Error('fs.readText did not read app://assets.');
  }
  await expectError(
    adapter.invoke('fs.writeText', {
      path: 'app://res/contract.txt',
      content: 'blocked'
    }),
    'PERMISSION_DENIED'
  );

  const exists = await adapter.invoke<{ exists: boolean }>(
    'fs.exists',
    { path: 'app://data/contract.txt' }
  );
  if (!exists.exists) {
    throw new Error('fs.exists did not find the written file.');
  }
  const removed = await adapter.invoke<{ removed: boolean }>(
    'fs.remove',
    { path: 'app://data/contract.txt' }
  );
  if (!removed.removed) {
    throw new Error('fs.remove did not remove the written file.');
  }

  const created = await adapter.invoke<{ created: boolean }>(
    'fs.mkdir',
    { path: 'app://data/contract-dir', recursive: true }
  );
  if (!created.created) {
    throw new Error('fs.mkdir did not create a directory.');
  }
  const createdAgain = await adapter.invoke<{ created: boolean }>(
    'fs.mkdir',
    { path: 'app://data/contract-dir', recursive: true }
  );
  if (createdAgain.created) {
    throw new Error('fs.mkdir recreated an existing directory.');
  }
  await adapter.invoke('fs.writeText', {
    path: 'app://data/contract-dir/nested.txt',
    content: 'nested'
  });
  const directoryStat = await adapter.invoke<{
    kind: string;
    size: number;
  }>('fs.stat', { path: 'app://data/contract-dir' });
  if (directoryStat.kind !== 'directory' || directoryStat.size !== 0) {
    throw new Error('fs.stat did not return directory metadata.');
  }
  const fileStat = await adapter.invoke<{
    kind: string;
    size: number;
    modifiedAt: number | null;
  }>('fs.stat', { path: 'app://data/contract-dir/nested.txt' });
  if (
    fileStat.kind !== 'file' ||
    fileStat.size !== 6 ||
    !('modifiedAt' in fileStat)
  ) {
    throw new Error('fs.stat did not return file metadata.');
  }
  const listing = await adapter.invoke<{
    entries: Array<{ name: string; path: string; kind: string }>;
  }>('fs.readDir', { path: 'app://data' });
  const listedDirectory = listing.entries.find(
    (entry) => entry.name === 'contract-dir'
  );
  if (listedDirectory?.kind !== 'directory') {
    throw new Error('fs.readDir did not list the created directory.');
  }
  const nestedListing = await adapter.invoke<{
    entries: Array<{ name: string; kind: string }>;
  }>('fs.readDir', { path: 'app://data/contract-dir' });
  if (
    nestedListing.entries.length !== 1 ||
    nestedListing.entries[0]?.name !== 'nested.txt' ||
    nestedListing.entries[0]?.kind !== 'file'
  ) {
    throw new Error('fs.readDir did not list directory contents.');
  }
  const resourceStat = await adapter.invoke<{ kind: string }>(
    'fs.stat',
    { path: 'app://res/contract.txt' }
  );
  if (resourceStat.kind !== 'file') {
    throw new Error('fs.stat did not read bundled file metadata.');
  }
  const resourceListing = await adapter.invoke<{
    entries: Array<{ name: string }>;
  }>('fs.readDir', { path: 'app://res' });
  if (!resourceListing.entries.some((entry) => entry.name === 'contract.txt')) {
    throw new Error('fs.readDir did not list bundled resources.');
  }
  await adapter.invoke('fs.remove', {
    path: 'app://data/contract-dir/nested.txt'
  });
  await adapter.invoke('fs.remove', { path: 'app://data/contract-dir' });

  const message = await adapter.invoke<{ confirmed: boolean }>(
    'dialog.message',
    { title: 'Kasumi', message: 'contract' }
  );
  if (!message.confirmed) {
    throw new Error('dialog.message did not return confirmation.');
  }
  const opened = await adapter.invoke<{ file: string | null }>(
    'dialog.openFile',
    {}
  );
  if (!opened.file?.startsWith('app://picked/')) {
    throw new Error('dialog.openFile did not return a virtual handle.');
  }
  const saved = await adapter.invoke<{ file: string | null }>(
    'dialog.saveFile',
    { suggestedName: 'contract.txt' }
  );
  if (!saved.file?.startsWith('app://picked/')) {
    throw new Error('dialog.saveFile did not return a virtual handle.');
  }

  const clipboard = await adapter.invoke<{ text: string }>(
    'clipboard.readText',
    {}
  );
  if (typeof clipboard.text !== 'string') {
    throw new Error('clipboard.readText did not return text.');
  }
  const copied = await adapter.invoke<{ written: boolean }>(
    'clipboard.writeText',
    { text: 'contract' }
  );
  if (!copied.written) {
    throw new Error('clipboard.writeText failed.');
  }
  const notified = await adapter.invoke<{ shown: boolean }>(
    'notification.show',
    { title: 'Kasumi', body: 'contract' }
  );
  if (!notified.shown) {
    throw new Error('notification.show failed.');
  }
  const granted = await adapter.invoke<{ granted: boolean }>(
    'permission.request',
    { permission: 'clipboard.read' }
  );
  if (!granted.granted) {
    throw new Error('permission.request did not grant a registered permission.');
  }
  const downloaded = await adapter.invoke<{ path: string; bytes: number }>(
    'http.download',
    {
      url: 'https://example.com/contract.bin',
      path: 'app://data/download.bin'
    }
  );
  if (
    downloaded.path !== 'app://data/download.bin' ||
    downloaded.bytes !== 8
  ) {
    throw new Error('http.download returned an invalid result.');
  }
  await expectError(
    adapter.invoke('http.download', {
      url: 'https://example.com/contract.bin',
      path: 'app://data/download.bin'
    }),
    'IO_ERROR'
  );
  const overwritten = await adapter.invoke<{ bytes: number }>(
    'http.download',
    {
      url: 'https://example.com/contract.bin',
      path: 'app://data/download.bin',
      overwrite: true
    }
  );
  if (overwritten.bytes !== 8) {
    throw new Error('http.download did not allow an explicit overwrite.');
  }
  const external = await adapter.invoke<{ opened: boolean }>(
    'shell.openExternal',
    { url: 'https://example.com/docs' }
  );
  if (!external.opened) {
    throw new Error('shell.openExternal failed for an allowlisted origin.');
  }
  await expectError(
    adapter.invoke('shell.openExternal', {
      url: 'https://example.com.evil.test/docs'
    }),
    'PERMISSION_DENIED'
  );

  if (platform === 'android') {
    const device = await adapter.invoke<{
      platform: string;
      architecture: string;
    }>('device.getInfo', {});
    if (device.platform !== 'android' || !device.architecture) {
      throw new Error('device.getInfo returned invalid device data.');
    }
  } else {
    await expectError(adapter.invoke('device.getInfo', {}), 'NOT_SUPPORTED');
  }

  if (platform === 'windows') {
    const titled = await adapter.invoke<{ updated: boolean }>(
      'window.setTitle',
      { title: 'Kasumi' }
    );
    if (!titled.updated) {
      throw new Error('window.setTitle failed.');
    }
    const resized = await adapter.invoke<{ updated: boolean }>(
      'window.setSize',
      { width: 960, height: 720 }
    );
    if (!resized.updated) {
      throw new Error('window.setSize failed.');
    }
  } else {
    await expectError(
      adapter.invoke('window.setTitle', { title: 'Kasumi' }),
      'NOT_SUPPORTED'
    );
    await expectError(
      adapter.invoke('window.setSize', { width: 960, height: 720 }),
      'NOT_SUPPORTED'
    );
  }
  const closed = await adapter.invoke<{ closed: boolean }>(
    'window.close',
    {}
  );
  if (!closed.closed) {
    throw new Error('window.close failed.');
  }
  const app = await adapter.invoke<{
    name: string;
    platform: KSMPlatform;
  }>('app.getInfo', {});
  if (app.name !== 'Kasumi' || app.platform !== platform) {
    throw new Error('app.getInfo returned the wrong application identity.');
  }
  const exited = await adapter.invoke<{ exited: boolean }>('app.exit', {});
  if (!exited.exited) {
    throw new Error('app.exit failed.');
  }
}
