import assert from 'node:assert/strict';
import test from 'node:test';

const plugins = [
  ['clipboard', ['readText', 'writeText']],
  ['device', ['getInfo']],
  ['dialog', ['confirm', 'message', 'openFile', 'saveFile']],
  ['fs', ['exists', 'mkdir', 'readDir', 'readText', 'remove', 'stat', 'writeText']],
  ['fs', ['appendText', 'copy', 'move', 'readBytes', 'writeBytes', 'walk', 'releaseScope']],
  ['dialog', ['openFolder', 'openFiles']],
  ['http', ['download', 'request', 'startTransfer', 'cancelTransfer', 'getCookies']],
  ['notification', ['show']],
  ['shell', ['openExternal', 'openPath', 'showInFolder']],
  ['updater', ['check', 'download', 'install', 'onProgress', 'onStatus']],
  ['global-shortcut', 'globalShortcut', ['register', 'unregister', 'onTriggered']],
  ['tray', ['show', 'hide', 'configure', 'onAction', 'onClick']],
  ['window', [
    'close',
    'create',
    'emit',
    'getState',
    'hide',
    'maximize',
    'minimize',
    'setAlwaysOnTop',
    'setPosition',
    'setSize',
    'setTitle',
    'setTransparent',
    'show'
  ]],
  ['audio', ['load', 'play', 'pause', 'stop', 'onTick']],
  ['media-session', 'mediaSession', ['update', 'clear', 'onCommand']],
  ['sql', ['close', 'drivers', 'execute', 'open', 'query']],
  ['path', ['basename', 'dirname', 'dirs', 'extname', 'isAbsolute', 'join', 'normalize']],
  ['store', ['clear', 'get', 'keys', 'remove', 'set']],
  ['os', ['getInfo']],
  ['log', ['write']]
];

for (const entry of plugins) {
  const [dir, pluginId, exports] =
    entry.length === 3 ? entry : [entry[0], entry[0], entry[1]];
  test(`kasumi-plugin-${dir} exposes its plugin API`, async () => {
    const module = await import(`../packages/kasumi-plugin-${dir}/dist/index.js`);
    assert.equal(module.PLUGIN_ID, pluginId);
    for (const name of exports) {
      assert.equal(typeof module[name], 'function', `${name} is not exported`);
    }
  });
}
