import {
  assertVirtualDirectoryPath,
  assertVirtualPath,
  assertVirtualStatPath
} from '../../../frontend/src/api/guards';
import { KSM } from '../../../frontend/src/bridge/ksm';

function expectRejected(path: string, writable = false): void {
  try {
    assertVirtualPath(path, writable);
  } catch (error) {
    if (error instanceof TypeError) {
      return;
    }
    throw error;
  }
  throw new Error(`Expected the path guard to reject ${path}.`);
}

function expectApiRejected(operation: () => unknown): void {
  try {
    operation();
  } catch (error) {
    if (error instanceof TypeError) {
      return;
    }
    throw error;
  }
  throw new Error('Expected the KSM file API to reject the path.');
}

function expectAccepted(operation: () => void): void {
  try {
    operation();
  } catch (error) {
    throw new Error(`Expected the path guard to accept the path: ${String(error)}`);
  }
}

export function runPathGuardCases(): void {
  expectRejected('app://data/C:/secret.txt');
  expectRejected('app://data/C:secret.txt');
  expectRejected('app://data/file.txt:stream');
  expectRejected('app://data/\\server\\share');
  expectRejected('app://data//server');
  expectRejected('app://data/./file.txt');
  expectRejected('app://data/nested//file.txt');
  expectRejected('app://data');
  expectRejected('app://picked/1/2');
  expectRejected('app://other/file.txt');
  expectRejected('file:///secret.txt');
  expectRejected('app://res/../data/secret.txt');
  expectRejected('app://assets/index.html', true);
  expectAccepted(() => assertVirtualDirectoryPath('app://data'));
  expectAccepted(() => assertVirtualDirectoryPath('app://data/', true));
  expectAccepted(() => assertVirtualStatPath('app://picked/1'));
  try {
    assertVirtualDirectoryPath('app://picked/1');
    throw new Error('Expected the directory guard to reject a picked file.');
  } catch (error) {
    if (!(error instanceof TypeError)) {
      throw error;
    }
  }

  expectApiRejected(() => KSM.fs.readText({ path: 'app://data/C:/secret.txt' }));
  expectApiRejected(() => KSM.fs.exists({ path: 'app://data/C:/secret.txt' }));
  expectApiRejected(() => KSM.fs.readDir({ path: 'app://picked/1' }));
  expectApiRejected(() => KSM.fs.writeText({
    path: 'app://assets/index.html',
    content: 'blocked'
  }));
}
