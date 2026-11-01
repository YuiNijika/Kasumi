import { readFile, readdir, writeFile } from 'node:fs/promises';
import path from 'node:path';

const LOCAL_FILE_HEADER = 0x04034b50;
const CENTRAL_DIRECTORY_HEADER = 0x02014b50;
const END_OF_CENTRAL_DIRECTORY = 0x06054b50;
const ZIP_VERSION = 20;

export async function createZipArchive(sourceDirectory, outputFile) {
  const entries = [];
  await collectFiles(sourceDirectory, sourceDirectory, entries);
  entries.sort((left, right) =>
    Buffer.compare(
      Buffer.from(left.name, 'utf8'),
      Buffer.from(right.name, 'utf8')
    )
  );

  const parts = [];
  const centralDirectory = [];
  let offset = 0;
  const timestamp = dosTimestamp(new Date());
  for (const entry of entries) {
    const name = Buffer.from(entry.name, 'utf8');
    const content = await readFile(entry.source);
    const checksum = crc32(content);
    const header = Buffer.alloc(30);
    header.writeUInt32LE(LOCAL_FILE_HEADER, 0);
    header.writeUInt16LE(ZIP_VERSION, 4);
    header.writeUInt16LE(0, 6);
    header.writeUInt16LE(0, 8);
    header.writeUInt16LE(timestamp.time, 10);
    header.writeUInt16LE(timestamp.date, 12);
    header.writeUInt32LE(checksum, 14);
    header.writeUInt32LE(content.length, 18);
    header.writeUInt32LE(content.length, 22);
    header.writeUInt16LE(name.length, 26);
    header.writeUInt16LE(0, 28);
    parts.push(header, name, content);

    const central = Buffer.alloc(46);
    central.writeUInt32LE(CENTRAL_DIRECTORY_HEADER, 0);
    central.writeUInt16LE(ZIP_VERSION, 4);
    central.writeUInt16LE(ZIP_VERSION, 6);
    central.writeUInt16LE(0, 8);
    central.writeUInt16LE(0, 10);
    central.writeUInt16LE(timestamp.time, 12);
    central.writeUInt16LE(timestamp.date, 14);
    central.writeUInt32LE(checksum, 16);
    central.writeUInt32LE(content.length, 20);
    central.writeUInt32LE(content.length, 24);
    central.writeUInt16LE(name.length, 28);
    central.writeUInt16LE(0, 30);
    central.writeUInt16LE(0, 32);
    central.writeUInt16LE(0, 34);
    central.writeUInt16LE(0, 36);
    central.writeUInt32LE(0, 38);
    central.writeUInt32LE(offset, 42);
    centralDirectory.push(central, name);
    offset += header.length + name.length + content.length;
  }

  const centralOffset = offset;
  let centralSize = 0;
  for (const part of centralDirectory) {
    parts.push(part);
    centralSize += part.length;
  }
  const end = Buffer.alloc(22);
  end.writeUInt32LE(END_OF_CENTRAL_DIRECTORY, 0);
  end.writeUInt16LE(0, 4);
  end.writeUInt16LE(0, 6);
  end.writeUInt16LE(entries.length, 8);
  end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(centralSize, 12);
  end.writeUInt32LE(centralOffset, 16);
  end.writeUInt16LE(0, 20);
  parts.push(end);
  await writeFile(outputFile, Buffer.concat(parts));
}

async function collectFiles(root, current, entries) {
  const children = await readdir(current, { withFileTypes: true });
  for (const child of children) {
    const source = path.join(current, child.name);
    if (child.isDirectory()) {
      await collectFiles(root, source, entries);
      continue;
    }
    if (!child.isFile()) {
      throw new Error(`Unsupported archive entry: ${source}`);
    }
    const relative = path.relative(root, source).split(path.sep).join('/');
    entries.push({ name: relative, source });
  }
}

function dosTimestamp(value) {
  const year = Math.max(value.getFullYear(), 1980);
  return {
    time:
      (value.getHours() << 11) |
      (value.getMinutes() << 5) |
      Math.floor(value.getSeconds() / 2),
    date:
      ((year - 1980) << 9) |
      ((value.getMonth() + 1) << 5) |
      value.getDate()
  };
}

function crc32(content) {
  let crc = 0xffffffff;
  for (const byte of content) {
    crc = (crc >>> 8) ^ CRC_TABLE[(crc ^ byte) & 0xff];
  }
  return (crc ^ 0xffffffff) >>> 0;
}

const CRC_TABLE = Array.from({ length: 256 }, (_, index) => {
  let value = index;
  for (let bit = 0; bit < 8; bit += 1) {
    value = (value & 1) !== 0
      ? 0xedb88320 ^ (value >>> 1)
      : value >>> 1;
  }
  return value >>> 0;
});
