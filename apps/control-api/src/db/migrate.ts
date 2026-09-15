import { existsSync, readFileSync, readdirSync } from 'fs';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';
import { pool } from './pool.js';

const __dirname = dirname(fileURLToPath(import.meta.url));

function resolveMigrationsDir(): string {
  // Prefer SQL copied next to compiled migrate.js (dist/db/migrations).
  // Fall back to source tree when tsc-only builds forgot to copy *.sql.
  const candidates = [
    join(__dirname, 'migrations'),
    join(__dirname, '..', '..', 'src', 'db', 'migrations'),
    join(process.cwd(), 'src', 'db', 'migrations'),
    join(process.cwd(), 'apps', 'control-api', 'src', 'db', 'migrations'),
    join(process.cwd(), 'apps', 'control-api', 'dist', 'db', 'migrations'),
  ];
  for (const dir of candidates) {
    if (existsSync(dir)) return dir;
  }
  throw new Error(
    `Migrations directory not found. Tried:\n- ${candidates.join('\n- ')}\n` +
      'Run: npm run build --workspace=@vs-v2/control-api (copies src/db/migrations -> dist/db/migrations)'
  );
}

export async function runMigrations(): Promise<void> {
  const migrationsDir = resolveMigrationsDir();
  const files = readdirSync(migrationsDir)
    .filter((f) => f.endsWith('.sql'))
    .sort();

  if (files.length === 0) {
    throw new Error(`No .sql migration files in ${migrationsDir}`);
  }

  console.log(`Running migrations from ${migrationsDir} (${files.length} file(s))`);

  await pool.query(`
    CREATE TABLE IF NOT EXISTS schema_migrations (
      id SERIAL PRIMARY KEY,
      filename VARCHAR(255) UNIQUE NOT NULL,
      applied_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
    )
  `);

  for (const file of files) {
    const { rows } = await pool.query(
      'SELECT 1 FROM schema_migrations WHERE filename = $1',
      [file]
    );
    if (rows.length > 0) continue;

    const sql = readFileSync(join(migrationsDir, file), 'utf8');
    const client = await pool.connect();
    try {
      await client.query('BEGIN');
      await client.query(sql);
      await client.query(
        'INSERT INTO schema_migrations (filename) VALUES ($1)',
        [file]
      );
      await client.query('COMMIT');
      console.log(`Applied migration: ${file}`);
    } catch (err) {
      await client.query('ROLLBACK');
      throw err;
    } finally {
      client.release();
    }
  }
}

if (import.meta.url === `file://${process.argv[1]}`) {
  runMigrations()
    .then(() => {
      console.log('Migrations complete');
      return pool.end();
    })
    .catch((err) => {
      console.error('Migration failed:', err);
      process.exit(1);
    });
}
