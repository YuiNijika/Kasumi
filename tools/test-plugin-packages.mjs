import assert from 'node:assert/strict';
import test from 'node:test';

const plugins = [
  ['clipboard', ['readText', 'writeText']],
  ['device', ['getInfo']],
  ['dialog', ['confirm', 'message', 'openFile', 'saveFile']],
  ['fs', ['exists', 'mkdir', 'readDir', 'readText', 'remove', 'stat', 'writeText']],
  ['http', ['download']],
  ['notification', ['show']],
  ['shell', ['openExternal']],
  ['window', ['close', 'maximize', 'minimize', 'setSize', 'setTitle']],
  ['sql', ['close', 'drivers', 'execute', 'open', 'query']],
  ['path', ['basename', 'dirname', 'dirs', 'extname', 'isAbsolute', 'join', 'normalize']],
  ['store', ['clear', 'get', 'keys', 'remove', 'set']],
  ['os', ['getInfo']],
  ['log', ['write']]
];

for (const [id, exports] of plugins) {
  test(`kasumi-plugin-${id} exposes its plugin API`, async () => {
    const module = await import(`../packages/kasumi-plugin-${id}/dist/index.js`);
    assert.equal(module.PLUGIN_ID, id);
    for (const name of exports) {
      assert.equal(typeof module[name], 'function', `${name} is not exported`);
    }
  });
}
