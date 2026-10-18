import assert from 'node:assert/strict';
import { readFile, stat } from 'node:fs/promises';
import { execFileSync } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const directory = path.dirname(fileURLToPath(import.meta.url));
const root = path.join(directory, 'dist');
const html = await readFile(path.join(root, 'index.html'), 'utf8');
const ids = [...html.matchAll(/\bid="([^"]+)"/g)].map(match => match[1]);
assert.equal(new Set(ids).size, ids.length, 'HTML IDs must be unique');
let localReferences = 0;
for (const [, reference] of html.matchAll(/\b(?:src|href)="([^"]+)"/g)) {
  if (/^https?:/.test(reference)) continue;
  if (reference.startsWith('#')) {
    if (reference.length > 1) assert(ids.includes(reference.slice(1)), `Missing anchor: ${reference}`);
    continue;
  }
  const file = path.resolve(root, reference);
  assert(file.startsWith(root + path.sep), 'Local assets must stay inside dist');
  assert((await stat(file)).isFile(), `Missing asset: ${reference}`);
  localReferences++;
}
for (const name of ['steps', 'piano', 'mixer']) {
  const image = await readFile(path.join(root, 'assets', `studio-${name}.png`));
  assert.equal(image.subarray(1, 4).toString(), 'PNG');
  assert(image.readUInt32BE(16) >= 960 && image.readUInt32BE(20) >= 640, 'Use native application captures');
}
const wav = await readFile(path.join(root, 'assets', 'after-hours.wav'));
assert.equal(wav.toString('ascii', 0, 4), 'RIFF');
assert.equal(wav.toString('ascii', 8, 12), 'WAVE');
assert.equal(wav.readUInt16LE(20), 1, 'Demo must be PCM');
assert.equal(wav.readUInt16LE(22), 1, 'Web demo must be mono');
assert.equal(wav.readUInt32LE(24), 48000);
assert.equal(wav.readUInt16LE(34), 16);
assert.equal(wav.readUInt32LE(40), wav.length - 44);
const duration = (wav.length - 44) / 2 / 48000;
assert(duration > 8 && duration < 9, 'Native demo duration must match the page');
for (const file of ['dist/app.js', 'server.mjs', 'check.mjs'])
  execFileSync(process.execPath, ['--check', path.join(directory, file)], { stdio: 'pipe' });
process.stdout.write(`PASS: ${localReferences} local references, 3 native captures, ${duration.toFixed(2)}s PCM demo, JavaScript syntax\n`);
