import assert from 'node:assert/strict';
import test from 'node:test';
import {
  createWindowsMsiSource,
  stableWindowsUpgradeCode
} from '../src/windows.mjs';

const config = {
  packageName: 'music-storm',
  version: '26.11.15'
};

test('generates a valid, stable Windows MSI upgrade code', () => {
  const first = stableWindowsUpgradeCode('music-storm');
  const second = stableWindowsUpgradeCode('music-storm');
  const other = stableWindowsUpgradeCode('other-app');

  assert.match(
    first,
    /^[0-9A-F]{8}-[0-9A-F]{4}-5[0-9A-F]{3}-[89AB][0-9A-F]{3}-[0-9A-F]{12}$/
  );
  assert.equal(first, second);
  assert.notEqual(first, other);
  assert.notEqual(first, '00000000-0000-0000-0000-000000000000');
});

test('generates WiX source with media, per-machine install and major upgrade', () => {
  const xml = createWindowsMsiSource(
    config,
    'C:\\build\\music-storm.exe',
    'C:\\app\\icons\\installer.ico'
  );

  assert.match(xml, /<Media Id="1" Cabinet="product\.cab" EmbedCab="yes" \/>/);
  assert.match(xml, /InstallScope="perMachine"/);
  assert.match(xml, /<MajorUpgrade DowngradeErrorMessage="[^"]+" \/>/);
  assert.match(xml, /<Component Id="MainExecutable" Guid="\*" Win64="yes">/);
  assert.match(xml, /<File Id="MainExe" Source="C:\\build\\music-storm\.exe" KeyPath="yes" \/>/);
  assert.match(xml, /UpgradeCode="[0-9A-F-]{36}"/);
});

test('escapes XML-sensitive installer metadata', () => {
  const xml = createWindowsMsiSource(
    {
      packageName: 'music&storm',
      version: '26.11.15'
    },
    'C:\\build\\a&b.exe',
    undefined
  );

  assert.match(xml, /Name="music&amp;storm"/);
  assert.match(xml, /Source="C:\\build\\a&amp;b\.exe"/);
});
