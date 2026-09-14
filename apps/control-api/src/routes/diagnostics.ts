import { FastifyInstance } from 'fastify';
import { listDiagnostics } from '../services/diagnosticsService.js';

export async function registerDiagnosticsRoutes(app: FastifyInstance): Promise<void> {
  app.get('/api/diagnostics', async () => ({
    items: await listDiagnostics(),
    timestamp: new Date().toISOString(),
  }));
}
