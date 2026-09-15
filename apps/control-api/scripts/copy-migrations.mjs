import { cpSync, mkdirSync, existsSync, readdirSync } from 'fs';
import { dirname, join } from 'path';
import { fileURLToPath } from 'url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const src = join(root, 'src', 'db', 'migrations');
const dest = join(root, 'dist', 'db', 'migrations');

if (!existsSync(src)) {
  console.error(`Missing migrations source: ${src}`);
  process.exit(1);
}

mkdirSync(dirname(dest), { recursive: true });
cpSync(src, dest, { recursive: true });

const files = readdirSync(dest).filter((f) => f.endsWith('.sql'));
console.log(`Copied ${files.length} migration(s) -> ${dest}`);
