import assert from 'node:assert/strict';
import test from 'node:test';

const plugins = [
  ['dialog', ['confirm', 'message', 'openFile', 'saveFile']],
  ['fs', ['exists', 'mkdir', 'readDir', 'readText', 'remove', 'stat', 'writeText']],
  ['sql', ['close', 'drivers', 'execute', 'open', 'query']]
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
