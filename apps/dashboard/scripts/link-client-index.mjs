import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const dist = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../dist-client');
const from = path.join(dist, 'index.client.html');
const to = path.join(dist, 'index.html');
if (!fs.existsSync(from)) {
  console.error('[link-client-index] missing', from);
  process.exit(1);
}
fs.copyFileSync(from, to);
console.log('[link-client-index] linked dist-client/index.html <- index.client.html');
